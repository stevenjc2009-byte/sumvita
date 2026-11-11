/* Quadratics a x^2 + b x + c = 0: square-rooting (no x term), factorising
 * (rational roots), otherwise the quadratic formula with exact simplified surds
 * and a decimal line. */
#include "solve_int.h"
#include <math.h>
#include <string.h>

static void add_pair(SvRoots *R, Rat A, Rat B, int64_t m)
{
    if (R->n + 2 > SV_MAXROOTS) return;
    SvRoot *r = &R->r[R->n];
    memset(r, 0, 2 * sizeof *r);
    r[0].exact = r[1].exact = true;
    r[0].A = r[1].A = A;
    r[0].B = B;
    r[1].B = rat_neg(B);
    r[0].m = r[1].m = m;
    r[0].pm_next = true;
    r[0].v = sv_root_val(&r[0]);
    r[1].v = sv_root_val(&r[1]);
    R->n += 2;
}

static void decimals(SvCtx *c, const SvRoots *R, int from)
{
    SvRoots T;
    memset(&T, 0, sizeof T);
    for (int i = from; i < R->n; i++) T.r[T.n++] = R->r[i];
    for (int i = 0; i < T.n; i++) T.r[i].pm_next = false;
    sv_step(c, sv_fmt("Write as decimals (%d d.p.)", sv_dp(c)), NULL, sv_or_list(c, &T, true));
}

void sv_quad_numeric(SvCtx *c, double a, double b, double k, SvRoots *R)
{
    double d = b * b - 4 * a * k;
    if (d < 0) {
        sv_step(c, "Work out the discriminant", "b²−4ac is negative, so there are no real solutions.",
                sv_fmt("b^2-4ac=%s", sv_dec(d, sv_dp(c))));
        return;
    }
    int from = R->n;
    double s = sqrt(d);
    sv_add_real(R, (-b + s) / (2 * a));
    sv_add_real(R, (-b - s) / (2 * a));
    sv_step(c, "Use the quadratic formula",
            "The numbers are too large to keep exact, so the answers are decimals.", NULL);
    decimals(c, R, from);
}

/* (q x - p) for the root p/q, or just x for 0 */
static Expr *factor_of(Rat r, const char *v)
{
    if (rat_is_zero(r)) return ex_var(v);
    return sv_lin_expr(rat_int(r.d), rat_int(-r.n), v);
}

static bool by_factorising(SvCtx *c, Rat a, Rat s, Rat b, SvRoots *R)
{
    const char *v = c->var;
    Rat two_a, r1, r2, t;
    if (!rat_mul(rat_int(2), a, &two_a)) return false;
    if (!rat_sub(rat_neg(b), s, &t) || !rat_div(t, two_a, &r1)) return false;
    if (!rat_add(rat_neg(b), s, &t) || !rat_div(t, two_a, &r2)) return false;
    if (rat_cmp(r1, r2) > 0) { t = r1; r1 = r2; r2 = t; }
    if (rat_is_zero(r2) && !rat_is_zero(r1)) { t = r1; r1 = r2; r2 = t; }   /* x first */
    Rat k, qq;
    if (!rat_mul(rat_int(r1.d), rat_int(r2.d), &qq) || !rat_div(a, qq, &k)) return false;
    Expr *f1 = factor_of(r1, v), *f2 = factor_of(r2, v);
    bool rep = rat_eq(r1, r2);
    Expr *prod = rep ? ex_pow2(f1, ex_int(2)) : ex_mul2(f1, f2);
    if (rat_eq(k, rat_int(-1))) prod = ex_neg(prod);
    else if (!rat_is_one(k)) prod = ex_mul2(ex_num(k), prod);
    bool common = rat_is_zero(r1) || rat_is_zero(r2);
    sv_step(c, "Factorise", common ? sv_fmt("Take out the common factor %s.", v) : NULL,
            ex_print(ex_rel(R_EQ, prod, ex_int(0))));
    Expr *x = ex_var(v);
    if (rep) {
        sv_step(c, "Solve", "Both brackets are the same, so there is one (repeated) solution.",
                ex_print(ex_rel(R_EQ, x, ex_num(r1))));
        sv_add_rat(R, r1);
        return true;
    }
    sv_step(c, "Set each factor equal to 0", "If two things multiply to make 0, one of them must be 0.",
            sv_fmt("%s \"or\" %s", ex_print(ex_rel(R_EQ, f1, ex_int(0))), ex_print(ex_rel(R_EQ, f2, ex_int(0)))));
    sv_step(c, "Solve each one", NULL,
            sv_fmt("%s \"or\" %s", ex_print(ex_rel(R_EQ, x, ex_num(r1))), ex_print(ex_rel(R_EQ, x, ex_num(r2)))));
    sv_add_rat(R, r1);
    sv_add_rat(R, r2);
    return true;
}

static bool by_square_root(SvCtx *c, Rat a, Rat k, SvRoots *R)
{
    const char *v = c->var;
    Rat q, s;
    int64_t m;
    if (!rat_div(rat_neg(k), a, &q)) return false;
    Expr *x = ex_var(v);
    sv_step_new(c, "Rearrange", NULL, ex_print(ex_rel(R_EQ, ex_pow2(x, ex_int(2)), ex_num(q))));
    if (rat_is_neg(q)) {
        sv_step(c, "No real solutions", sv_fmt("%s² can't be negative.", v), NULL);
        return true;
    }
    if (rat_is_zero(q)) {
        sv_step(c, "Square root both sides", NULL, ex_print(ex_rel(R_EQ, x, ex_int(0))));
        sv_add_rat(R, q);
        return true;
    }
    if (!sv_sqrt_split(q, &s, &m)) return false;
    int from = R->n;
    add_pair(R, rat_int(0), s, m);
    sv_step(c, "Square root both sides", "Remember the negative square root as well.",
            ex_print(ex_rel(R_EQ, x, sv_surd_expr(rat_int(0), s, m, true))));
    if (m > 1) decimals(c, R, from);
    return true;
}

static const char *paren(Rat r)
{
    const char *s = sv_rat_str(r);
    return rat_is_neg(r) || !rat_is_int(r) ? sv_fmt("(%s)", s) : s;
}

static bool by_formula(SvCtx *c, Rat a, Rat b, Rat k, Rat D, SvRoots *R)
{
    const char *v = c->var;
    const char *A = paren(a), *C = paren(k);
    const char *nb = rat_is_neg(b) ? sv_fmt("-(%s)", sv_rat_str(b)) : sv_fmt("-%s", paren(b));
    const char *bsq = sv_fmt("%s^2", paren(b));
    sv_step(c, "Use the quadratic formula",
            sv_fmt("a=%s, b=%s, c=%s", sv_rat_str(a), sv_rat_str(b), sv_rat_str(k)),
            sv_fmt("%s=(-b±√(b^2-4ac))/(2a)", v));
    sv_step(c, "Substitute a, b and c", NULL, sv_fmt("%s=(%s±√(%s-4×%s×%s))/(2×%s)", v, nb, bsq, A, C, A));
    sv_step(c, "Work out the discriminant", NULL, sv_fmt("%s-4×%s×%s=%s", bsq, A, C, sv_rat_str(D)));
    if (rat_is_neg(D)) {
        sv_step(c, "No real solutions", "b²−4ac is negative, and you can't square root a negative number.", NULL);
        return true;
    }
    Rat two_a, s, Aq, Bq;
    int64_t m;
    if (!rat_mul(rat_int(2), a, &two_a)) return false;
    Expr *pk[2] = { ex_num(rat_neg(b)), ex_func1(F_SQRT, ex_num(D)) };
    Expr *raw = ex_div2(ex_node(E_PM, 2, pk), ex_num(two_a));
    const char *l1 = ex_print(ex_rel(R_EQ, ex_var(v), raw));
    sv_step(c, "Simplify", NULL, l1);
    if (!sv_sqrt_split(D, &s, &m) || !rat_div(rat_neg(b), two_a, &Aq) || !rat_div(s, two_a, &Bq)) return false;
    int from = R->n;
    add_pair(R, Aq, Bq, m);
    const char *l2 = ex_print(ex_rel(R_EQ, ex_var(v), sv_surd_expr(Aq, Bq, m, true)));
    if (strcmp(l1, l2) != 0) {
        const char *note = rat_is_one(s) ? NULL
            : sv_fmt("√%s = %s", sv_rat_str(D), ex_print(sv_surd_expr(rat_int(0), s, m, false)));
        sv_step(c, "Simplify the surd", note, l2);
    }
    decimals(c, R, from);
    return true;
}

bool sv_quad_core(SvCtx *c, Rat a, Rat b, Rat k, SvRoots *R)
{
    Rat bb, ac, D, s;
    if (rat_is_zero(b)) {
        if (by_square_root(c, a, k, R)) return true;
    } else if (rat_mul(b, b, &bb) && rat_mul(a, k, &ac) && rat_mul(ac, rat_int(4), &ac) && rat_sub(bb, ac, &D)) {
        if (!rat_is_neg(D) && rat_root_exact(D, 2, &s) && by_factorising(c, a, s, b, R)) return true;
        if (rat_is_zero(D) && by_factorising(c, a, rat_int(0), b, R)) return true;
        if (by_formula(c, a, b, k, D, R)) return true;
    }
    sv_quad_numeric(c, rat_to_double(a), rat_to_double(b), rat_to_double(k), R);
    return true;
}
