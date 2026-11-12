/* Changing the subject when it appears more than once: clear fractions,
 * expand, collect the subject terms on one side, factorise it out, divide. */
#include "solve_int.h"
#include <string.h>

/* split p into the terms containing v and the rest */
static bool split(const Poly *p, const char *v, Poly *with, Poly *without)
{
    Poly c0;
    if (!poly_coeff(p, v, 0, &c0) || !poly_sub(p, &c0, with)) return false;
    *without = c0;
    return true;
}

/* negative terms minus positive terms */
static int neg_bias(const Poly *p)
{
    int b = 0;
    for (int i = 0; i < p->n; i++) b += rat_is_neg(p->t[i].c) ? 1 : -1;
    return b;
}

static Expr *pe(const Poly *p, const char *v)
{
    return sv_nice(sv_pexpr(p, v));
}

static Expr *times_den(Expr *a, const Poly *D, const char *v)
{
    Rat k;
    if (poly_is_const(D, &k) && rat_is_one(k)) return a;
    Expr *d = pe(D, v);
    if (d->kind == E_NUM) return ex_mul2(d, rr_par(a));
    return ex_mul2(rr_par(a), rr_par(d));
}

bool rr_collect(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    const char *v = c->var;
    Poly nl, dl, nr, dr, PL, PR, A, B, wl, ol, wr, orr, t;
    if (!sv_frac_form(L, &nl, &dl) || !sv_frac_form(R, &nr, &dr))
        return sv_fail(c, sv_fmt("Can't make %s the subject: %s appears more than once and at least once inside "
                                 "a root, power or function, so the %s terms can't be collected together", v, v, v));
    if (poly_equal(&dl, &dr)) { dl = dr = poly_const(rat_int(1)); }
    Rat k;
    bool dens = !(poly_is_const(&dl, &k) && rat_is_one(k)) || !(poly_is_const(&dr, &k) && rat_is_one(k));
    if (!poly_mul(&nl, &dr, &PL) || !poly_mul(&nr, &dl, &PR)) return sv_fail(c, "The numbers are too large");
    if (dens) {
        Poly M;
        if (!poly_mul(&dl, &dr, &M)) return sv_fail(c, "The numbers are too large");
        Expr *ms = pe(&M, v);
        Expr *l = times_den(pe(&nl, v), &dr, v), *r = times_den(pe(&nr, v), &dl, v);
        sv_step(c, sv_fmt("Multiply both sides by %s", ex_print(rr_par(ms))), "This clears the fractions.",
                ex_print(ex_rel(R_EQ, l, r)));
    }
    sv_step_new(c, "Expand the brackets", NULL, ex_print(ex_rel(R_EQ, pe(&PL, v), pe(&PR, v))));
    Poly Q;
    if (!poly_sub(&PL, &PR, &Q)) return sv_fail(c, "The numbers are too large");
    int dq = poly_degree(&Q, v);
    if (dq == 2) return rr_quadratic(c, &Q, out);
    if (dq > 2)
        return sv_fail(c, sv_fmt("Can't make %s the subject: %s appears to the power %d and to a lower power too, "
                                 "and no rearranging step can undo both at once", v, v, dq));
    if (!split(&PL, v, &wl, &ol) || !split(&PR, v, &wr, &orr) || !poly_sub(&wl, &wr, &t) || !poly_sub(&orr, &ol, &B))
        return sv_fail(c, "The numbers are too large");
    if (!poly_coeff(&t, v, 1, &A)) return sv_fail(c, "The numbers are too large");
    if (poly_is_zero(&A)) return sv_fail(c, sv_fmt("The %s terms cancel, so %s can't be the subject", v, v));
    if (neg_bias(&A) + neg_bias(&B) > 0) {
        if (!poly_neg(&A, &A) || !poly_neg(&B, &B) || !poly_neg(&t, &t)) return sv_fail(c, "The numbers are too large");
    }
    sv_step_new(c, sv_fmt("Collect the %s terms on one side", v), NULL, ex_print(ex_rel(R_EQ, pe(&t, v), pe(&B, v))));
    Expr *ae = pe(&A, v), *x = ex_var(v);
    if (A.n > 1) sv_step(c, sv_fmt("Factorise out %s", v), NULL, ex_print(ex_rel(R_EQ, ex_mul2(x, rr_par(ae)), pe(&B, v))));
    Expr *ans;
    if (poly_is_const(&A, &k) && rat_eq(k, rat_int(-1))) {
        ans = rr_neg(pe(&B, v));
        sv_step(c, "Multiply both sides by -1", NULL, ex_print(ex_rel(R_EQ, x, ans)));
    } else if (poly_is_const(&A, &k) && rat_is_one(k)) {
        ans = pe(&B, v);
    } else {
        Expr *be = pe(&B, v);
        ans = ex_div2(rr_par(be), rr_par(ae));
        if (be->kind == E_DIV) ans = rr_div(be, ae);
        sv_step(c, sv_fmt("Divide both sides by %s", ex_print(rr_par(ae))), NULL, ex_print(ex_rel(R_EQ, x, ans)));
    }
    *out = ex_rel(R_EQ, x, ans);
    return true;
}
