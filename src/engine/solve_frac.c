/* Algebraic fractions as numerator / denominator polynomials, and equations with
 * the unknown in a denominator (multiply through, solve, reject any root that
 * makes a denominator zero). */
#include "solve_int.h"
#include <math.h>
#include <string.h>

static bool add_frac(Poly n1, Poly d1, Poly n2, Poly d2, Poly *N, Poly *D)
{
    Poly q, a, b;
    if (poly_equal(&d1, &d2)) { *D = d1; return poly_add(&n1, &n2, N); }
    if (poly_div_exact(&d2, &d1, &q)) { *D = d2; return poly_mul(&n1, &q, &a) && poly_add(&a, &n2, N); }
    if (poly_div_exact(&d1, &d2, &q)) { *D = d1; return poly_mul(&n2, &q, &a) && poly_add(&n1, &a, N); }
    return poly_mul(&n1, &d2, &a) && poly_mul(&n2, &d1, &b) && poly_add(&a, &b, N) && poly_mul(&d1, &d2, D);
}

bool sv_frac_form(const Expr *e, Poly *N, Poly *D)
{
    Poly n1, d1, n2, d2;
    if (!e) return false;
    if (poly_from_expr(e, N)) { *D = poly_const(rat_int(1)); return true; }
    switch (e->kind) {
    case E_ADD:
        if (!sv_frac_form(e->kids[0], &n1, &d1)) return false;
        for (int i = 1; i < e->nkids; i++) {
            if (!sv_frac_form(e->kids[i], &n2, &d2) || !add_frac(n1, d1, n2, d2, &n1, &d1)) return false;
        }
        *N = n1; *D = d1;
        return true;
    case E_MUL:
        if (!sv_frac_form(e->kids[0], &n1, &d1)) return false;
        for (int i = 1; i < e->nkids; i++) {
            if (!sv_frac_form(e->kids[i], &n2, &d2) || !poly_mul(&n1, &n2, &n1) || !poly_mul(&d1, &d2, &d1)) return false;
        }
        *N = n1; *D = d1;
        return true;
    case E_DIV:
        if (!sv_frac_form(e->kids[0], &n1, &d1) || !sv_frac_form(e->kids[1], &n2, &d2)) return false;
        if (poly_is_zero(&n2)) return false;
        return poly_mul(&n1, &d2, N) && poly_mul(&d1, &n2, D);
    case E_NEG:
        return sv_frac_form(e->kids[0], &n1, D) && poly_neg(&n1, N);
    case E_POW: {
        long long k;
        if (!ex_is_int(e->kids[1], &k) || k > 50 || k < -50) return false;
        if (!sv_frac_form(e->kids[0], &n1, &d1)) return false;
        if (k < 0) { Poly t = n1; n1 = d1; d1 = t; k = -k; if (poly_is_zero(&d1)) return false; }
        return poly_pow(&n1, (int)k, N) && poly_pow(&d1, (int)k, D);
    }
    default:
        return false;
    }
}

static bool only_var(const Poly *p, const char *v)
{
    return p->nv == 0 || (p->nv == 1 && strcmp(p->v[0], v) == 0);
}

static const char *bracketed(Expr *e)
{
    const char *s = ex_print(e);
    return e->kind == E_ADD ? sv_fmt("(%s)", s) : s;
}

/* *out stays NULL when this is not an equation with the unknown in a denominator */
bool sv_rational_eq(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    Poly nl, dl, nr, dr, M, ql, qr, NL, NR;
    *out = NULL;
    if (!sv_frac_form(L, &nl, &dl) || !sv_frac_form(R, &nr, &dr)) return true;
    if (!only_var(&nl, c->var) || !only_var(&dl, c->var) || !only_var(&nr, c->var) || !only_var(&dr, c->var)) return true;
    if (poly_is_const(&dl, NULL) && poly_is_const(&dr, NULL)) return true;
    if (poly_div_exact(&dl, &dr, &ql)) M = dl;
    else if (poly_div_exact(&dr, &dl, &qr)) M = dr;
    else if (!poly_mul(&dl, &dr, &M)) return true;
    if (!poly_div_exact(&M, &dl, &ql) || !poly_div_exact(&M, &dr, &qr)) return true;
    if (!poly_mul(&nl, &ql, &NL) || !poly_mul(&nr, &qr, &NR)) return true;
    /* drop a constant factor from M so the title reads "Multiply both sides by x" */
    Rat ct, inv;
    if (poly_content(&M, &ct) && rat_inv(ct, &inv)) {
        Poly t, a, b;
        int64_t la, lb;
        if (poly_scale(&M, inv, &t) && poly_scale(&NL, inv, &a) && poly_scale(&NR, inv, &b) &&
            sv_lcm_dens(&a, &la) && sv_lcm_dens(&b, &lb) && la == 1 && lb == 1) { M = t; NL = a; NR = b; }
    }
    sv_step(c, sv_fmt("Multiply both sides by %s", bracketed(sv_pexpr(&M, c->var))), "This clears the fractions.",
            ex_print(ex_rel(R_EQ, sv_pexpr(&NL, c->var), sv_pexpr(&NR, c->var))));
    Poly diff;
    Rat k;
    if (poly_sub(&NL, &NR, &diff) && poly_is_const(&diff, &k) && !rat_is_zero(k)) {
        /* x^2+x+1=x^2+x: the letters cancel and what is left is false */
        if (!poly_is_zero(&NR))
            sv_step(c, sv_fmt("Subtract %s from both sides", ex_print(sv_pexpr(&NR, c->var))), NULL,
                    ex_print(ex_rel(R_EQ, ex_num(k), ex_int(0))));
        sv_step(c, "This is never true", sv_fmt("%s=0 is false, so there is no solution.", sv_rat_str(k)), NULL);
        *out = ex_text("no solution");
        return true;
    }
    SvRoots Rt;
    memset(&Rt, 0, sizeof Rt);
    if (!sv_poly_solve(c, &NL, &NR, &Rt)) return false;
    Rat m[SV_MAXDEG + 1];
    int md;
    bool have = sv_upoly(&M, c->var, m, &md);
    SvRoots keep;
    memset(&keep, 0, sizeof keep);
    keep.all = Rt.all;
    for (int i = 0; i < Rt.n; i++) {
        SvRoot *r = &Rt.r[i];
        bool bad = false;
        Rat v;
        if (have && r->exact && rat_is_zero(r->B)) bad = upoly_eval(m, md, r->A, &v) && rat_is_zero(v);
        else if (have) {
            double x = sv_root_val(r), y = 0;
            for (int k = md; k >= 0; k--) y = y * x + rat_to_double(m[k]);
            bad = fabs(y) < 1e-9;
        }
        if (bad) {
            sv_step(c, sv_fmt("Reject %s=%s", c->var, ex_print(sv_root_expr(r))),
                    "It makes a denominator zero, so it is not a solution.", NULL);
            if (r->pm_next && i + 1 < Rt.n) Rt.r[i + 1].pm_next = false;
            if (i > 0 && keep.n > 0) keep.r[keep.n - 1].pm_next = false;
            continue;
        }
        keep.r[keep.n++] = *r;
    }
    if (keep.n > 0 && keep.r[keep.n - 1].pm_next) keep.r[keep.n - 1].pm_next = false;
    if (keep.all) {
        *out = ex_text(sv_fmt("true for all values of %s where it is defined", c->var));
        return true;
    }
    *out = Rt.n > 0 && keep.n == 0 ? ex_text("no solution") : sv_answer(c, &keep);
    return true;
}
