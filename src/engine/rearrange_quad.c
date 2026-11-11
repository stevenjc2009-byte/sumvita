/* Changing the subject when it appears in a quadratic: y=x^2+x for x.
 * Bring every term to one side, then use the quadratic formula with the other
 * letters treated as numbers: x=(-1±√(1+4y))/2. With no subject term to the power 1
 * (y=3x^2+x^2) the x^2 terms are collected and square rooted instead. */
#include "solve_int.h"
#include "arena.h"
#include <string.h>

static Expr *pe(const Poly *p, const char *v) { return sv_nice(sv_pexpr(p, v)); }

static bool too_big(SvCtx *c) { return sv_fail(c, "The numbers are too large"); }

/* p for a×b×c substitution: bracketed unless one term with a positive whole coefficient */
static const char *par(const Poly *p, const char *v)
{
    if (poly_is_zero(p)) return "0";
    const char *s = ex_print(pe(p, v));
    if (p->n == 1 && !rat_is_neg(p->t[0].c) && rat_is_int(p->t[0].c)) return s;
    return sv_fmt("(%s)", s);
}

/* p for p^2: bracketed unless a whole number or a single letter */
static const char *par_sq(const Poly *p, const char *v)
{
    Rat k;
    if (poly_is_const(p, &k) && rat_is_int(k) && !rat_is_neg(k)) return sv_rat_str(k);
    if (p->n == 1 && rat_is_one(p->t[0].c) && poly_total_degree(p) == 1) return ex_print(pe(p, v));
    return sv_fmt("(%s)", ex_print(pe(p, v)));
}

/* b^2-4ac in the formula's own order, number first: 1+4y, b^2+4ay */
static Expr *disc_expr(const Poly *D, const char *v)
{
    Expr *e = sv_pexpr(D, v);
    if (e->kind != E_ADD) return e;
    Expr **k = amalloc(sizeof(Expr *) * (size_t)e->nkids);
    for (int i = 0; i < e->nkids; i++) k[i] = e->kids[e->nkids - 1 - i];
    return sv_nice(ex_node(E_ADD, e->nkids, k));
}

/* (nb ± √D)/den */
static Expr *formula(const Poly *nb, const Poly *D, const Poly *den, const char *v)
{
    Expr *r = ex_func1(F_SQRT, disc_expr(D, v)), *top;
    if (poly_is_zero(nb)) {
        top = ex_node(E_PM, 1, &r);
    } else {
        Expr *k[2] = { pe(nb, v), r };
        top = ex_node(E_PM, 2, k);
    }
    Rat d;
    if (poly_is_const(den, &d) && rat_is_one(d)) return top;
    return ex_div2(top, pe(den, v));
}

/* positive whole-number content of p (1 if it is not a whole number) */
static int64_t content_int(const Poly *p)
{
    Rat r;
    if (!poly_content(p, &r) || !rat_is_int(r) || r.n < 1) return 1;
    return r.n;
}

/* a·v^2 + k = 0: collect, divide, square root (±) */
static bool pure_square(SvCtx *c, const Poly *a, const Poly *k, Expr **out)
{
    const char *v = c->var;
    Poly nk, xv = poly_var(v), x2, ax2;
    if (!poly_neg(k, &nk) || !poly_mul(&xv, &xv, &x2) || !poly_mul(a, &x2, &ax2)) return too_big(c);
    Expr *x = ex_var(v), *sq = ex_pow2(ex_var(v), ex_int(2)), *K = pe(&nk, v);
    sv_step_new(c, sv_fmt("Collect the %s^2 terms on one side", v), NULL, ex_print(ex_rel(R_EQ, pe(&ax2, v), K)));
    Rat av;
    if (!(poly_is_const(a, &av) && rat_is_one(av))) {
        Expr *ae = pe(a, v);
        K = rr_div(K, ae);
        sv_step(c, sv_fmt("Divide both sides by %s", ex_print(rr_par(ae))), NULL, ex_print(ex_rel(R_EQ, sq, K)));
    }
    Expr *ans;
    if (poly_is_zero(&nk)) {
        ans = ex_int(0);
    } else {
        Expr *r = ex_func1(F_SQRT, K);
        ans = ex_node(E_PM, 1, &r);
    }
    sv_step(c, "Square root both sides", "A square root can be positive or negative, so write ±.",
            ex_print(ex_rel(R_EQ, x, ans)));
    *out = ex_rel(R_EQ, x, ans);
    return true;
}

bool rr_quadratic(SvCtx *c, const Poly *Q0, Expr **out)
{
    const char *v = c->var;
    Poly Q = *Q0, a, b, k, t;
    int64_t m;
    if (!poly_coeff(&Q, v, 2, &a) || !poly_coeff(&Q, v, 1, &b) || !poly_coeff(&Q, v, 0, &k)) return too_big(c);
    if (a.n > 0 && rat_is_neg(a.t[0].c)) {   /* keep the squared term positive */
        if (!poly_neg(&Q, &t)) return too_big(c);
        Q = t;
        if (!poly_neg(&a, &a) || !poly_neg(&b, &b) || !poly_neg(&k, &k)) return too_big(c);
    }
    if (poly_is_zero(&b)) return pure_square(c, &a, &k, out);
    Expr *x = ex_var(v), *zero = ex_int(0);
    sv_step_new(c, "Rearrange so one side is 0",
                sv_fmt("%s is squared, so this is a quadratic in %s. Treat the other letters like numbers.", v, v),
                ex_print(ex_rel(R_EQ, pe(&Q, v), zero)));
    if (!sv_lcm_dens(&Q, &m)) return too_big(c);
    if (m > 1) {
        if (!poly_scale(&Q, rat_int(m), &t)) return too_big(c);
        Q = t;
        sv_step(c, sv_fmt("Multiply every term by %lld", (long long)m), "This clears the fractions.",
                ex_print(ex_rel(R_EQ, pe(&Q, v), zero)));
        if (!poly_coeff(&Q, v, 2, &a) || !poly_coeff(&Q, v, 1, &b) || !poly_coeff(&Q, v, 0, &k)) return too_big(c);
    }
    sv_step(c, "Use the quadratic formula",
            sv_fmt("a=%s, b=%s, c=%s", ex_print(pe(&a, v)), ex_print(pe(&b, v)), ex_print(pe(&k, v))),
            sv_fmt("%s=(-b±√(b^2-4ac))/(2a)", v));
    const char *A = par(&a, v), *C = par(&k, v);
    sv_step(c, "Substitute a, b and c", NULL,
            sv_fmt("%s=(-%s±√(%s^2-4×%s×%s))/(2×%s)", v, par(&b, v), par_sq(&b, v), A, C, A));
    Poly b2, ak, D, nb, den;
    Rat dv;
    if (!poly_mul(&b, &b, &b2) || !poly_mul(&a, &k, &ak) || !poly_scale(&ak, rat_int(4), &t) || !poly_sub(&b2, &t, &D) ||
        !poly_neg(&b, &nb) || !poly_scale(&a, rat_int(2), &den))
        return too_big(c);
    if (poly_is_const(&D, &dv) && rat_is_neg(dv))
        return sv_fail(c, sv_fmt("b^2-4ac is negative, so %s has no real values", v));
    Expr *ans = poly_is_zero(&D) ? rr_div(pe(&nb, v), pe(&den, v)) : formula(&nb, &D, &den, v);
    sv_step(c, "Simplify", NULL, ex_print(ex_rel(R_EQ, x, ans)));
    /* cancel a whole number g from -b, √(b^2-4ac) and 2a: (-4±√(16+8y))/4 -> (-2±√(4+2y))/2 */
    Rat cD, s;
    int64_t sm, g = i64_gcd(content_int(&nb), content_int(&den));
    if (poly_is_zero(&D)) g = 1;
    else if (poly_content(&D, &cD) && rat_is_int(cD) && sv_sqrt_split(cD, &s, &sm) && rat_is_int(s)) g = i64_gcd(g, s.n);
    else g = 1;
    if (g > 1) {
        Poly nb2, D2, den2;
        if (!poly_scale(&nb, rat_make(1, g), &nb2) || !poly_scale(&den, rat_make(1, g), &den2) ||
            !poly_scale(&D, rat_make(1, g * g), &D2))
            return too_big(c);
        Expr *was = ex_func1(F_SQRT, disc_expr(&D, v)), *now = ex_func1(F_SQRT, disc_expr(&D2, v));
        ans = formula(&nb2, &D2, &den2, v);
        sv_step(c, sv_fmt("Cancel the common factor %lld", (long long)g),
                sv_fmt("%s=%lld%s, so the top and bottom both divide by %lld.", ex_print(was), (long long)g,
                       ex_print(now), (long long)g),
                ex_print(ex_rel(R_EQ, x, ans)));
    }
    *out = ex_rel(R_EQ, x, ans);
    return true;
}
