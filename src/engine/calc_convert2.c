/* -> Decimal (terminating check by the prime factors of the denominator,
 * recurring answers as recur(...) plus a rounded decimal) and -> %. */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_util.h"

#define TIMES "\xC3\x97"

static const char *RD(Rat r) { return ex_print(cu_rat_disp(r)); }

/* "2^3×11" */
static const char *factor_text(int64_t d, bool *only25, int64_t *other)
{
    int64_t pr[24];
    int ex[24];
    int n = d > 1 ? i64_factorize(d, pr, ex, 24) : 0;
    char buf[200];
    size_t o = 0;
    *only25 = true;
    *other = 0;
    for (int i = 0; i < n && o + 40 < sizeof buf; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%lld", i ? TIMES : "", (long long)pr[i]);
        if (ex[i] > 1) o += (size_t)snprintf(buf + o, sizeof buf - o, "^%d", ex[i]);
        if (pr[i] != 2 && pr[i] != 5) { *only25 = false; if (!*other) *other = pr[i]; }
    }
    buf[o] = 0;
    return astrdup(buf);
}

/* Recurring text of |r| ("0.3(18)") or NULL. */
static const char *recur_text(Rat r)
{
    char buf[128];
    if (!rat_to_recurring_str(rat_abs(r), buf, sizeof buf)) return NULL;
    return astrdup(buf);
}

static void check_denominator(Rat r, Steps *st)
{
    bool only25;
    int64_t other;
    if (r.d > 1000000000000LL) return;
    const char *ft = factor_text(r.d, &only25, &other);
    if (only25)
        steps_add(st, "Check the denominator", asprintf_a("%lld = %s has only 2s and 5s, so the decimal stops",
                                                          (long long)r.d, ft), NULL);
    else
        steps_add(st, "Check the denominator",
                  asprintf_a("%lld = %s has the prime factor %lld, so the decimal recurs", (long long)r.d, ft,
                             (long long)other), NULL);
}

void calc_to_decimal(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    Steps *st = &out->steps;
    if (!cu_domain_ok(t, s->degrees, out)) return;
    CVal v;
    if (!cu_value(t, s->degrees, &v)) { cu_fail(out, "Maths error"); return; }
    out->value = v.v;
    out->has_value = true;
    Rat r;
    if (!v.exact || !cu_rational(v.e, &r)) {
        bool rounded;
        out->answer = cu_fmt_dec(v.v, s, &rounded);
        out->answer_alt = v.exact ? ex_print(v.e) : "";
        if (rounded) steps_add(st, cu_round_title(s), "It never ends or repeats", "%s", out->answer);
        return;
    }
    if (r.d == 1) { out->answer = cu_rat_str(r); return; }
    check_denominator(r, st);
    const char *exact = cu_exact_dec(r);
    if (exact) {
        int64_t p10 = 10, mul;
        while (p10 % r.d != 0 && p10 < 1000000000000000LL) p10 *= 10;
        if (p10 % r.d == 0 && i64_mul_ok(r.n, p10 / r.d, &mul))
            steps_add(st, asprintf_a("Make the bottom %lld", (long long)p10),
                      asprintf_a("Multiply the top and bottom by %lld", (long long)(p10 / r.d)), "%s=%s=%s", RD(r),
                      ex_print(ex_div2(ex_num(rat_int(mul)), ex_num(rat_int(p10)))), exact);
        out->answer = exact;
        out->answer_alt = RD(r);
        return;
    }
    const char *rt = recur_text(r);
    const char *rounded = cu_fmt_dec(v.v, s, NULL);
    if (!rt) {
        out->answer = rounded;
        out->answer_alt = RD(r);
        steps_add(st, cu_round_title(s), "The repeating block is too long to show", "%s", rounded);
        return;
    }
    const char *ans = asprintf_a("%srecur(%s)", rat_is_neg(r) ? "-" : "", rt);
    steps_add(st, "Divide the top by the bottom", "The digits repeat for ever", "%s=%s", RD(r), ans);
    steps_add(st, cu_round_title(s), "", "%s", rounded);
    out->answer = ans;
    out->answer_alt = rounded;
}

void calc_to_percent(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    Steps *st = &out->steps;
    if (!cu_domain_ok(t, s->degrees, out)) return;
    CVal v;
    if (!cu_value(t, s->degrees, &v)) { cu_fail(out, "Maths error"); return; }
    out->value = v.v;
    out->has_value = true;
    if (t->kind == E_PCT) {
        out->answer = ex_print(t);
        steps_add(st, "Already a percentage", "", NULL);
        return;
    }
    Rat r, p;
    if (!v.exact || !cu_rational(v.e, &r) || !rat_mul(r, rat_int(100), &p)) {
        bool rounded;
        out->answer = asprintf_a("%s%%", cu_fmt_dec(v.v * 100, s, &rounded));
        steps_add(st, "Multiply by 100%", "", "%s" TIMES "100%%=%s", ex_print(t), out->answer);
        return;
    }
    const char *shown = r.d == 1 ? cu_rat_str(r) : (t->kind == E_NUM ? ex_print(t) : RD(r));
    const char *exact = cu_exact_dec(p), *ans;
    if (exact) {
        ans = asprintf_a("%s%%", exact);
        out->answer_alt = "";
    } else {
        const char *rt = recur_text(p);
        ans = rt ? asprintf_a("%srecur(%s)%%", rat_is_neg(p) ? "-" : "", rt)
                 : asprintf_a("%s%%", cu_fmt_dec(rat_to_double(p), s, NULL));
        out->answer_alt = asprintf_a("%s%%", cu_fmt_dec(rat_to_double(p), s, NULL));
    }
    const char *paren = (r.d != 1 || rat_is_neg(r)) && t->kind != E_NUM ? "(" : "";
    steps_add(st, "Multiply by 100%", "Per cent means out of 100", "%s%s%s" TIMES "100%%=%s", paren, shown,
              paren[0] ? ")" : "", ans);
    out->answer = ans;
}
