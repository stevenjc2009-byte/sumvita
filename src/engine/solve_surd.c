/* Equations with a square root of the unknown and the unknown outside it
 * (sqrt(x+3) = x-3): isolate the root, square both sides, solve what is left,
 * then check every answer in the original equation and reject the false ones. */
#include "solve_ext.h"
#include <math.h>
#include <string.h>

#define MAXR 8

static bool has_pm(const Expr *e)
{
    if (e->kind == E_PM) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_pm(e->kids[i])) return true;
    return false;
}

/* the + (sign 1) or - (sign -1) reading of every ± in e */
static Expr *pm_pick(const Expr *e, int sign)
{
    if (e->kind == E_PM) {
        if (e->nkids == 1) return sign > 0 ? pm_pick(e->kids[0], sign) : ex_neg(pm_pick(e->kids[0], sign));
        Expr *b = pm_pick(e->kids[1], sign);
        return ex_add2(pm_pick(e->kids[0], sign), sign > 0 ? b : ex_neg(b));
    }
    Expr *c = ex_clone(e);
    for (int i = 0; i < c->nkids; i++) c->kids[i] = pm_pick(e->kids[i], sign);
    return c;
}

static void add_ans(const Expr *r, Expr **ex, int *n)
{
    if (r->kind != E_REL || r->nkids != 2 || *n + 2 > MAXR) return;
    const Expr *v = r->kids[1];
    if (has_pm(v)) {
        ex[(*n)++] = pm_pick(v, 1);
        ex[(*n)++] = pm_pick(v, -1);
    } else ex[(*n)++] = ex_clone(v);
}

static bool poly_in(const Expr *e, const char *var, int maxdeg, int mindeg)
{
    Poly p;
    if (!poly_from_expr(e, &p)) return false;
    if (p.nv > 1 || (p.nv == 1 && strcmp(p.v[0], var) != 0)) return false;
    int d = poly_degree(&p, var);
    return d >= mindeg && d <= maxdeg;
}

static double evalv(const Expr *e, bool *ok)
{
    EvalEnv env;
    double v = NAN;
    memset(&env, 0, sizeof env);
    *ok = ex_eval(e, &env, &v) && isfinite(v);
    return v;
}

bool sv_surd_eq(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    const Expr *S = NULL, *O = NULL, *T = NULL;
    Rat a = rat_int(0), d = rat_int(0);
    for (int side = 0; side < 2 && !S; side++) {
        const Expr *s = side ? R : L, *o = side ? L : R, *t = NULL;
        Rat a1, d1;
        if (sv_lindec(c, s, F_SQRT, &a1, &d1, &t) && !rat_is_zero(a1) && t && poly_in(t->kids[0], c->var, 2, 1) &&
            poly_in(o, c->var, 2, 1)) { S = s; O = o; T = t; a = a1; d = d1; }
    }
    if (!S) return false;
    const char *var = c->var;
    Expr *Q = ex_clone(O);
    if (!rat_is_one(a) || !rat_is_zero(d)) {
        Q = ex_tidy(ex_simplify(ex_div2(ex_sub2(ex_clone(O), ex_num(d)), ex_num(a))));
        sv_step(c, "Isolate the square root", NULL, ex_print(ex_rel(R_EQ, ex_clone(T), Q)));
    }
    Expr *sq = ex_pow2(ex_clone(Q), ex_int(2));
    sv_step(c, "Square both sides", "Squaring can create answers that don't work, so each one is checked afterwards.",
            ex_print(ex_rel(R_EQ, ex_clone(T->kids[0]), sq)));
    Expr *sub = NULL;
    if (!sv_equation(c, T->kids[0], sq, &sub) || !sub) return false;
    Expr *cand[MAXR];
    int nc = 0;
    if (sub->kind == E_LIST) {
        for (int i = 0; i < sub->nkids; i++) add_ans(sub->kids[i], cand, &nc);
    } else add_ans(sub, cand, &nc);
    Expr *good[MAXR];
    double gv[MAXR];
    int ng = 0;
    for (int i = 0; i < nc; i++) {
        Expr *sl = ex_subst(L, var, cand[i]), *sr = ex_subst(R, var, cand[i]);
        bool okl, okr;
        double l = evalv(sl, &okl), r = evalv(sr, &okr);
        bool works = okl && okr && fabs(l - r) <= 1e-9 * fmax(1.0, fmax(fabs(l), fabs(r)));
        const char *ls = ex_print(ex_tidy(ex_simplify(sl))), *rs = ex_print(ex_tidy(ex_simplify(sr)));
        sv_step(c, sv_fmt("Check %s=%s in the original equation", var, ex_print(cand[i])),
                sv_fmt("Left side %s, right side %s. %s", ls, rs,
                       works ? "They match, so this answer works."
                             : "They don't match, so reject it: it only appeared because of the squaring."),
                ex_print(ex_rel(R_EQ, sl, sr)));
        if (works) {
            int j = ng++;
            double key = evalv(cand[i], &okl);
            while (j > 0 && gv[j - 1] > key) { good[j] = good[j - 1]; gv[j] = gv[j - 1]; j--; }
            good[j] = cand[i];
            gv[j] = key;
        }
    }
    if (ng == 0) {
        sv_step(c, "No solutions", "Every answer was rejected, so the equation has no solution.", NULL);
        *out = ex_text("no real solutions");
        return true;
    }
    Expr *items[MAXR];
    int n = 0;
    for (int i = 0; i < ng; i++)
        if (i == 0 || fabs(gv[i] - gv[i - 1]) > 1e-9) items[n++] = ex_rel(R_EQ, ex_var(var), good[i]);
    *out = n == 1 ? items[0] : ex_node(E_LIST, n, items);
    return true;
}
