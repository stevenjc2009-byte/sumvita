/* Values for Evaluate: exact (fractions, surds, π) where possible, else a decimal.
 * Also the maths-error check and the answer / answer_alt text. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_more.h"

/* A canonical (ex_simplify) tree we can show exactly: numbers, π, roots of numbers. */
static bool exact_disp(const Expr *e, int depth)
{
    if (depth > 60) return false;
    switch (e->kind) {
    case E_NUM: return true;
    case E_CONST: return e->op == C_PI;
    case E_ADD: case E_MUL: case E_NEG: case E_DIV:
        for (int i = 0; i < e->nkids; i++)
            if (!exact_disp(e->kids[i], depth + 1)) return false;
        return true;
    case E_POW: {
        const Expr *b = e->kids[0], *x = e->kids[1];
        if (x->kind != E_NUM) return false;
        if (b->kind == E_NUM) return x->r.d <= 3 && !rat_is_neg(b->r);
        return x->r.d == 1 && x->r.n >= -12 && x->r.n <= 12 && exact_disp(b, depth + 1);
    }
    default: return false;
    }
}

static int count_nodes(const Expr *e, int limit)
{
    int n = 1;
    for (int i = 0; i < e->nkids && n < limit; i++) n += count_nodes(e->kids[i], limit - n);
    return n;
}

bool cu_value(const Expr *t, bool degrees, CVal *out)
{
    EvalEnv env = {.degrees = degrees};
    double v;
    out->e = NULL;
    out->exact = false;
    out->v = NAN;
    if (!t || !ex_eval(t, &env, &v)) return false;
    out->v = v;
    SurdSum ss;
    if (ss_from_expr(t, &ss)) {
        out->e = ss_to_expr(&ss);
        out->exact = true;
        return true;
    }
    Expr *simp = ex_simplify(t);
    if (ss_from_expr(simp, &ss)) {
        out->e = ss_to_expr(&ss);
        out->exact = true;
        return true;
    }
    if (exact_disp(simp, 0) && count_nodes(simp, 40) < 40) {
        out->e = cu_pretty(ex_tidy(simp));
        out->exact = true;
        return true;
    }
    out->e = ex_real(v);
    return true;
}

bool cu_rational(const Expr *t, Rat *r)
{
    SurdSum ss;
    if (ss_from_expr(t, &ss) && ss_rational(&ss, r)) return true;
    Expr *simp = ex_simplify(t);
    if (simp->kind == E_NUM) { *r = simp->r; return true; }
    return false;
}

/* ---- maths errors ---- */

static const char *why(const Expr *e, const EvalEnv *env)
{
    double a = 0, b = 0;
    bool ka = e->nkids > 0 && ex_eval(e->kids[0], env, &a);
    bool kb = e->nkids > 1 && ex_eval(e->kids[1], env, &b);
    switch (e->kind) {
    case E_DIV: return kb && b == 0 ? "Maths error: can't divide by 0" : "Maths error: answer too big";
    case E_POW:
        if (ka && a == 0 && kb && b < 0) return "Maths error: can't divide by 0";
        if (ka && a < 0) return "Maths error: no real root of a negative number";
        return "Maths error: answer too big";
    case E_FACT: return "Maths error: ! needs a whole number from 0 to 170";
    case E_FUNC:
        switch (e->op) {
        case F_SQRT: return "Maths error: square root of a negative number";
        case F_ROOT:
            if (ka && (a < 1 || a != round(a))) return "Maths error: the root index must be a whole number, 1 or more";
            if (kb && b < 0) return "Maths error: even root of a negative number";
            return "Maths error: answer too big";
        case F_LN: case F_LOG: return "Maths error: log of 0 or a negative number";
        case F_TAN: return env->degrees ? "Maths error: tan 90\xC2\xB0 is undefined" : "Maths error: tan is undefined here";
        case F_ASIN: case F_ACOS: return "Maths error: sin\xE2\x81\xBB\xC2\xB9 and cos\xE2\x81\xBB\xC2\xB9 need a value from -1 to 1";
        case F_NCR: case F_NPR: return "Maths error: needs whole numbers with r no bigger than n";
        default: break;
        }
        break;
    default: break;
    }
    return "Maths error: answer too big";
}

static const char *find_error(const Expr *e, const EvalEnv *env, int depth)
{
    if (depth > 300) return NULL;
    switch (e->kind) {
    case E_REL: case E_LIST: case E_TEXT: case E_PM: case E_VAR: return NULL;
    default: break;
    }
    for (int i = 0; i < e->nkids; i++) {
        const char *m = find_error(e->kids[i], env, depth + 1);
        if (m) return m;
    }
    for (int i = 0; i < e->nkids; i++) {
        ExprKind k = e->kids[i]->kind;
        if (k == E_REL || k == E_LIST || k == E_TEXT || k == E_PM || k == E_VAR) return NULL;
    }
    double v;
    if (ex_eval(e, env, &v)) return NULL;
    return why(e, env);
}

bool cu_domain_ok(const Expr *t, bool degrees, CalcResult *out)
{
    EvalEnv env = {.degrees = degrees};
    const char *m = find_error(t, &env, 0);
    if (m) { cu_fail(out, "%s", m); return false; }
    return true;
}

/* ---- answer text ---- */

static bool has_mixed(const Expr *e)
{
    if (!e) return false;
    if (e->kind == E_MIXED) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_mixed(e->kids[i])) return true;
    return false;
}

void cu_set_answer(const CVal *v, bool dec_pref, bool mixed_pref, const CalcSettings *s, CalcResult *out)
{
    const char *ans, *alt = "";
    bool decimal_main = false, rounded = false;
    Rat r;
    if (v->exact && cu_rational(v->e, &r)) {
        const char *dec = cu_exact_dec(r);
        if (r.d == 1) {
            ans = cu_rat_str(r);
        } else if (dec_pref && dec) {
            ans = dec;
            alt = cu_rat_str(r);
        } else {
            ans = cu_rat_str(r);
            alt = cu_fmt_dec(v->v, s, NULL);
            Rat a = rat_abs(r);
            if (a.n > a.d) {
                int64_t w = a.n / a.d;
                const char *mx = asprintf_a("%s%lld_%lld/%lld", rat_is_neg(r) ? "-" : "", (long long)w,
                                            (long long)(a.n - w * a.d), (long long)a.d);
                steps_add(&out->steps, "As a mixed number", "", "%s=%s", ans, mx);
                if (mixed_pref) ans = mx;       /* typed in mixed numbers: answer in them */
            }
        }
    } else if (v->exact) {
        ans = ex_print(v->e);
        alt = cu_fmt_dec(v->v, s, NULL);
    } else {
        ans = cu_fmt_dec(v->v, s, &rounded);
        decimal_main = true;
    }
    if (!s->prefer_exact && alt[0]) {
        const char *tmp = ans;
        ans = alt;
        alt = tmp;
        decimal_main = true;
        cu_fmt_dec(v->v, s, &rounded);
    }
    if (strcmp(ans, alt) == 0) alt = "";
    if (decimal_main && rounded) steps_add(&out->steps, cu_round_title(s), "", "%s", ans);
    out->answer = ans;
    out->answer_alt = alt;
    out->value = v->v;
    out->has_value = true;
}

static void eval_relation(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    EvalEnv env = {.degrees = s->degrees};
    bool truth = true;
    for (int i = 0; i + 1 < t->nkids; i++) {
        double a, b;
        if (!cu_domain_ok(t->kids[i], s->degrees, out) || !cu_domain_ok(t->kids[i + 1], s->degrees, out)) return;
        if (!ex_eval(t->kids[i], &env, &a) || !ex_eval(t->kids[i + 1], &env, &b)) {
            cu_fail(out, "Maths error");
            return;
        }
        double tol = 1e-12 * fmax(1.0, fmax(fabs(a), fabs(b)));
        int op = (i < EXPR_MAX_REL && t->ops[i]) ? t->ops[i] : t->op;
        bool ok;
        switch (op) {
        case R_LT: ok = a < b - tol; break;
        case R_LE: ok = a <= b + tol; break;
        case R_GT: ok = a > b + tol; break;
        case R_GE: ok = a >= b - tol; break;
        case R_NE: ok = fabs(a - b) > tol; break;
        default: ok = fabs(a - b) <= tol; break;
        }
        truth = truth && ok;
    }
    out->answer = truth ? "\"true\"" : "\"false\"";
}

void calc_eval(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    if (t->kind == E_REL) { eval_relation(t, s, out); return; }
    if (t->kind == E_TEXT) { cu_fail(out, "Nothing to work out"); return; }
    if (t->kind == E_LIST) {
        char *buf = "";
        for (int i = 0; i < t->nkids; i++) {
            const Expr *k = t->kids[i];
            const char *txt;
            CVal v;
            if (k->kind == E_TEXT || k->kind == E_REL) {
                cu_fail(out, "Evaluate one value at a time");
                return;
            }
            if (!cu_domain_ok(k, s->degrees, out)) return;
            if (!cu_value(k, s->degrees, &v)) { cu_fail(out, "Maths error"); return; }
            txt = v.exact ? ex_print(v.e) : cu_fmt_dec(v.v, s, NULL);
            buf = asprintf_a("%s%s%s", buf, i ? ", " : "", txt);
        }
        out->answer = buf;
        return;
    }
    if (!cu_domain_ok(t, s->degrees, out)) return;
    CVal v;
    if (!cu_value(t, s->degrees, &v)) { cu_fail(out, "Maths error"); return; }
    /* standard-form sums and surd fractions / brackets get the same GCSE working
     * as their own actions; everything else gets the BIDMAS walk */
    if (!cu_std_working(t, &out->steps)) {
        int sw = cu_surd_working(t, &out->steps, out);
        if (!out->ok) return;
        if (sw <= 0) calc_eval_steps(t, s->degrees, &out->steps);
    }
    cu_set_answer(&v, cu_has_decimal(t), has_mixed(t), s, out);
}
