/* Polynomial equations of any degree once both sides are polynomials in the unknown:
 * linear -> solve_linear, quadratic -> solve_quad, cubic and above -> pure powers,
 * the factor theorem for rational roots, numeric roots for what is left. */
#include "solve_int.h"
#include "solve_ext.h"
#include "arena.h"
#include <math.h>
#include <string.h>

static const char *bracketed(Expr *e)
{
    const char *s = ex_print(e);
    return e->kind == E_ADD ? sv_fmt("(%s)", s) : s;
}

static void numeric_rest(SvCtx *c, const Rat *q, int d, SvRoots *R, const char *note)
{
    double a[SV_MAXDEG + 1], roots[SV_MAXDEG];
    for (int i = 0; i <= d; i++) a[i] = rat_to_double(q[i]);
    int n = sv_upoly_real_roots(a, d, roots, SV_MAXDEG);
    SvRoots T;
    memset(&T, 0, sizeof T);
    for (int i = 0; i < n; i++) { sv_add_real(R, roots[i]); sv_add_real(&T, roots[i]); }
    sv_step(c, "Solve numerically", note,
            n ? sv_or_list_num(c, &T) : NULL);
    /* GCSE trial and improvement: one bracketing line per decimal place */
    if (d == 3)
        for (int i = 0; i < n; i++) sv_trial_steps(c, a, d, roots[i]);
    if (!n) sv_step(c, "No real solutions", "This part has no real roots.", NULL);
}

static bool pure_power(SvCtx *c, const Rat *p, int d, SvRoots *R)
{
    for (int i = 1; i < d; i++)
        if (!rat_is_zero(p[i])) return false;
    Rat q, r;
    if (!rat_div(rat_neg(p[0]), p[d], &q)) return false;
    Expr *x = ex_var(c->var);
    sv_step_new(c, "Rearrange", NULL, ex_print(ex_rel(R_EQ, ex_pow2(x, ex_int(d)), ex_num(q))));
    const char *t = d == 3 ? "Cube root both sides" : sv_fmt("Take the %d%s root of both sides", d, d == 2 ? "nd" : "th");
    if (d % 2 == 0 && rat_is_neg(q)) {
        sv_step(c, "No real solutions", sv_fmt("An even power of %s can't be negative.", c->var), NULL);
        return true;
    }
    bool exact = rat_root_exact(rat_abs(q), d, &r) || rat_is_zero(q);
    if (rat_is_zero(q)) r = q;
    double v = exact ? rat_to_double(r) : pow(fabs(rat_to_double(q)), 1.0 / d);
    if (d % 2 == 1) {
        if (rat_is_neg(q)) { r = rat_neg(r); v = -v; }
        if (exact) sv_add_rat(R, r); else sv_add_real(R, v);
    } else if (exact && !rat_is_zero(r)) {
        int k = R->n;
        sv_add_rat(R, r);
        sv_add_rat(R, rat_neg(r));
        if (R->n == k + 2) { R->r[k].A = rat_int(0); R->r[k].B = r; R->r[k].pm_next = true;
                             R->r[k + 1].A = rat_int(0); R->r[k + 1].B = rat_neg(r); }
    } else if (exact) sv_add_rat(R, r);
    else { sv_add_real(R, v); sv_add_real(R, -v); }
    sv_step(c, t, d % 2 == 0 ? "Remember the negative root as well." : NULL, ex_print(sv_answer(c, R)));
    return true;
}

/* positive divisors of n (n > 0), up to max of them; false if n is too big to search */
static int divisors(int64_t n, int64_t *out, int max)
{
    int k = 0;
    if (n > 1000000000000LL) return -1;
    for (int64_t i = 1; i * i <= n && k < max; i++)
        if (n % i == 0) {
            out[k++] = i;
            if (i != n / i && k < max) out[k++] = n / i;
        }
    return k;
}

static bool find_root(const Rat *p, int d, Rat *root)
{
    int64_t l = 1;
    for (int i = 0; i <= d; i++)
        if (!i64_lcm(l, p[i].d, &l)) return false;
    Rat a0, an;
    if (!rat_mul(p[0], rat_int(l), &a0) || !rat_mul(p[d], rat_int(l), &an)) return false;
    if (rat_is_zero(a0)) { *root = rat_int(0); return true; }
    static int64_t dp[512], dq[512];
    int np = divisors(a0.n < 0 ? -a0.n : a0.n, dp, 512), nq = divisors(an.n < 0 ? -an.n : an.n, dq, 512);
    if (np < 0 || nq < 0) return false;
    for (int i = 0; i < np; i++)
        for (int j = 0; j < nq; j++)
            for (int s = 1; s >= -1; s -= 2) {
                Rat r = rat_make(s * dp[i], dq[j]), v;
                if (upoly_eval(p, d, r, &v) && rat_is_zero(v)) { *root = r; return true; }
            }
    return false;
}

static bool to_int_quad(const Rat *q, Rat *a, Rat *b, Rat *k)
{
    Poly p = poly_from_upoly(q, 2, "x");
    Rat ct, inv;
    Rat c[3];
    int d;
    if (!poly_content(&p, &ct) || !rat_inv(ct, &inv)) return false;
    if (rat_is_neg(q[2])) inv = rat_neg(inv);
    if (!poly_scale(&p, inv, &p) || !poly_to_upoly(&p, "x", c, 2, &d) || d != 2) return false;
    *a = c[2]; *b = c[1]; *k = c[0];
    return true;
}

static bool high_degree(SvCtx *c, const Poly *P, SvRoots *R)
{
    Rat p[SV_MAXDEG + 1], q[SV_MAXDEG + 1], rem, root;
    int d;
    if (!sv_upoly(P, c->var, p, &d)) return sv_fail(c, "The power is too high to solve");
    if (pure_power(c, p, d, R)) return true;
    Expr *fac[SV_MAXDEG];
    int nf = 0;
    bool any = false;
    while (d >= 3 && find_root(p, d, &root)) {
        if (!upoly_div_linear(p, d, root, q, &rem) || !rat_is_zero(rem)) break;
        Expr *f = rat_is_zero(root) ? ex_var(c->var) : sv_lin_expr(rat_int(root.d), rat_int(-root.n), c->var);
        const char *rs = sv_rat_str(root);
        const char *note = sv_fmt("f(%s) = 0, so %s is a factor.", rs, bracketed(f));
        sv_step(c, "Use the factor theorem", note, sv_fmt("f(%s)=0", rs));
        Rat scale = rat_make(1, root.d);
        for (int i = 0; i < d; i++)
            if (!rat_mul(q[i], scale, &q[i])) return sv_fail(c, "The numbers are too large");
        memcpy(p, q, sizeof(Rat) * (size_t)d);
        d--;
        fac[nf++] = f;
        Poly qp = poly_from_upoly(p, d, c->var);
        Expr *k[SV_MAXDEG + 1];
        for (int i = 0; i < nf; i++) k[i] = fac[i];
        k[nf] = sv_pexpr(&qp, c->var);
        sv_step(c, rat_is_zero(root) ? sv_fmt("Take out the common factor %s", c->var)
                                     : sv_fmt("Divide by %s", bracketed(f)), NULL,
                ex_print(ex_rel(R_EQ, ex_node(E_MUL, nf + 1, k), ex_int(0))));
        sv_add_rat(R, root);
        any = true;
    }
    if (d >= 3) {
        numeric_rest(c, p, d, R, any ? "There are no more whole-number or fraction roots, so the rest were found numerically."
                                     : "There are no whole-number or fraction roots, so the roots were found numerically.");
        return true;
    }
    Poly qp = poly_from_upoly(p, d, c->var);
    if (d == 2) {
        Rat a, b, k;
        sv_step(c, "Solve the quadratic factor", NULL, ex_print(ex_rel(R_EQ, sv_pexpr(&qp, c->var), ex_int(0))));
        if (to_int_quad(p, &a, &b, &k)) return sv_quad_core(c, a, b, k, R);
        sv_quad_numeric(c, rat_to_double(p[2]), rat_to_double(p[1]), rat_to_double(p[0]), R);
        return true;
    }
    Rat r;
    if (d == 1 && rat_div(rat_neg(p[0]), p[1], &r)) sv_add_rat(R, r);
    sv_step(c, "Solve each factor = 0", NULL, sv_or_list(c, R, false));
    return true;
}

bool sv_poly_solve(SvCtx *c, const Poly *PL, const Poly *PR, SvRoots *R)
{
    Poly P;
    Rat p[SV_MAXDEG + 1];
    int d, dl = poly_degree(PL, c->var), dr = poly_degree(PR, c->var);
    if (!poly_sub(PL, PR, &P)) return sv_fail(c, "The numbers are too large");
    if (!sv_upoly(&P, c->var, p, &d)) return sv_fail(c, "The power is too high to solve");
    if (d <= 1) {
        SvLin L;
        Poly Z = poly_zero();
        bool cancel = (dl > 1 || dr > 1) && d == 1;
        if (cancel) sv_step(c, "Rearrange so one side is 0", sv_fmt("The %s² terms cancel.", c->var),
                            ex_print(ex_rel(R_EQ, sv_pexpr(&P, c->var), ex_int(0))));
        if (!sv_linear(c, R_EQ, cancel ? &P : PL, cancel ? &Z : PR, &L)) return false;
        if (L.kind == SV_ONE) sv_add_rat(R, L.val);
        else if (L.kind == SV_ALL) R->all = true;
        return true;
    }
    if (d == 2 && rat_is_zero(p[1])) return sv_quad_core(c, p[2], rat_int(0), p[0], R);
    if (d >= 3 && pure_power(c, p, d, R)) return true;
    int op = R_EQ;
    if (!sv_to_zero(c, &op, PL, PR, &P) || !sv_upoly(&P, c->var, p, &d)) return sv_fail(c, "The numbers are too large");
    if (d == 2) return sv_quad_core(c, p[2], p[1], p[0], R);
    return high_degree(c, &P, R);
}
