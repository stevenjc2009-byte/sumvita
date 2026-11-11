/* Inequalities: linear (flip the sign for a negative), double inequalities
 * a < px+q <= b (do the same to all three parts), quadratic (critical values,
 * sketch reasoning, then the regions). */
#include "solve_int.h"
#include <math.h>
#include <string.h>

static bool only_var(const Poly *p, const char *v)
{
    return p->nv == 0 || (p->nv == 1 && strcmp(p->v[0], v) == 0);
}

static void integers(SvCtx *c, double lo, bool lo_eq, double hi, bool hi_eq)
{
    double a = lo_eq ? ceil(lo - 1e-9) : floor(lo + 1e-9) + 1;
    double b = hi_eq ? floor(hi + 1e-9) : ceil(hi - 1e-9) - 1;
    if (b < a || b - a > 30) return;
    Expr *k[32];
    int n = 0;
    for (double v = a; v <= b && n < 32; v += 1) k[n++] = ex_int((long long)v);
    sv_step(c, "Integer solutions", sv_fmt("The whole numbers %s can be.", c->var),
            ex_print(n == 1 ? k[0] : ex_node(E_LIST, n, k)));
}

static Expr *text_all(SvCtx *c, bool all)
{
    sv_step(c, all ? "Every value works" : "No value works", NULL, NULL);
    return ex_text(all ? sv_fmt("true for all values of %s", c->var) : sv_fmt("no values of %s", c->var));
}

static bool quad_ineq(SvCtx *c, int op, const Poly *PL, const Poly *PR, Expr **out)
{
    Poly P;
    Rat p[SV_MAXDEG + 1];
    int d;
    if (!sv_to_zero(c, &op, PL, PR, &P) || !sv_upoly(&P, c->var, p, &d) || d != 2)
        return sv_fail(c, "The numbers are too large");
    Expr *pe = sv_pexpr(&P, c->var);
    sv_step(c, "Find the critical values", sv_fmt("Solve %s=0.", ex_print(pe)), ex_print(ex_rel(R_EQ, pe, ex_int(0))));
    SvRoots R;
    memset(&R, 0, sizeof R);
    if (c->st) steps_push(c->st);
    sv_quad_core(c, p[2], p[1], p[0], &R);
    if (c->st) steps_pop(c->st);
    Expr *x = ex_var(c->var);
    bool gt = op == R_GT || op == R_GE, eq = op == R_GE || op == R_LE;
    if (R.n == 0) {
        sv_step(c, "Sketch the graph", sv_fmt("y=%s is a U-shaped curve that never reaches the x-axis, so it is always above it.",
                ex_print(pe)), NULL);
        *out = text_all(c, gt);
        return true;
    }
    if (R.n == 1) {
        Expr *r = sv_root_expr(&R.r[0]);
        sv_step(c, "Sketch the graph", sv_fmt("y=%s is a U-shaped curve that just touches the x-axis at %s=%s.",
                ex_print(pe), c->var, ex_print(r)), NULL);
        if (op == R_GE) { *out = text_all(c, true); return true; }
        if (op == R_LT) { *out = text_all(c, false); return true; }
        *out = ex_rel(op == R_GT ? R_NE : R_EQ, x, r);
        sv_step(c, "Write the solution", NULL, ex_print(*out));
        return true;
    }
    int lo = sv_root_val(&R.r[0]) < sv_root_val(&R.r[1]) ? 0 : 1, hi = 1 - lo;
    R.r[0].pm_next = R.r[1].pm_next = false;
    Expr *el = sv_root_expr(&R.r[lo]), *eh = sv_root_expr(&R.r[hi]);
    sv_step(c, "Critical values", NULL, sv_fmt("%s \"or\" %s", ex_print(ex_rel(R_EQ, x, el)), ex_print(ex_rel(R_EQ, x, eh))));
    sv_step(c, "Sketch the graph",
            sv_fmt("y=%s is a U-shaped curve crossing the x-axis at %s and %s, so it is %s the axis %s them.",
                   ex_print(pe), ex_print(el), ex_print(eh), gt ? "above" : "below", gt ? "outside" : "between"),
            NULL);
    if (gt) {
        Expr *a = ex_rel(eq ? R_LE : R_LT, x, el), *b = ex_rel(op, x, eh);
        sv_step(c, "Write the solution", NULL, sv_fmt("%s \"or\" %s", ex_print(a), ex_print(b)));
        Expr *k[2] = { a, b };
        *out = ex_node(E_LIST, 2, k);
        return true;
    }
    Expr *k[3] = { el, x, eh };
    int ops[2] = { op, op };
    *out = sv_chain(3, k, ops);
    sv_step(c, "Write the solution", NULL, ex_print(*out));
    integers(c, sv_root_val(&R.r[lo]), eq, sv_root_val(&R.r[hi]), eq);
    return true;
}

bool sv_inequality(SvCtx *c, int op, const Expr *L, const Expr *R, Expr **out)
{
    Poly PL, PR, P;
    if (!poly_from_expr(L, &PL) || !poly_from_expr(R, &PR) || !only_var(&PL, c->var) || !only_var(&PR, c->var))
        return sv_fail(c, "I can only solve inequalities made of polynomials");
    if (!sv_prepare(c, op, L, R, &PL, &PR) || !poly_sub(&PL, &PR, &P)) return sv_fail(c, "The numbers are too large");
    int d = poly_degree(&P, c->var);
    if (d == 2) return quad_ineq(c, op, &PL, &PR, out);
    if (d > 2) return sv_fail(c, "I can only solve linear and quadratic inequalities");
    SvLin res;
    Poly Z = poly_zero();
    bool cancel = poly_degree(&PL, c->var) > 1 || poly_degree(&PR, c->var) > 1;
    if (cancel) sv_step_rel(c, "Rearrange so one side is 0", sv_fmt("The %s² terms cancel.", c->var), op, sv_pexpr(&P, c->var), ex_int(0));
    if (!sv_linear(c, op, cancel ? &P : &PL, cancel ? &Z : &PR, &res)) return false;
    if (res.kind != SV_ONE) {
        *out = ex_text(res.kind == SV_ALL ? sv_fmt("true for all values of %s", c->var) : sv_fmt("no values of %s", c->var));
        return true;
    }
    *out = ex_rel(res.op, ex_var(c->var), ex_num(res.val));
    return true;
}

static bool exact_num(const Expr *e, Rat *r)
{
    Expr *s = ex_simplify(e);
    if (!s || s->kind != E_NUM) return false;
    *r = s->r;
    return true;
}

static bool is_lt(int op) { return op == R_LT || op == R_LE; }
static bool is_gt(int op) { return op == R_GT || op == R_GE; }

bool sv_double_ineq(SvCtx *c, const Expr *e, Expr **out)
{
    if (e->nkids != 3) return sv_fail(c, "Use at most two inequality signs");
    int o1 = e->ops[0] ? e->ops[0] : e->op, o2 = e->ops[1];
    Rat lo, hi, a, b, t;
    const Expr *mid = e->kids[1];
    if (ex_has_var(e->kids[0], c->var) || ex_has_var(e->kids[2], c->var) || !ex_has_var(mid, c->var))
        return sv_fail(c, "Put the letter in the middle part only");
    if (!exact_num(e->kids[0], &lo) || !exact_num(e->kids[2], &hi)) return sv_fail(c, "The outer parts must be numbers");
    if (is_gt(o1) && is_gt(o2)) { t = lo; lo = hi; hi = t; int k = sv_flip(o1); o1 = sv_flip(o2); o2 = k; }
    else if (!(is_lt(o1) && is_lt(o2))) return sv_fail(c, "Both signs must point the same way");
    Poly M;
    Rat m[SV_MAXDEG + 1];
    int d;
    if (!poly_from_expr(mid, &M) || !sv_upoly(&M, c->var, m, &d) || d != 1) return sv_fail(c, "The middle part must be linear");
    a = m[1]; b = m[0];
    const char *v = c->var;
#define SHOW(t, n) do { Expr *k_[3] = { ex_num(lo), sv_lin_expr(a, b, v), ex_num(hi) }; int o_[2] = { o1, o2 }; \
                        sv_step_new(c, (t), (n), ex_print(sv_chain(3, k_, o_))); } while (0)
    int64_t l;
    if (sv_lcm_dens(&M, &l) && l > 1) {
        Rat L = rat_int(l);
        if (!rat_mul(lo, L, &lo) || !rat_mul(hi, L, &hi) || !rat_mul(a, L, &a) || !rat_mul(b, L, &b)) return sv_fail(c, "The numbers are too large");
        SHOW(sv_fmt("Multiply all three parts by %lld", (long long)l), NULL);
    }
    SHOW(strchr(ex_print(mid), '(') ? "Expand the brackets" : "Simplify", NULL);
    if (!rat_is_zero(b)) {
        const char *ti = rat_is_neg(b) ? sv_fmt("Add %s to all three parts", sv_rat_str(rat_neg(b)))
                                       : sv_fmt("Subtract %s from all three parts", sv_rat_str(b));
        if (!rat_sub(lo, b, &lo) || !rat_sub(hi, b, &hi)) return sv_fail(c, "The numbers are too large");
        b = rat_int(0);
        SHOW(ti, NULL);
    }
    if (!rat_is_one(a)) {
        const char *ti = sv_fmt("Divide all three parts by %s", sv_rat_str(a));
        if (!rat_div(lo, a, &lo) || !rat_div(hi, a, &hi)) return sv_fail(c, "The numbers are too large");
        const char *note = NULL;
        if (rat_is_neg(a)) {
            t = lo; lo = hi; hi = t;
            int k = o1; o1 = o2; o2 = k;
            note = "Dividing by a negative number reverses the inequality signs, so it is written the other way round.";
        }
        a = rat_int(1);
        SHOW(ti, note);
    }
#undef SHOW
    Expr *k[3] = { ex_num(lo), ex_var(v), ex_num(hi) };
    int ops[2] = { o1, o2 };
    *out = sv_chain(3, k, ops);
    if (rat_cmp(lo, hi) > 0 || (rat_eq(lo, hi) && (o1 == R_LT || o2 == R_LT))) {
        sv_step(c, "No value works", "No number can be in this range.", NULL);
        *out = ex_text(sv_fmt("no values of %s", v));
        return true;
    }
    integers(c, rat_to_double(lo), o1 == R_LE, rat_to_double(hi), o2 == R_LE);
    return true;
}
