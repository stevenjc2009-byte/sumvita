/* alg_solve: picks the unknown and the method (see solve_int.h for the parts).
 * Equations, inequalities (incl. double inequalities), systems (E_LIST), and
 * statements with no letter at all (true / false). */
#include "solve_int.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool only_var(const Poly *p, const char *v)
{
    return p->nv == 0 || (p->nv == 1 && strcmp(p->v[0], v) == 0);
}

bool sv_equation(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    Poly PL, PR;
    if (poly_from_expr(L, &PL) && poly_from_expr(R, &PR) && only_var(&PL, c->var) && only_var(&PR, c->var)) {
        if (!sv_prepare(c, R_EQ, L, R, &PL, &PR))
            return sv_numeric_fallback(c, L, R, "The numbers are too large to work with exactly, so it was solved numerically.", out);
        SvRoots Rt;
        memset(&Rt, 0, sizeof Rt);
        if (!sv_poly_solve(c, &PL, &PR, &Rt))
            return sv_numeric_fallback(c, L, R, "The numbers are too large to work with exactly, so it was solved numerically.", out);
        if (Rt.n == 0 && !Rt.all && poly_degree(&PL, c->var) <= 1 && poly_degree(&PR, c->var) <= 1) {
            *out = ex_text("no solution");
            return true;
        }
        *out = sv_answer(c, &Rt);
        return true;
    }
    return sv_nonpoly(c, L, R, out);
}

static bool holds(int op, double a, double b)
{
    double tol = 1e-9 * fmax(1.0, fmax(fabs(a), fabs(b)));
    switch (op) {
    case R_EQ: return fabs(a - b) <= tol;
    case R_NE: return fabs(a - b) > tol;
    case R_LT: return a < b - tol;
    case R_LE: return a <= b + tol;
    case R_GT: return a > b + tol;
    case R_GE: return a >= b - tol;
    }
    return false;
}

static bool no_letters(SvCtx *c, const Expr *e, Expr **out)
{
    EvalEnv env;
    memset(&env, 0, sizeof env);
    env.degrees = c->s ? c->s->degrees : true;
    Expr *k[EXPR_MAX_REL + 1];
    double v[EXPR_MAX_REL + 1];
    int n = e->nkids > EXPR_MAX_REL + 1 ? EXPR_MAX_REL + 1 : e->nkids;
    for (int i = 0; i < n; i++) {
        k[i] = ex_tidy(ex_simplify(e->kids[i]));
        if (!ex_eval(e->kids[i], &env, &v[i])) return sv_fail(c, "One side can't be worked out");
    }
    int ops[EXPR_MAX_REL];
    for (int i = 0; i < n - 1; i++) ops[i] = i == 0 ? e->op : e->ops[i];
    sv_step(c, "Work out each side", NULL, ex_print(sv_chain(n, k, ops)));
    bool ok = true;
    for (int i = 0; i + 1 < n; i++) ok = ok && holds(ops[i], v[i], v[i + 1]);
    sv_step(c, ok ? "The statement is true" : "The statement is false", NULL, NULL);
    *out = ex_text(ok ? "true" : "false");
    return true;
}

static bool pick_var(SvCtx *c, const Expr *e, char names[][12], int nv)
{
    if (c->s && c->s->target_var[0]) {
        snprintf(c->var, sizeof c->var, "%s", c->s->target_var);
        if (!ex_has_var(e, c->var)) return sv_fail(c, "That letter isn't in the equation");
        return true;
    }
    for (int i = 0; i < nv; i++)
        if (strcmp(names[i], "x") == 0) { strcpy(c->var, "x"); return true; }
    if (nv == 1) { memcpy(c->var, names[0], sizeof c->var); c->var[sizeof c->var - 1] = 0; return true; }
    return sv_fail(c, "Which letter should I solve for?");
}

bool alg_solve(const Expr *e, const CalcSettings *s, Steps *st, Expr **out, char *err, int errcap)
{
    SvCtx c;
    memset(&c, 0, sizeof c);
    c.s = s; c.st = st; c.err = err; c.errcap = errcap;
    if (err && errcap > 0) err[0] = 0;
    if (!e || !out) return sv_fail(&c, "Nothing to solve");
    *out = NULL;
    if (e->kind == E_LIST) return sv_system(&c, e, out);
    if (e->kind != E_REL) {
        Expr *eq = ex_rel(R_EQ, ex_clone(e), ex_int(0));
        sv_step(&c, "Set it equal to 0", NULL, ex_print(eq));
        e = eq;
    }
    if (e->nkids < 2) return sv_fail(&c, "Nothing to solve");
    char names[8][12];
    int nv = ex_vars(e, names, 8);
    c.last = ex_print(e);
    if (nv == 0) return no_letters(&c, e, out);
    if (!pick_var(&c, e, names, nv)) return false;
    if (e->nkids > 2) {
        if (nv > 1) return sv_fail(&c, "A double inequality needs just one letter");
        return sv_double_ineq(&c, e, out);
    }
    if (nv > 1) {
        if (e->op != R_EQ) return sv_fail(&c, "An inequality to solve needs just one letter");
        return alg_rearrange(e, c.var, st, out, err, errcap);
    }
    if (e->op == R_EQ) return sv_equation(&c, e->kids[0], e->kids[1], out);
    if (e->op == R_NE) return sv_fail(&c, "Can't solve a \xE2\x89\xA0 statement");
    return sv_inequality(&c, e->op, e->kids[0], e->kids[1], out);
}
