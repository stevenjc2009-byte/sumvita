/* Shared helpers for test_solve.c / test_rearrange.c: run an algebra action on
 * typed input, check every step's maths re-parses, and substitute answers back. */
#ifndef SOLVE_CHECK_H
#define SOLVE_CHECK_H

#include <math.h>
#include <stdlib.h>
#include "testlib.h"
#include "engine/arena.h"
#include "engine/algebra.h"
#include "engine/calc.h"

static CalcSettings S;
static Steps ST;
static __attribute__((unused)) int dump;

static __attribute__((unused)) Expr *P(const char *s)
{
    char err[96];
    int pos;
    Expr *e = ex_parse(s, err, sizeof err, &pos);
    if (!e) printf("PARSE FAIL \"%s\": %s\n", s, err);
    return e;
}

/* every step has a title and its maths line (if any) parses */
static __attribute__((unused)) void check_steps(const char *in)
{
    for (int i = 0; i < ST.n; i++) {
        const Step *s = &ST.v[i];
        CHECK(s->title && s->title[0], "%s: step %d has no title", in, i);
        if (dump) printf("   %*s[%s] %s  {%s}\n", s->depth * 2, "", s->title, s->math ? s->math : "", s->note ? s->note : "");
        if (s->math && s->math[0]) {
            char err[96];
            int pos;
            CHECK(ex_parse(s->math, err, sizeof err, &pos) != NULL, "%s: step \"%s\" maths \"%s\" does not parse (%s)", in, s->title,
                  s->math, err);
        }
    }
}

enum { A_SOLVE, A_CSQ, A_REARR };

/* printed answer, or "ERR:<message>"; *res gets the answer tree */
static __attribute__((unused)) const char *run_alg(int act, const char *in, const char *var, Expr **res)
{
    Expr *e = P(in), *out = NULL;
    char err[160] = "";
    steps_init(&ST);
    if (!e) return "ERR:parse";
    bool ok = act == A_SOLVE ? alg_solve(e, &S, &ST, &out, err, sizeof err)
            : act == A_CSQ   ? alg_complete_square(e, &ST, &out, err, sizeof err)
                             : alg_rearrange(e, var, &ST, &out, err, sizeof err);
    if (dump) printf("%s  ->  %s\n", in, ok && out ? ex_print(out) : err);
    check_steps(in);
    if (res) *res = ok ? out : NULL;
    if (!ok || !out) return asprintf_a("ERR:%s", err);
    return ex_print(out);
}

/* ---- substitution ---- */
static __attribute__((unused)) Expr *pm_pick(const Expr *e, int sign)
{
    if (!e) return NULL;
    if (e->kind == E_PM) {
        if (e->nkids == 1) return sign > 0 ? pm_pick(e->kids[0], sign) : ex_neg(pm_pick(e->kids[0], sign));
        Expr *b = pm_pick(e->kids[1], sign);
        return ex_add2(pm_pick(e->kids[0], sign), sign > 0 ? b : ex_neg(b));
    }
    Expr *c = ex_clone(e);
    for (int i = 0; i < c->nkids; i++) c->kids[i] = pm_pick(e->kids[i], sign);
    return c;
}

static __attribute__((unused)) bool has_pm(const Expr *e)
{
    if (e->kind == E_PM) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_pm(e->kids[i])) return true;
    return false;
}

typedef struct { int n; const char *name[8]; double val[8]; } Sol;

static __attribute__((unused)) double ev(const Expr *e, const Sol *s)
{
    EvalEnv env;
    memset(&env, 0, sizeof env);
    env.degrees = true;
    for (int i = 0; i < s->n; i++) { env.var_names[i] = s->name[i]; env.var_values[i] = s->val[i]; }
    env.nvars = s->n;
    double v;
    if (!ex_eval(e, &env, &v)) return NAN;
    return v;
}

static __attribute__((unused)) bool eq_holds(const Expr *rel, const Sol *s)
{
    double a = ev(rel->kids[0], s), b = ev(rel->kids[1], s);
    return fabs(a - b) <= 1e-9 * fmax(1.0, fmax(fabs(a), fabs(b)));
}

/* collect solution sets from an answer tree */
static __attribute__((unused)) int collect(const Expr *ans, Sol *out, int max)
{
    if (!ans || ans->kind == E_TEXT) return 0;
    if (ans->kind == E_REL && ans->op == R_EQ && ans->nkids == 2 && ans->kids[0]->kind == E_VAR) {
        int n = 0;
        for (int sg = 1; sg >= -1 && n < max; sg -= 2) {
            Sol s = { 1, { ans->kids[0]->name }, { ev(pm_pick(ans->kids[1], sg), &(Sol){ 0, { 0 }, { 0 } }) } };
            out[n++] = s;
            if (!has_pm(ans->kids[1])) break;
        }
        return n;
    }
    if (ans->kind != E_LIST) return 0;
    bool system = ans->nkids >= 2 && ans->kids[0]->kind == E_REL && ans->kids[1]->kind == E_REL &&
                  ans->kids[0]->kids[0]->kind == E_VAR && ans->kids[1]->kids[0]->kind == E_VAR &&
                  strcmp(ans->kids[0]->kids[0]->name, ans->kids[1]->kids[0]->name) != 0;
    if (system) {
        Sol s = { 0, { 0 }, { 0 } };
        for (int i = 0; i < ans->nkids && i < 4; i++) {
            Sol one;
            if (collect(ans->kids[i], &one, 1) != 1) return 0;
            s.name[s.n] = one.name[0]; s.val[s.n++] = one.val[0];
        }
        out[0] = s;
        return 1;
    }
    int n = 0;
    for (int i = 0; i < ans->nkids && n < max; i++) n += collect(ans->kids[i], out + n, max - n);
    return n;
}

/* substitute every solution into every equation of the input; returns how many were checked */
static __attribute__((unused)) int verify(const char *in, const Expr *ans)
{
    Sol sols[16];
    int n = collect(ans, sols, 16);
    Expr *e = P(in);
    if (!e) return 0;
    for (int i = 0; i < n; i++) {
        if (e->kind == E_LIST) {
            for (int k = 0; k < e->nkids; k++)
                CHECK(eq_holds(e->kids[k], &sols[i]), "%s: solution %d fails equation %d", in, i, k);
        } else {
            CHECK(eq_holds(e, &sols[i]), "%s: solution %s=%.12g fails (L=%.12g R=%.12g)", in, sols[i].name[0], sols[i].val[0],
                  ev(e->kids[0], &sols[i]), ev(e->kids[1], &sols[i]));
        }
    }
    return n;
}

/* ---- inequalities: compare the input and the answer at many sample points ---- */
static __attribute__((unused)) bool rel_true(const Expr *r, double x, const char *v)
{
    Sol s = { 1, { v }, { x } };
    for (int i = 0; i + 1 < r->nkids; i++) {
        double a = ev(r->kids[i], &s), b = ev(r->kids[i + 1], &s);
        int op = i == 0 ? r->op : r->ops[i];
        bool ok = op == R_LT ? a < b : op == R_LE ? a <= b : op == R_GT ? a > b : op == R_GE ? a >= b
                : op == R_EQ ? fabs(a - b) < 1e-12 : fabs(a - b) >= 1e-12;
        if (!ok) return false;
    }
    return true;
}

static __attribute__((unused)) bool ans_true(const Expr *ans, double x, const char *v)
{
    if (ans->kind == E_TEXT) return strncmp(ans->text, "true", 4) == 0;
    if (ans->kind == E_LIST) {
        for (int i = 0; i < ans->nkids; i++)
            if (ans_true(ans->kids[i], x, v)) return true;
        return false;
    }
    return rel_true(ans, x, v);
}

static __attribute__((unused)) void verify_ineq(const char *in, const Expr *ans)
{
    Expr *e = P(in);
    if (!e || !ans) { CHECK(0, "%s: no answer", in); return; }
    int bad = 0;
    double badx = 0;
    for (double x = -30.0137; x < 30; x += 0.0731) {
        if (rel_true(e, x, "x") != ans_true(ans, x, "x")) { bad++; badx = x; }
    }
    CHECK(bad == 0, "%s: answer disagrees with the inequality at %d points (e.g. x=%g)", in, bad, badx);
}

#endif
