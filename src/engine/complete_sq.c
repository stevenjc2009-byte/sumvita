/* Completing the square: ax^2+bx+c = a(x+p)^2+q with exact fractions, then the
 * turning point (-p, q) and whether it is a minimum or a maximum. Given an equation
 * ax^2+bx+c=0 it does the same working with "=0" on every line, then solves
 * a(x+p)^2 = -q by square rooting. */
#include "solve_ext.h"
#include <stdio.h>
#include <string.h>

/* (x+p)^2 with the bracket kept */
static Expr *sq(Rat p, const char *v)
{
    Expr *b = sv_lin_expr(rat_int(1), p, v);
    if (b->kind == E_ADD) b->flags |= EF_PAREN;
    return ex_pow2(b, ex_int(2));
}

/* k*e, written -e for k = -1 and e for k = 1. A fraction k is kept as the
 * division n/d in front, so it prints 1/2(x+1)^2 (GCSE) rather than (x+1)^2/2. */
static Expr *times(Rat k, Expr *e)
{
    if (rat_is_one(k)) return e;
    if (rat_eq(k, rat_int(-1))) return ex_neg(e);
    if (!rat_is_int(k)) return ex_mul2(ex_div2(ex_int(k.n), ex_int(k.d)), e);
    return ex_mul2(ex_num(k), e);
}

/* e + q, written e - |q| for a negative q, e for q = 0 */
static Expr *plus(Expr *e, Rat q)
{
    if (rat_is_zero(q)) return e;
    if (rat_is_neg(q)) return ex_add2(e, ex_neg(ex_num(rat_neg(q))));
    return ex_add2(e, ex_num(q));
}

static Expr *paren(Expr *e)
{
    e->flags |= EF_PAREN;
    return e;
}

/* "…=0" on each working line of an equation */
static const char *line(bool eq, Expr *e)
{
    return ex_print(eq ? ex_rel(R_EQ, e, ex_int(0)) : e);
}

/* a(x+p)^2 = -q  ->  x = -p ± √(-q/a): the square-root working and the answer */
static bool solve_rest(SvCtx *c, Rat a, Rat p, Rat q, Expr **out)
{
    const char *v = c->var;
    Rat r, s, mq;
    int64_t m;
    mq = rat_neg(q);
    if (!rat_div(mq, a, &r)) return sv_fail(c, "The numbers are too large");
    Expr *sqe = sq(p, v);
    if (!rat_is_zero(q)) sv_step_new(c, "Move the number to the other side", NULL, ex_print(ex_rel(R_EQ, times(a, sqe), ex_num(mq))));
    if (!rat_is_one(a))
        sv_step(c, sv_fmt("Divide both sides by %s", sv_rat_str(a)), NULL, ex_print(ex_rel(R_EQ, sq(p, v), ex_num(r))));
    SvRoots R;
    memset(&R, 0, sizeof R);
    if (rat_is_neg(r)) {
        sv_step(c, "No real solutions", "A squared number can't be negative.", NULL);
        *out = sv_answer(c, &R);
        return true;
    }
    Expr *lhs = sv_lin_expr(rat_int(1), p, v);
    Rat np = rat_neg(p);
    if (!sv_sqrt_split(r, &s, &m)) return sv_fail(c, "The numbers are too large");
    if (rat_is_zero(r)) {
        sv_step(c, "Square root both sides", NULL, ex_print(ex_rel(R_EQ, lhs, ex_int(0))));
        sv_add_rat(&R, np);
        *out = sv_answer(c, &R);
        return true;
    }
    Expr *root = ex_func1(F_SQRT, ex_num(r));
    sv_step(c, "Square root both sides", "Remember the negative square root as well.",
            ex_print(ex_rel(R_EQ, lhs, ex_node(E_PM, 1, &(Expr *){m == 1 ? ex_num(s) : root}))));
    Expr *mid = rat_is_zero(np) ? ex_node(E_PM, 1, &(Expr *){m == 1 ? ex_num(s) : root})
                                : ex_node(E_PM, 2, (Expr *[]){ex_num(np), m == 1 ? ex_num(s) : root});
    if (!rat_is_zero(p))
        sv_step(c, sv_fmt("%s %s %s both sides", rat_is_neg(p) ? "Add" : "Subtract", sv_rat_str(rat_abs(p)),
                       rat_is_neg(p) ? "to" : "from"), NULL,
                ex_print(ex_rel(R_EQ, ex_var(v), mid)));
    if (m == 1) {
        Rat r1, r2;
        if (!rat_sub(np, s, &r1) || !rat_add(np, s, &r2)) return sv_fail(c, "The numbers are too large");
        sv_add_rat(&R, r1);
        sv_add_rat(&R, r2);
        sv_step(c, "Work out both answers", NULL, sv_or_list(c, &R, false));
    } else {
        Rat Bq = s;
        sv_add_pair(&R, np, Bq, m);
        const char *l2 = ex_print(ex_rel(R_EQ, ex_var(v), sv_surd_expr(np, Bq, m, true)));
        if (strcmp(l2, ex_print(ex_rel(R_EQ, ex_var(v), mid))) != 0)
            sv_step(c, "Simplify the surd", sv_fmt("%s = %s", ex_print(root), ex_print(sv_surd_expr(rat_int(0), s, m, false))), l2);
        SvRoots T = R;
        T.r[0].pm_next = false;
        sv_step(c, sv_fmt("Write as decimals (%d d.p.)", sv_dp(c)), NULL, sv_or_list(c, &T, true));
    }
    *out = sv_answer(c, &R);
    return true;
}

bool alg_complete_square(const Expr *e, Steps *st, Expr **out, char *err, int errcap)
{
    SvCtx c;
    memset(&c, 0, sizeof c);
    c.st = st; c.err = err; c.errcap = errcap;
    if (err && errcap > 0) err[0] = 0;
    if (!e || !out) return sv_fail(&c, "Nothing to work with");
    *out = NULL;
    bool eq = e->kind == E_REL && e->nkids == 2 && e->op == R_EQ;
    if (eq) e = ex_add2(ex_clone(e->kids[0]), ex_neg(ex_clone(e->kids[1])));
    else if (e->kind == E_REL || e->kind == E_LIST) return sv_fail(&c, "Type a quadratic like x^2+6x+5, or an equation like x^2+6x+5=0");
    char names[8][12];
    int nv = ex_vars(e, names, 8);
    if (nv != 1) return sv_fail(&c, nv == 0 ? "There is no letter in this expression" : "Use just one letter");
    snprintf(c.var, sizeof c.var, "%s", names[0]);
    const char *v = c.var;
    Poly P;
    Rat k[SV_MAXDEG + 1];
    int d;
    if (!poly_from_expr(e, &P) || !sv_upoly(&P, v, k, &d) || d != 2)
        return sv_fail(&c, "Completing the square needs a quadratic like x^2+6x+5");
    Rat a = k[2], b = k[1], cc = k[0], ba, p, p2, q, t;
    c.last = ex_print(e);
    Expr *std = sv_pexpr(&P, v);
    sv_step_new(&c, eq ? "Write in the form ax^2+bx+c=0" : "Write in the form ax^2+bx+c", NULL, line(eq, std));
    if (!rat_div(b, a, &ba) || !rat_div(ba, rat_int(2), &p) || !rat_mul(p, p, &p2)) return sv_fail(&c, "The numbers are too large");
    /* q = c - a p^2 */
    if (!rat_mul(a, p2, &t) || !rat_sub(cc, t, &q)) return sv_fail(&c, "The numbers are too large");
    Expr *x2 = ex_pow2(ex_var(v), ex_int(2));
    if (rat_is_zero(b)) {
        sv_step(&c, sv_fmt("There is no %s term", v), sv_fmt("The square is just %s^2, so p=0.", v), NULL);
    } else if (rat_is_one(a)) {
        /* x^2+bx = (x+p)^2 - p^2 */
        Poly xb = sv_lin_poly(b, rat_int(0), v), xx = poly_var(v), x2p;
        if (!poly_mul(&xx, &xx, &x2p) || !poly_add(&x2p, &xb, &xb)) return sv_fail(&c, "The numbers are too large");
        sv_step(&c, sv_fmt("Halve the coefficient of %s", v),
                sv_fmt("Half of %s is %s, so %s=%s.", sv_rat_str(b), sv_rat_str(p), ex_print(sv_pexpr(&xb, v)),
                       ex_print(plus(sq(p, v), rat_neg(p2)))),
                line(eq, plus(plus(sq(p, v), rat_neg(p2)), cc)));
    } else {
        Poly in = sv_lin_poly(ba, rat_int(0), v), xx = poly_var(v), x2p;
        if (!poly_mul(&xx, &xx, &x2p) || !poly_add(&x2p, &in, &in)) return sv_fail(&c, "The numbers are too large");
        Expr *inner = sv_pexpr(&in, v);
        sv_step(&c, sv_fmt("Take out the factor %s from the %s^2 and %s terms", sv_rat_str(a), v, v), NULL,
                line(eq, plus(times(a, paren(ex_clone(inner))), cc)));
        Expr *done = plus(sq(p, v), rat_neg(p2));
        sv_step(&c, "Complete the square inside the bracket",
                sv_fmt("Half of %s is %s, so %s=%s.", sv_rat_str(ba), sv_rat_str(p), ex_print(inner), ex_print(done)),
                line(eq, plus(times(a, paren(ex_clone(done))), cc)));
        Rat ap2;
        if (!rat_mul(a, p2, &ap2)) return sv_fail(&c, "The numbers are too large");
        sv_step(&c, "Multiply out the outer bracket", NULL, line(eq, plus(plus(times(a, sq(p, v)), rat_neg(ap2)), cc)));
    }
    Expr *ans = plus(times(a, rat_is_zero(p) ? x2 : sq(p, v)), q);
    sv_step_new(&c, "Simplify", NULL, line(eq, ans));
    if (eq) return solve_rest(&c, a, p, q, out);
    Rat tx = rat_neg(p);
    bool min = !rat_is_neg(a);
    sv_step(&c, "Turning point",
            sv_fmt("The bracket is 0 when %s=%s, and a=%s is %s, so this is a %s point.", v, sv_rat_str(tx),
                   sv_rat_str(a), min ? "positive" : "negative", min ? "minimum" : "maximum"),
            sv_fmt("(%s, %s)", sv_rat_str(tx), sv_rat_str(q)));
    *out = ans;
    return true;
}
