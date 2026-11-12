/* Factorise: driver, highest common factor, and the alg_factor entry point.
 * Methods live in factor_two.c (squares, cubes), factor_quad.c, factor_group.c, factor_root.c.
 * Factor order rule: see alg_int.h. */
#include "alg_int.h"

#include <stdio.h>
#include <string.h>

static const FMethod METHODS[] = {
    fac_hcf, fac_squares, fac_cubes, fac_quad, fac_psq, fac_group, fac_root, fac_quadfactor,
};

static bool add_piece(FCtx *c, const Poly *q, bool asc)
{
    Rat k;
    if (poly_is_const(q, &k)) return rat_mul(c->num, k, &c->num);
    if (q->n == 1) {
        Rat co, nk;
        Poly m, nm;
        if (!alg_term(q, 0, &co, &m, NULL) || !rat_mul(c->num, co, &nk) || !poly_mul(&c->mono, &m, &nm))
            return false;
        c->num = nk;
        c->mono = nm;
        return true;
    }
    if (c->nbr >= FAC_MAXBR) return false;
    c->br[c->nbr].p = *q;
    c->br[c->nbr].asc = asc || rat_is_neg(alg_lead_coef(q));
    c->nbr++;
    return true;
}

/* The user's own term order, so grouping pairs terms the way they were typed. */
static void take_typed(FCtx *c, const Expr *e)
{
    c->ntyped = 0;
    if (!e || e->kind != E_ADD || e->nkids > 16) return;
    Poly sum = poly_zero(), ns;
    for (int i = 0; i < e->nkids; i++) {
        if (!poly_from_expr(e->kids[i], &c->typed[i]) || c->typed[i].n != 1) return;
        if (!poly_add(&sum, &c->typed[i], &ns)) return;
        sum = ns;
    }
    if (sum.n == e->nkids) c->ntyped = e->nkids;   /* no like terms typed separately */
}

bool fac_run(FCtx *c, const Poly *p, const Expr *typed, Steps *st)
{
    memset(c, 0, sizeof *c);
    c->num = rat_int(1);
    c->mono = poly_const(rat_int(1));
    c->tg = poly_const(rat_int(1));
    c->st = st;
    take_typed(c, typed);
    if (!add_piece(c, p, false)) return false;
    if (c->nbr == 1 && poly_total_degree(&c->br[0].p) == 1) {
        c->proven = true;
        c->why = "Every term is a single letter or number to the power 1, so there is nothing to factorise.";
    }
    FSplit *s = amalloc(sizeof *s);
    FCtx *save = amalloc(sizeof *save);
    for (int i = 0; i < c->nbr;) {
        bool ok = false;
        bool first = !c->any && i == 0;
        if (!first) { c->proven = false; c->why = NULL; }
        c->noroots = false;
        for (size_t m = 0; m < sizeof METHODS / sizeof METHODS[0] && !ok; m++) {
            memset(s, 0, sizeof *s);
            steps_init(&s->pre);
            steps_init(&s->det);
            ok = METHODS[m](c, i, s);
        }
        if (!ok) { i++; continue; }
        *save = *c;
        FBr old = c->br[i];
        memmove(&c->br[i], &c->br[i + 1], sizeof(FBr) * (size_t)(c->nbr - i - 1));
        c->nbr--;
        bool good = true;
        for (int j = 0; j < s->n && good; j++) good = add_piece(c, &s->pc[j], s->asc[j] || (old.asc && old.p.n == 2));
        if (!good) { *c = *save; i++; continue; }
        c->any = true;
        if (st) {
            alg_steps_copy(st, &s->pre);
            steps_add(st, s->title, s->note, "%s", ex_print(fac_render(c, -1, NULL)));
            steps_push(st);
            alg_steps_copy(st, &s->det);
            steps_pop(st);
        }
    }
    return true;
}

bool fac_hcf(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Rat k;
    Poly m, g, r;
    if (!alg_hcf(q, &k, &m)) return false;
    bool neg = rat_is_neg(alg_lead_coef(q));
    if (neg && q->n == 2) {
        int li = alg_lead_index(q);
        if (!rat_is_neg(q->t[1 - li].c)) neg = false;   /* 25-x^2 stays as it is */
    }
    if (neg) k = rat_neg(k);
    if (rat_is_one(k) && poly_is_const(&m, NULL)) return false;
    if (!poly_scale(&m, k, &g) || !poly_div_exact(q, &g, &r)) return false;
    s->pc[0] = g;
    s->pc[1] = r;
    s->n = 2;
    if (!c->any && i == 0) { c->tg = g; c->tg_set = true; }
    const char *gs = ex_print(alg_km_expr(k, &m));
    if (rat_eq(k, rat_int(-1)) && poly_is_const(&m, NULL)) {
        s->title = "Take out a factor of -1";
        s->note = "This makes the first term positive, which makes it easier to factorise.";
    } else {
        s->title = asprintf_a("Take out the common factor %s", gs);
        s->note = asprintf_a("%s is the highest common factor of every term.", gs);
    }
    return true;
}

static bool has_add(const Expr *e)
{
    if (e->kind == E_ADD) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_add(e->kids[i])) return true;
    return false;
}

/* Already written as a plain sum of terms (no brackets to multiply out)? */
static bool is_flat(const Expr *e)
{
    if (e->kind != E_ADD) return !has_add(e) && e->kind != E_DIV;
    for (int i = 0; i < e->nkids; i++)
        if (has_add(e->kids[i]) || e->kids[i]->kind == E_DIV) return false;
    return true;
}

bool alg_factor(const Expr *e, Steps *st, Expr **out, char *err, int errcap)
{
    Poly p;
    if (!e || e->kind == E_REL || e->kind == E_LIST || !poly_from_expr(e, &p)) {
        snprintf(err, (size_t)errcap, "Can only factorise polynomials");
        return false;
    }
    if (poly_is_zero(&p) || poly_is_const(&p, NULL)) {
        steps_add(st, "Can't factorise this any further", "It is just a number.", "%s", ex_print(e));
        *out = ex_clone(e);
        return true;
    }
    bool flat = is_flat(e);
    if (!flat)
        steps_add(st, has_add(e) ? "Multiply out the brackets" : "Write it as a sum of terms", "Expand first so every term can be seen.", "%s", alg_pstr(&p));
    FCtx *c = amalloc(sizeof *c);
    if (!fac_run(c, &p, flat ? e : NULL, st)) {
        snprintf(err, (size_t)errcap, "Too big to factorise");
        return false;
    }
    if (!c->any) {
        bool proven = c->proven || c->nbr == 0;
        const char *why = c->why ? c->why : (c->nbr == 0 ? "It is a single term." :
            "None of the methods (common factor, quadratics, two squares, cubes, grouping, factor theorem) find a factor.");
        steps_add(st, proven ? "Can't factorise this any further" : "No factors found",
                  why, "%s", ex_print(e));
        *out = ex_clone(e);
        return true;
    }
    Expr *ans = fac_render(c, -1, NULL);
    Poly back;
    if (!poly_from_expr(ans, &back) || !poly_equal(&back, &p)) {
        snprintf(err, (size_t)errcap, "Factorise check failed");
        return false;
    }
    steps_add(st, "Check by expanding", "Multiplying the brackets back out gives the original expression.",
              "%s=%s", ex_print(ans), flat ? ex_print(e) : alg_pstr(&p));
    *out = ans;
    return true;
}
