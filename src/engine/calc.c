/* calc_run: the one entry point the UI calls (calc.h). Parses, picks the job,
 * hands algebra to algebra.h, and owns Simplify plus a few shared helpers.
 * The numeric jobs live in calc_eval.c, calc_convert*.c, calc_number.c, calc_surd*.c. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "algebra.h"
#include "arena.h"
#include "calc.h"
#include "calc_util.h"
#include "parse_norm.h"
#include "simplify_int.h"

void calc_settings_default(CalcSettings *s)
{
    if (!s) return;
    memset(s, 0, sizeof *s);
    s->degrees = true;
    s->dp = -1;
    s->sf = 0;
    s->prefer_exact = true;
    s->target_var[0] = 0;
    s->ans = 0;
    s->ans_set = false;
}

const char *calc_action_name(CalcAction a)
{
    static const char *const N[ACT_COUNT] = {
        "Auto", "Evaluate", "Simplify", "Expand", "Factorise", "Solve", "Complete square",
        "Rearrange", "\xE2\x86\x92 Fraction", "\xE2\x86\x92 Decimal", "\xE2\x86\x92 %",
        "\xE2\x86\x92 Mixed", "Standard form", "Surds",
    };
    return (unsigned)a < (unsigned)ACT_COUNT ? N[a] : "?";
}

/* ---- shared helpers ---- */

void cu_fail(CalcResult *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out->error, sizeof out->error, fmt, ap);
    va_end(ap);
    out->ok = false;
    out->answer = "";
    out->answer_alt = "";
    out->has_value = false;
}

const char *cu_rat_str(Rat r)
{
    char buf[64];
    rat_to_str(r, buf, sizeof buf);
    return astrdup(buf);
}

Expr *cu_rat_disp(Rat r)
{
    Rat a = rat_abs(r);
    Expr *e = a.d == 1 ? ex_num(a) : ex_div2(ex_num(rat_int(a.n)), ex_num(rat_int(a.d)));
    if (rat_is_neg(r)) return a.d == 1 ? ex_num(r) : ex_neg(e);
    return e;
}

Expr *cu_pretty(const Expr *e)
{
    if (!e) return NULL;
    Expr *c = amalloc(sizeof(Expr));
    *c = *e;
    if (e->nkids > 0) {
        c->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
        for (int i = 0; i < e->nkids; i++) c->kids[i] = cu_pretty(e->kids[i]);
    }
    if (c->kind == E_MUL && c->nkids >= 2 && c->kids[0]->kind == E_NUM && c->kids[0]->r.d != 1 &&
        !(c->kids[0]->flags & (NF_DECIMAL | EF_PAREN))) {
        Rat k = c->kids[0]->r;
        Expr *num;
        if (k.n == 1 || k.n == -1) {
            num = c->nkids == 2 ? c->kids[1] : ex_node(E_MUL, c->nkids - 1, c->kids + 1);
        } else {
            Expr **kk = amalloc(sizeof(Expr *) * (size_t)c->nkids);
            kk[0] = ex_num(rat_int(k.n < 0 ? -k.n : k.n));
            for (int i = 1; i < c->nkids; i++) kk[i] = c->kids[i];
            num = ex_node(E_MUL, c->nkids, kk);
        }
        Expr *d = ex_div2(num, ex_num(rat_int(k.d)));
        d->flags = c->flags & EF_PAREN;
        return k.n < 0 ? ex_neg(d) : d;
    }
    return c;
}

bool cu_has_decimal(const Expr *e)
{
    if (!e) return false;
    if (e->kind == E_NUM && (e->flags & NF_DECIMAL)) return true;
    for (int i = 0; i < e->nkids; i++)
        if (cu_has_decimal(e->kids[i])) return true;
    return false;
}

/* ---- Simplify ---- */

static int count_terms(const Expr *e)
{
    if (e->kind != E_ADD) return 1;
    int n = 0;
    for (int i = 0; i < e->nkids; i++) n += count_terms(e->kids[i]);
    return n;
}

static bool var_base(const Expr *f, const char *name)
{
    if (f->kind == E_POW) f = f->kids[0];
    return f->kind == E_VAR && strcmp(f->name, name) == 0;
}

static bool uses_index_laws(const Expr *e)
{
    if (e->kind == E_POW && (e->kids[0]->kind == E_POW || (e->kids[0]->kind == E_MUL && ex_has_var(e->kids[0], NULL))))
        return true;
    if (e->kind == E_MUL || e->kind == E_DIV) {
        char names[8][12];
        int nv = ex_vars(e, names, 8);
        for (int v = 0; v < nv; v++) {
            int cnt = 0;
            if (e->kind == E_DIV) {
                cnt = ex_has_var(e->kids[0], names[v]) + ex_has_var(e->kids[1], names[v]);
            } else {
                for (int i = 0; i < e->nkids; i++) cnt += var_base(e->kids[i], names[v]);
            }
            if (cnt >= 2) return true;
        }
    }
    for (int i = 0; i < e->nkids; i++)
        if (uses_index_laws(e->kids[i])) return true;
    return false;
}

static bool divides_by_letters(const Expr *e)
{
    if (e->kind == E_DIV && ex_has_var(e->kids[1], NULL)) return true;
    for (int i = 0; i < e->nkids; i++)
        if (divides_by_letters(e->kids[i])) return true;
    return false;
}

void calc_simplify(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    if (!ex_has_var(t, NULL) && t->kind != E_REL && t->kind != E_LIST) {
        calc_eval(t, s, out);
        return;
    }
    if (divides_by_letters(t)) {
        Expr *res = NULL;
        char err[160] = "";
        if (!alg_simplify_fraction(t, &out->steps, &res, err, sizeof err) || !res) {
            cu_fail(out, "%s", err[0] ? err : "Can't simplify this fraction");
            return;
        }
        out->answer = ex_print(res);
        return;
    }
    /* expand / common denominator / collect with GCSE working (simplify_steps.c) */
    Expr *w = NULL;
    if (simp_working(t, &out->steps, &w) && w) {
        out->answer = ex_print(w);
        return;
    }
    Expr *simp = ex_simplify(t);
    const char *ans = ex_print(cu_pretty(ex_tidy(simp)));
    const char *echo = ex_print(t);
    bool collect = count_terms(t) > count_terms(simp);
    bool index = uses_index_laws(t);
    if (strcmp(ans, echo) == 0) {
        steps_add(&out->steps, "Already as simple as it can be", "", NULL);
    } else {
        if (collect)
            steps_add(&out->steps, "Collect like terms", "Add the terms with the same letters and powers",
                      index ? NULL : "%s=%s", echo, ans);
        if (index)
            steps_add(&out->steps, "Use the index laws",
                      "Multiplying: add the powers. Dividing: subtract them. Power of a power: multiply them.",
                      "%s=%s", echo, ans);
        if (!collect && !index) steps_add(&out->steps, "Simplify", "", "%s=%s", echo, ans);
    }
    out->answer = ans;
}

/* ---- dispatch ---- */

static bool is_relation_input(const Expr *e)
{
    if (e->kind == E_REL) return true;
    if (e->kind != E_LIST) return false;
    bool any = false;
    for (int i = 0; i < e->nkids; i++) {
        if (e->kids[i]->kind == E_REL) any = true;
        else if (e->kids[i]->kind != E_TEXT) return false;
    }
    return any;
}

/* Solve answers that are alternatives read with "or", as a GCSE answer is
 * written: x=2 "or" x=3, and x=1, y=0 "or" x=3, y=2 for pairs. A list of
 * different letters (x=3, y=2) is one answer and keeps its commas. */
static bool same_var_rel(const Expr *k, const Expr *first)
{
    return k->kind == E_REL && k->nkids == 2 && k->kids[0]->kind == E_VAR &&
           first->kind == E_REL && first->nkids == 2 && first->kids[0]->kind == E_VAR &&
           strcmp(k->kids[0]->name, first->kids[0]->name) == 0;
}

static const Expr *solve_display(const Expr *res)
{
    if (res->kind != E_LIST || res->nkids < 2) return res;
    bool pairs = true, alts = true;
    int n = 0;
    for (int i = 0; i < res->nkids; i++) {
        const Expr *k = res->kids[i];
        if (k->kind != E_LIST) pairs = false;
        else n += k->nkids;
        if (!same_var_rel(k, res->kids[0])) alts = false;
    }
    if (!pairs && !alts) return res;
    if (!pairs) n = res->nkids;
    Expr **v = amalloc(sizeof(Expr *) * (size_t)(n + res->nkids));
    int m = 0;
    for (int i = 0; i < res->nkids; i++) {
        if (i) v[m++] = ex_text("or");
        const Expr *k = res->kids[i];
        if (!pairs) { v[m++] = (Expr *)k; continue; }
        for (int j = 0; j < k->nkids; j++) v[m++] = k->kids[j];
    }
    return ex_node(E_LIST, m, v);
}

static void run_algebra(CalcAction a, const Expr *t, const CalcSettings *s, CalcResult *out)
{
    Expr *res = NULL;
    char err[160] = "";
    bool ok = false;
    switch (a) {
    case ACT_EXPAND: ok = alg_expand(t, &out->steps, &res, err, sizeof err); break;
    case ACT_FACTOR: ok = alg_factor(t, &out->steps, &res, err, sizeof err); break;
    case ACT_SOLVE:
        ok = cs_factor_product(t, s, &out->steps, &res) || alg_solve(t, s, &out->steps, &res, err, sizeof err);
        if (ok && res) {
            char names[8][12];
            bool symbolic = ex_vars(t, names, 8) >= 2;
            if (symbolic) res = cs_tidy_answer(res);
            cs_tidy_steps(&out->steps, symbolic);
        }
        break;
    case ACT_COMPLETE_SQ: ok = alg_complete_square(t, &out->steps, &res, err, sizeof err); break;
    case ACT_REARRANGE:
        if (!s->target_var[0]) { cu_fail(out, "Choose the letter to make the subject"); return; }
        ok = alg_rearrange(t, s->target_var, &out->steps, &res, err, sizeof err);
        break;
    default: break;
    }
    if (!ok || !res) {
        cu_fail(out, "%s", err[0] ? err : "Can't do that with this input");
        return;
    }
    out->answer = ex_print(a == ACT_SOLVE || a == ACT_COMPLETE_SQ ? solve_display(res) : res);
    Expr *dec = a == ACT_SOLVE && cu_has_decimal(t) ? cs_decimal_answer(res) : NULL;
    if (dec) {      /* a decimal typed: answer as a decimal, the fraction as the other form */
        out->answer_alt = out->answer;
        out->answer = ex_print(solve_display(dec));
    }
    double v;
    EvalEnv env = {.degrees = s->degrees};
    if (res->kind == E_NUM && ex_eval(res, &env, &v)) {
        out->value = v;
        out->has_value = true;
    }
}

static void dispatch(CalcAction a, const Expr *t, const CalcSettings *s, CalcResult *out)
{
    /* solutions -> equation: "x=-2 and x=-3" on =, Solve or Expand (roots_eq.c) */
    if ((a == ACT_AUTO || a == ACT_SOLVE || a == ACT_EXPAND) && calc_roots_eq(t, out)) return;
    if (a == ACT_AUTO) {
        if (is_relation_input(t)) a = ACT_SOLVE;
        else if (!ex_has_var(t, NULL)) a = ACT_EVAL;
        else a = ACT_SIMPLIFY;
    }
    switch (a) {
    case ACT_EVAL:
        if (ex_has_var(t, NULL)) { cu_fail(out, "It has letters in it: try Simplify or Solve"); return; }
        calc_eval(t, s, out);
        return;
    case ACT_SIMPLIFY: calc_simplify(t, s, out); return;
    case ACT_FACTOR:
        if (!ex_has_var(t, NULL) && t->kind != E_REL && t->kind != E_LIST) { calc_prime_factors(t, out); return; }
        run_algebra(a, t, s, out);
        return;
    case ACT_EXPAND: case ACT_SOLVE: case ACT_COMPLETE_SQ: case ACT_REARRANGE:
        run_algebra(a, t, s, out);
        return;
    case ACT_TO_FRACTION: calc_to_fraction(t, s, out); return;
    case ACT_TO_DECIMAL: calc_to_decimal(t, s, out); return;
    case ACT_TO_PERCENT: calc_to_percent(t, s, out); return;
    case ACT_TO_MIXED: calc_to_mixed(t, s, out); return;
    case ACT_STD_FORM: calc_std_form(t, s, out); return;
    case ACT_SURD: calc_surd(t, s, out); return;
    default: cu_fail(out, "Unknown action"); return;
    }
}

static bool needs_number(CalcAction a)
{
    return a == ACT_TO_FRACTION || a == ACT_TO_DECIMAL || a == ACT_TO_PERCENT || a == ACT_TO_MIXED ||
           a == ACT_STD_FORM;
}

void calc_run(CalcAction action, const char *input, const CalcSettings *s, CalcResult *out)
{
    CalcSettings def;
    if (!out) return;
    if (!s) { calc_settings_default(&def); s = &def; }
    memset(out, 0, sizeof *out);
    out->ok = true;
    out->input_echo = "";
    out->answer = "";
    out->answer_alt = "";
    out->error_pos = -1;
    steps_init(&out->steps);
    if ((unsigned)action >= (unsigned)ACT_COUNT) { cu_fail(out, "Unknown action"); return; }

    char err[120];
    int pos = -1, off = 0;
    CalcAction cmd = ACT_AUTO;
    if (!input) input = "";
    off = pn_command(input, &cmd);          /* "solve 2x+3=11": the word picks the action on = */
    if (off > 0 && action == ACT_AUTO) action = cmd;
    input += off;
    Expr *e = ex_parse(input, err, sizeof err, &pos);
    if (!e) {
        cu_fail(out, "%s", err[0] ? err : "Can't read that: check the brackets and signs");
        out->error_pos = pos >= 0 ? pos + off : pos;
        return;
    }
    out->input_echo = ex_print(e);
    Expr *t = cu_prepare(e, s, out);
    if (!t) return;
    if (needs_number(action) && ex_has_var(t, NULL)) {
        cu_fail(out, "%s needs a number, not letters", calc_action_name(action) + (action == ACT_STD_FORM ? 0 : 4));
        return;
    }
    if ((action == ACT_SOLVE || (action == ACT_AUTO && is_relation_input(t))) && cs_precheck(t, input, out)) return;
    dispatch(action, t, s, out);
    if (!out->ok) {
        out->answer = "";
        out->answer_alt = "";
    }
    if (!out->answer) out->answer = "";
    if (!out->answer_alt) out->answer_alt = "";
}
