/* Getting an equation / inequality ready to solve, one GCSE line at a time:
 * clear fractions (multiply by the LCM of the denominators), expand brackets,
 * collect like terms, and bring everything to one side. */
#include "solve_int.h"
#include "arena.h"
#include <string.h>

#define MAXT 48

typedef struct { const Expr *t; int sign; Poly p; } Term;

static int split(const Expr *e, int sign, Term *out, int n)
{
    if (n < 0) return n;
    if (e->kind == E_ADD) {
        for (int i = 0; i < e->nkids; i++) n = split(e->kids[i], sign, out, n);
        return n;
    }
    if (e->kind == E_NEG && e->kids[0]->kind != E_ADD) return split(e->kids[0], -sign, out, n);
    if (e->kind == E_NEG) { e = e->kids[0]; sign = -sign; }
    if (n >= MAXT) return -1;
    out[n].t = e;
    out[n].sign = sign;
    return n + 1;
}

static bool has_add(const Expr *e)
{
    if (e->kind == E_ADD) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_add(e->kids[i])) return true;
    return false;
}

static Expr *neg_disp(Expr *d)
{
    if (d->kind == E_NUM) return ex_num(rat_neg(d->r));
    if (d->kind == E_MUL && d->nkids > 0 && d->kids[0]->kind == E_NUM) {
        Expr *n = ex_node(E_MUL, d->nkids, d->kids);
        n->kids[0] = ex_num(rat_neg(d->kids[0]->r));
        if (rat_is_one(n->kids[0]->r)) return n->nkids == 2 ? n->kids[1] : ex_node(E_MUL, n->nkids - 1, n->kids + 1);
        return n;
    }
    if (d->kind == E_NEG) return d->kids[0];
    return ex_neg(d);
}

/* join display pieces into one sum, splicing nested sums */
static Expr *join(Expr **d, int n)
{
    Expr *k[MAXT * 4];
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (d[i]->kind == E_ADD && !(d[i]->flags & EF_PAREN)) {
            for (int j = 0; j < d[i]->nkids && m < MAXT * 4; j++) k[m++] = d[i]->kids[j];
        } else if (ex_is_zero(d[i])) {
            continue;   /* a term like 0x^2 */
        } else if (m < MAXT * 4) k[m++] = d[i];
    }
    if (m == 0) return ex_int(0);
    return m == 1 ? k[0] : ex_node(E_ADD, m, k);
}

/* term multiplied by m, keeping a bracket when the term was (bracket)/k or k(bracket) */
static Expr *scaled(const Term *t, int64_t m, const char *var, bool *bracket)
{
    const Expr *e = t->t;
    Rat f;
    if (e->kind == E_DIV && e->kids[1]->kind == E_NUM && rat_is_int(e->kids[1]->r) &&
        e->kids[1]->r.n > 0 && m % e->kids[1]->r.n == 0 && e->kids[0]->kind == E_ADD) {
        int64_t q = m / e->kids[1]->r.n;
        Expr *in = ex_clone(e->kids[0]);
        Expr *d = q == 1 ? in : ex_mul2(ex_int(q), in);
        if (q == 1 && t->sign < 0) return ex_neg(in);
        *bracket = true;
        return t->sign < 0 ? neg_disp(d) : d;
    }
    if (e->kind == E_MUL && e->nkids == 2 && e->kids[0]->kind == E_NUM && e->kids[1]->kind == E_ADD &&
        rat_mul(e->kids[0]->r, rat_int(m * t->sign), &f) && rat_is_int(f)) {
        *bracket = true;
        return ex_mul2(ex_num(f), ex_clone(e->kids[1]));
    }
    Poly q;
    if (!poly_scale(&t->p, rat_int(m), &q)) q = t->p;
    return sv_pexpr(&q, var);
}

static bool side_terms(const Expr *e, Term *t, int *n)
{
    *n = split(e, 1, t, 0);
    if (*n < 0) return false;
    for (int i = 0; i < *n; i++) {
        Poly p;
        if (!poly_from_expr(t[i].t, &p)) return false;
        if (t[i].sign < 0 && !poly_neg(&p, &p)) return false;
        t[i].p = p;
    }
    return true;
}

static bool sum_terms(const Term *t, int n, Poly *out)
{
    Poly s = poly_zero();
    for (int i = 0; i < n; i++)
        if (!poly_add(&s, &t[i].p, &s)) return false;
    *out = s;
    return true;
}

static const char *show(int op, Expr *l, Expr *r)
{
    return ex_print(ex_rel(op, l, r));
}

bool sv_prepare(SvCtx *c, int op, const Expr *L, const Expr *R, Poly *PL, Poly *PR)
{
    Term tl[MAXT], tr[MAXT];
    int nl, nr;
    if (!side_terms(L, tl, &nl) || !side_terms(R, tr, &nr)) return false;
    const char *last = show(op, (Expr *)L, (Expr *)R);
    bool bracket = false;
    for (int i = 0; i < nl; i++) bracket |= has_add(tl[i].t);
    for (int i = 0; i < nr; i++) bracket |= has_add(tr[i].t);

    int64_t m = 1;
    for (int i = 0; i < nl + nr; i++) {
        int64_t k;
        const Term *t = i < nl ? &tl[i] : &tr[i - nl];
        if (!sv_lcm_dens(&t->p, &k) || !i64_lcm(m, k, &m)) { m = 1; break; }
    }
    Expr *dl[MAXT], *dr[MAXT];
    if (m > 1) {
        bool br = false;
        for (int i = 0; i < nl; i++) dl[i] = scaled(&tl[i], m, c->var, &br);
        for (int i = 0; i < nr; i++) dr[i] = scaled(&tr[i], m, c->var, &br);
        bool okm = true;
        for (int i = 0; i < nl && okm; i++) okm = poly_scale(&tl[i].p, rat_int(m), &tl[i].p);
        for (int i = 0; i < nr && okm; i++) okm = poly_scale(&tr[i].p, rat_int(m), &tr[i].p);
        if (!okm) return false;
        const char *mm = sv_fmt("%lld", (long long)m);
        last = show(op, join(dl, nl), join(dr, nr));
        sv_step(c, sv_fmt("Multiply both sides by %s", mm),
                sv_has_decimal(L) || sv_has_decimal(R)
                    ? sv_fmt("Multiplying every term by %s clears the decimals.", mm)
                    : sv_fmt("%s is the lowest common multiple of the denominators.", mm),
                last);
        bracket = br;
    }
    for (int i = 0; i < nl; i++) dl[i] = sv_pexpr(&tl[i].p, c->var);
    for (int i = 0; i < nr; i++) dr[i] = sv_pexpr(&tr[i].p, c->var);
    const char *ex = show(op, join(dl, nl), join(dr, nr));
    if (strcmp(ex, last) != 0) {
        sv_step(c, bracket ? "Expand the brackets" : "Simplify", NULL, ex);
        last = ex;
    }
    if (!sum_terms(tl, nl, PL) || !sum_terms(tr, nr, PR)) return false;
    const char *co = show(op, sv_pexpr(PL, c->var), sv_pexpr(PR, c->var));
    /* only when terms really combine (a pure reorder like 5-2x -> -2x+5 is not shown) */
    int shown = 0;
    for (int i = 0; i < nl; i++) shown += dl[i]->kind == E_ADD ? dl[i]->nkids : 1;
    for (int i = 0; i < nr; i++) shown += dr[i]->kind == E_ADD ? dr[i]->nkids : 1;
    int left = (PL->n ? PL->n : 1) + (PR->n ? PR->n : 1);
    if (strcmp(co, last) != 0 && left < shown) sv_step(c, "Collect like terms", NULL, co);
    return true;
}

bool sv_to_zero(SvCtx *c, int *op, const Poly *PL, const Poly *PR, Poly *P)
{
    Poly p;
    Rat lc[SV_MAXDEG + 1];
    int deg;
    if (!poly_sub(PL, PR, &p)) return false;
    bool flipped = false;
    if (sv_upoly(&p, c->var, lc, &deg) && rat_is_neg(lc[deg])) {
        if (!poly_neg(&p, &p)) return false;
        *op = sv_flip(*op);
        flipped = true;
    }
    if (!poly_is_zero(PR) || flipped)
        sv_step(c, "Rearrange so one side is 0",
                flipped ? "Move every term to the other side so the highest power has a positive coefficient." : NULL,
                show(*op, sv_pexpr(&p, c->var), ex_int(0)));
    Rat ct, inv;
    if (poly_content(&p, &ct) && !rat_is_one(ct) && !rat_is_zero(ct) && rat_inv(ct, &inv)) {
        Poly q;
        if (poly_scale(&p, inv, &q)) {
            p = q;
            const char *t = rat_is_int(ct) ? sv_fmt("Divide both sides by %s", sv_rat_str(ct))
                                           : sv_fmt("Multiply both sides by %s", sv_rat_str(inv));
            sv_step(c, t, NULL, show(*op, sv_pexpr(&p, c->var), ex_int(0)));
        }
    }
    *P = p;
    return true;
}
