/* Simultaneous linear equations (2x2, 3x3) by elimination, GCSE-style:
 * number the equations, scale, add/subtract to eliminate, solve, substitute back,
 * check. Linear + quadratic pairs go to solve_simq.c. */
#include "solve_int.h"
#include <stdio.h>
#include <string.h>

#define NV 3

typedef struct { Rat a[NV]; Rat k; int label; } Eq;

typedef struct {
    SvCtx *c;
    int nv;
    char (*vars)[12];
    int next;
} Sys;

static Expr *eq_expr(const Sys *S, const Eq *e)
{
    Poly p = poly_zero();
    for (int i = 0; i < S->nv; i++) {
        Poly t = poly_var(S->vars[i]), u;
        if (!poly_scale(&t, e->a[i], &u) || !poly_add(&p, &u, &p)) return ex_text("?");
    }
    return ex_rel(R_EQ, sv_pexpr(&p, NULL), ex_num(e->k));
}

static const char *eq_str(const Sys *S, const Eq *e) { return ex_print(eq_expr(S, e)); }

static bool scale(Eq *e, Rat m)
{
    for (int i = 0; i < NV; i++)
        if (!rat_mul(e->a[i], m, &e->a[i])) return false;
    return rat_mul(e->k, m, &e->k);
}

static Rat rabs(Rat r) { return rat_abs(r); }

/* sum of known terms shown as numbers, unknown var kept: 2×3+3y=12 */
static Expr *subst_expr(const Sys *S, const Eq *e, const bool *known, const Rat *val)
{
    Expr *k[NV];
    int n = 0;
    for (int i = 0; i < S->nv; i++) {
        if (rat_is_zero(e->a[i])) continue;
        Expr *x = known[i] ? ex_num(val[i]) : ex_var(S->vars[i]);
        if (rat_is_one(e->a[i])) k[n++] = x;
        else if (rat_eq(e->a[i], rat_int(-1)) && !(known[i] && rat_is_neg(val[i]))) k[n++] = ex_neg(x);
        else k[n++] = ex_mul2(ex_num(e->a[i]), x);
    }
    Expr *l = n == 0 ? ex_int(0) : n == 1 ? k[0] : ex_node(E_ADD, n, k);
    return ex_rel(R_EQ, l, ex_num(e->k));
}

static bool solve_rec(Sys *S, Eq *E, int n, bool *active, Rat *val, int *kind);

static bool eliminate(Sys *S, Eq *E, int n, bool *active, Rat *val, int *kind)
{
    SvCtx *c = S->c;
    int best = -1, piv = -1;
    int64_t bestcost = 0;
    for (int v = 0; v < S->nv; v++) {
        if (!active[v]) continue;
        int p = -1;
        for (int i = 0; i < n; i++)
            if (!rat_is_zero(E[i].a[v]) && (p < 0 || rat_cmp(rabs(E[i].a[v]), rabs(E[p].a[v])) < 0)) p = i;
        if (p < 0) continue;
        int64_t cost = 0, l;
        for (int i = 0; i < n; i++) {
            if (i == p || rat_is_zero(E[i].a[v])) continue;
            if (!rat_is_int(E[i].a[v]) || !rat_is_int(E[p].a[v]) || !i64_lcm(E[i].a[v].n, E[p].a[v].n, &l)) l = 1000000;
            cost += l < 0 ? -l : l;
        }
        if (best < 0 || cost <= bestcost) { best = v; piv = p; bestcost = cost; }
    }
    if (best < 0) return false;
    const char *vn = S->vars[best];
    Eq R[NV];
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (i == piv) continue;
        if (rat_is_zero(E[i].a[best])) { R[m++] = E[i]; continue; }
        Eq P = E[piv], Q = E[i];
        Rat ap = rabs(P.a[best]), aq = rabs(Q.a[best]), mp, mq;
        int64_t l;
        if (rat_is_int(ap) && rat_is_int(aq) && i64_lcm(ap.n, aq.n, &l)) {
            if (!rat_div(rat_int(l), ap, &mp) || !rat_div(rat_int(l), aq, &mq)) return sv_fail(c, "The numbers are too large");
        } else { mp = aq; mq = ap; }
        if (!rat_is_one(mp)) {
            if (!scale(&P, mp)) return sv_fail(c, "The numbers are too large");
            P.label = S->next++;
            sv_step(c, sv_fmt("Multiply equation (%d) by %s", E[piv].label, sv_rat_str(mp)),
                    sv_fmt("Call this (%d).", P.label), eq_str(S, &P));
        }
        if (!rat_is_one(mq)) {
            if (!scale(&Q, mq)) return sv_fail(c, "The numbers are too large");
            Q.label = S->next++;
            sv_step(c, sv_fmt("Multiply equation (%d) by %s", E[i].label, sv_rat_str(mq)),
                    sv_fmt("Call this (%d).", Q.label), eq_str(S, &Q));
        }
        bool same = rat_is_neg(P.a[best]) == rat_is_neg(Q.a[best]);
        Eq N;
        for (int k = 0; k < NV; k++)
            if (!(same ? rat_sub(Q.a[k], P.a[k], &N.a[k]) : rat_add(Q.a[k], P.a[k], &N.a[k]))) return sv_fail(c, "The numbers are too large");
        if (!(same ? rat_sub(Q.k, P.k, &N.k) : rat_add(Q.k, P.k, &N.k))) return sv_fail(c, "The numbers are too large");
        N.a[best] = rat_int(0);
        N.label = S->next++;
        const char *t = same ? sv_fmt("Subtract (%d) from (%d) to eliminate %s", P.label, Q.label, vn)
                             : sv_fmt("Add (%d) and (%d) to eliminate %s", P.label, Q.label, vn);
        sv_step(c, t, n > 2 ? sv_fmt("Call this (%d).", N.label) : NULL, eq_str(S, &N));
        R[m++] = N;
    }
    active[best] = false;
    if (!solve_rec(S, R, m, active, val, kind) || *kind != SV_ONE) return true;
    /* back-substitute into the pivot equation */
    Eq P = E[piv];
    bool known[NV];
    Rat sum = rat_int(0), t;
    const char *kv = "";
    for (int v = 0; v < S->nv; v++) known[v] = v != best;
    for (int v = 0; v < S->nv; v++) {
        if (v == best || rat_is_zero(P.a[v])) continue;
        if (!rat_mul(P.a[v], val[v], &t) || !rat_add(sum, t, &sum)) return sv_fail(c, "The numbers are too large");
        const char *one = sv_fmt("%s=%s", S->vars[v], sv_rat_str(val[v]));
        kv = *kv ? sv_fmt("%s and %s", kv, one) : one;
    }
    sv_step(c, sv_fmt("Substitute %s into (%d)", *kv ? kv : "the values", P.label), NULL,
            ex_print(subst_expr(S, &P, known, val)));
    Poly L = sv_lin_poly(P.a[best], sum, vn), Rr = poly_const(P.k);
    char save[12];
    memcpy(save, c->var, sizeof save);
    snprintf(c->var, sizeof c->var, "%s", vn);
    sv_step_new(c, sv_fmt("Solve for %s", vn), NULL, ex_print(ex_rel(R_EQ, sv_pexpr(&L, vn), ex_num(P.k))));
    SvLin res;
    bool ok = sv_linear(c, R_EQ, &L, &Rr, &res);
    memcpy(c->var, save, sizeof save);
    if (!ok || res.kind != SV_ONE) return false;
    val[best] = res.val;
    active[best] = true;
    return true;
}

static bool solve_rec(Sys *S, Eq *E, int n, bool *active, Rat *val, int *kind)
{
    *kind = SV_ONE;
    int na = 0;
    for (int v = 0; v < S->nv; v++) na += active[v];
    for (int i = 0; i < n; i++) {
        bool zero = true;
        for (int v = 0; v < S->nv; v++) zero = zero && rat_is_zero(E[i].a[v]);
        if (zero) {
            *kind = rat_is_zero(E[i].k) ? SV_ALL : SV_NONE;
            sv_step(S->c, *kind == SV_ALL ? "Every letter cancels: 0=0" : "This is impossible",
                    *kind == SV_ALL ? "The equations are the same line, so there are infinitely many solutions."
                                    : "The equations contradict each other, so there is no solution.", NULL);
            return true;
        }
    }
    if (na == 0 || n == 0) return false;
    if (n == 1 && na == 1) {
        int v = 0;
        while (!active[v]) v++;
        if (!rat_div(E[0].k, E[0].a[v], &val[v])) return false;
        if (!rat_is_one(E[0].a[v]))
            sv_step(S->c, sv_fmt("Divide both sides by %s", sv_rat_str(E[0].a[v])), NULL,
                    ex_print(ex_rel(R_EQ, ex_var(S->vars[v]), ex_num(val[v]))));
        return true;
    }
    return eliminate(S, E, n, active, val, kind);
}

static bool to_eq(const Expr *rel, Sys *S, Eq *e)
{
    Poly L, R, P, q;
    if (!poly_from_expr(rel->kids[0], &L) || !poly_from_expr(rel->kids[1], &R) || !poly_sub(&L, &R, &P)) return false;
    if (poly_total_degree(&P) > 1) return false;
    Rat c0 = rat_int(0);
    memset(e, 0, sizeof *e);
    for (int i = 0; i < NV; i++) e->a[i] = rat_int(0);
    for (int i = 0; i < S->nv; i++) {
        Rat k;
        if (!poly_coeff(&P, S->vars[i], 1, &q)) return false;
        if (poly_is_zero(&q)) continue;
        if (!poly_is_const(&q, &k)) return false;
        e->a[i] = k;
    }
    for (int i = 0; i < P.n; i++) {
        bool cst = true;
        for (int j = 0; j < POLY_MAXV; j++) cst = cst && P.t[i].e[j] == 0;
        if (cst) c0 = P.t[i].c;
    }
    e->k = rat_neg(c0);
    return true;
}

/* Multiply each equation with fractions (or decimals) by the LCM of its denominators,
 * so the elimination works with whole numbers: x/2+y=4 -> x+2y=8. */
static bool clear_fractions(Sys *S, Eq *E, int n, const Expr *sys)
{
    for (int i = 0; i < n; i++) {
        int64_t l = 1;
        int nfrac = 0;
        for (int v = 0; v <= S->nv; v++) {
            Rat r = v < S->nv ? E[i].a[v] : E[i].k;
            if (rat_is_int(r)) continue;
            nfrac++;
            if (!i64_lcm(l, r.d, &l)) return sv_fail(S->c, "The numbers are too large");
        }
        if (l == 1) continue;
        int from = E[i].label;
        if (!scale(&E[i], rat_int(l))) return sv_fail(S->c, "The numbers are too large");
        E[i].label = S->next++;
        bool dec = sv_has_decimal(sys->kids[i]);
        const char *what = dec ? (nfrac > 1 ? "decimals" : "decimal") : (nfrac > 1 ? "fractions" : "fraction");
        sv_step(S->c, sv_fmt("Multiply equation (%d) by %lld to clear the %s", from, (long long)l, what),
                sv_fmt("Call this (%d).", E[i].label), eq_str(S, &E[i]));
    }
    return true;
}

bool sv_system(SvCtx *c, const Expr *e, Expr **out)
{
    char vars[8][12];
    int n = e->nkids;
    for (int i = 0; i < n; i++)
        if (e->kids[i]->kind != E_REL || e->kids[i]->op != R_EQ || e->kids[i]->nkids != 2)
            return sv_fail(c, "Separate the equations with commas, each with one = sign");
    int nv = ex_vars(e, vars, 8);
    if (nv == 0) return sv_fail(c, "There are no letters to solve for");
    if (n < 2 || n > 3) return sv_fail(c, "I can solve 2 or 3 simultaneous equations");
    if (nv != n) return sv_fail(c, "You need as many equations as letters");
    Sys S = { c, nv, vars, n + 1 };
    Eq E[NV];
    Expr *shown[NV];
    bool linear = true;
    for (int i = 0; i < n && linear; i++) {
        linear = to_eq(e->kids[i], &S, &E[i]);
        E[i].label = i + 1;
        if (linear) shown[i] = eq_expr(&S, &E[i]);
    }
    if (!linear) {
        if (n == 2) return sv_system_linquad(c, e->kids, vars, out);
        return sv_fail(c, "With 3 equations they must all be linear");
    }
    const char *orig = ex_print(e), *now = ex_print(ex_node(E_LIST, n, shown));
    if (strcmp(orig, now) != 0)
        sv_step(c, nv == 2 ? "Rearrange into the form ax+by=c" : "Rearrange into the form ax+by+cz=d", NULL, now);
    sv_step(c, "Number the equations", n == 2 ? "Call them (1) and (2)." : "Call them (1), (2) and (3).", now);
    Eq O[NV];
    memcpy(O, E, sizeof O);
    if (!clear_fractions(&S, E, n, e)) return false;
    bool active[NV] = { true, true, true };
    Rat val[NV];
    int kind;
    for (int i = nv; i < NV; i++) active[i] = false;
    if (!solve_rec(&S, E, n, active, val, &kind)) {
        if (c->err && !c->err[0]) sv_fail(c, "These equations can't be solved together");
        return false;
    }
    if (kind != SV_ONE) {
        *out = ex_text(kind == SV_ALL ? "infinitely many solutions" : "no solution");
        return true;
    }
    bool known[NV] = { true, true, true };
    const Eq *chk = &O[n - 1];   /* check in the equation as it was given */
    Rat lhs = rat_int(0), t;
    for (int v = 0; v < nv; v++)
        if (!rat_mul(chk->a[v], val[v], &t) || !rat_add(lhs, t, &lhs)) break;
    Expr *cs = subst_expr(&S, chk, known, val);
    sv_step(c, sv_fmt("Check in equation (%d)", chk->label), sv_fmt("Both sides equal %s.", sv_rat_str(lhs)), ex_print(cs));
    Expr *k[NV];
    for (int v = 0; v < nv; v++) k[v] = ex_rel(R_EQ, ex_var(vars[v]), ex_num(val[v]));
    *out = ex_node(E_LIST, nv, k);
    return true;
}
