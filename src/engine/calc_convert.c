/* -> Fraction (terminating decimal, recurring decimal by the algebra method,
 * percentage, mixed number, anything exact) and -> Mixed (both ways). */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_util.h"

#define TIMES "\xC3\x97"

static const char *RD(Rat r) { return ex_print(cu_rat_disp(r)); }

static void finish_rat(Rat r, const CalcSettings *s, CalcResult *out)
{
    out->answer = cu_rat_str(r);
    out->value = rat_to_double(r);
    out->has_value = true;
    const char *d = cu_exact_dec(r);
    out->answer_alt = r.d == 1 ? "" : (d ? d : cu_fmt_dec(out->value, s, NULL));
}

/* Unwrap a leading minus: the working is done on the positive number. */
static const Expr *unsign(const Expr *t, bool *neg)
{
    *neg = false;
    while (t->kind == E_NEG && t->nkids == 1) { *neg = !*neg; t = t->kids[0]; }
    return t;
}

/* Digits of r truncated after nd decimal places, then "…". */
static const char *dots(Rat r, int nd)
{
    char buf[96];
    uint64_t n = (uint64_t)r.n, d = (uint64_t)r.d;
    int o = snprintf(buf, sizeof buf, "%llu.", (unsigned long long)(n / d));
    uint64_t rem = n % d;
    for (int i = 0; i < nd && o < 80; i++) {
        rem *= 10;
        buf[o++] = (char)('0' + rem / d);
        rem %= d;
    }
    buf[o] = 0;
    return asprintf_a("%s\xE2\x80\xA6", buf);
}

/* x = 0.3(18): 100x = 31.8(18), 99x = 31.5, x = 31.5/99 = 315/990 = 7/22 */
static bool recurring(const Expr *t, CalcResult *out)
{
    const char *txt = t->text, *lp = txt ? strchr(txt, '(') : NULL, *dot = txt ? strchr(txt, '.') : NULL;
    if (!lp || !dot || rat_is_neg(t->r) || t->r.d > 100000000) return false;
    int a = (int)(lp - dot - 1), rl = (int)strlen(lp) - 2;
    if (rl < 1 || rl > 9) return false;
    int64_t p10 = 1;
    for (int i = 0; i < rl; i++) p10 *= 10;
    Rat x = t->r, big, diff;
    if (!rat_mul(x, rat_int(p10), &big) || !rat_mul(x, rat_int(p10 - 1), &diff)) return false;
    const char *dd = cu_exact_dec(diff);
    if (!dd) return false;
    int nd = a + 2 * rl + 1;
    if (nd < 5) nd = 5;
    Steps *st = &out->steps;
    steps_add(st, "Call the number x", asprintf_a("x = %s", dots(x, nd)), "x=%s", ex_print(t));
    Expr *bx = amalloc(sizeof(Expr));
    bx->kind = E_RECUR;
    bx->r = big;
    steps_add(st, asprintf_a("Multiply by %lld", (long long)p10),
              asprintf_a("%lld digit%s repeat%s, so move the point %d place%s: %lldx = %s", (long long)rl, rl == 1 ? "" : "s",
                         rl == 1 ? "s" : "", rl, rl == 1 ? "" : "s", (long long)p10, dots(big, nd - rl)),
              "%lldx=%s", (long long)p10, ex_print(bx));
    steps_add(st, "Subtract to remove the repeating part", asprintf_a("%lldx - x = %lldx", (long long)p10, (long long)(p10 - 1)),
              "%lldx=%s", (long long)(p10 - 1), dd);
    steps_add(st, asprintf_a("Divide by %lld", (long long)(p10 - 1)), "", "x=%s/%lld", dd, (long long)(p10 - 1));
    Rat top = diff, bot = rat_int(p10 - 1);
    while (top.d != 1) {
        if (!rat_mul(top, rat_int(10), &top) || !rat_mul(bot, rat_int(10), &bot)) return false;
    }
    if (!rat_eq(top, diff))
        steps_add(st, "Make the top a whole number", "Multiply the top and bottom by 10 until it is", "%s/%lld=%s",
                  dd, (long long)(p10 - 1), ex_print(ex_div2(ex_num(top), ex_num(bot))));
    cw_simplify_hcf(top, bot, st);
    return true;
}

void calc_to_fraction(const Expr *t0, const CalcSettings *s, CalcResult *out)
{
    bool neg;
    const Expr *t = unsign(t0, &neg);
    Rat r;
    Steps *st = &out->steps;
    if (!cu_domain_ok(t0, s->degrees, out)) return;
    if (!cu_rational(t, &r)) {
        cu_fail(out, "Can't write this exactly as a fraction");
        return;
    }
    if (t->kind == E_NUM && (t->flags & NF_DECIMAL) && r.d != 1) {
        int64_t D = 1, N;
        while (D % r.d != 0) D *= 10;
        N = r.n * (D / r.d);
        int places = 0;
        for (int64_t k = D; k > 1; k /= 10) places++;
        steps_add(st, asprintf_a("Write over %lld", (long long)D),
                  asprintf_a("%d decimal place%s, so the bottom is %lld", places, places == 1 ? "" : "s", (long long)D),
                  "%s=%s", ex_print(t), ex_print(ex_div2(ex_num(rat_int(N)), ex_num(rat_int(D)))));
        cw_simplify_hcf(rat_int(N), rat_int(D), st);
    } else if (t->kind == E_RECUR) {
        if (!recurring(t, out)) steps_add(st, "Recurring decimal", "", "%s=%s", ex_print(t), RD(r));
    } else if (t->kind == E_PCT) {
        Rat p;
        if (rat_mul(r, rat_int(100), &p)) {
            Rat x = p, h = rat_int(100);
            steps_add(st, "Percent means out of 100", "", "%s=%s/100", ex_print(t), ex_print(t->kids[0]));
            while (x.d != 1 && rat_mul(x, rat_int(10), &x) && rat_mul(h, rat_int(10), &h)) {}
            if (x.d == 1 && !rat_eq(x, p))
                steps_add(st, "Make the top a whole number", "", "%s/100=%s", ex_print(t->kids[0]),
                          ex_print(ex_div2(ex_num(x), ex_num(h))));
            if (x.d == 1) cw_simplify_hcf(x, h, st);
        }
    } else if (t->kind == E_MIXED && t->nkids == 3) {
        steps_add(st, "Change to an improper fraction", "Whole number × bottom + top", "%s=(%s" TIMES "%s+%s)/%s=%s",
                  ex_print(t), ex_print(t->kids[0]), ex_print(t->kids[2]), ex_print(t->kids[1]), ex_print(t->kids[2]),
                  RD(r));
    } else if (t->kind == E_DIV && t->kids[0]->kind == E_NUM && t->kids[1]->kind == E_NUM &&
               t->kids[0]->r.d == 1 && t->kids[1]->r.d == 1) {
        if (!cw_simplify_hcf(t->kids[0]->r, t->kids[1]->r, st))
            steps_add(st, "Already in its simplest form", "The top and bottom have no common factor", NULL);
    } else if (r.d == 1 && t->kind == E_NUM) {
        steps_add(st, "Already a whole number", "", "%s=%s/1", ex_print(t), cu_rat_str(r));
    } else {
        steps_add(st, "Work it out exactly", "", "%s=%s", ex_print(t), RD(r));
    }
    if (neg) {
        r = rat_neg(r);
        steps_add(st, "Put the minus sign back", "", "%s=%s", ex_print(t0), RD(r));
    }
    finish_rat(r, s, out);
    if (r.d != 1) out->answer = RD(r);
}

void calc_to_mixed(const Expr *t0, const CalcSettings *s, CalcResult *out)
{
    bool neg;
    const Expr *t = unsign(t0, &neg);
    Rat r;
    Steps *st = &out->steps;
    if (!cu_domain_ok(t0, s->degrees, out)) return;
    if (!cu_rational(t, &r)) { cu_fail(out, "Needs a fraction"); return; }
    const char *sign = neg ? "-" : "";
    if (t->kind == E_MIXED && t->nkids == 3) {             /* mixed -> improper */
        Rat w = t->kids[0]->r, n = t->kids[1]->r, d = t->kids[2]->r, wd, top;
        if (!rat_mul(w, d, &wd) || !rat_add(wd, n, &top)) { cu_fail(out, "Number too big"); return; }
        steps_add(st, "Multiply the whole number by the bottom", "", "%s" TIMES "%s=%s", cu_rat_str(w), cu_rat_str(d),
                  cu_rat_str(wd));
        steps_add(st, "Add the top", "Keep the same bottom", "%s=%s%s", ex_print(t0), sign,
                  ex_print(ex_div2(ex_num(top), ex_num(d))));
        if (neg) r = rat_neg(r);
        out->answer = asprintf_a("%s%s", sign, ex_print(ex_div2(ex_num(top), ex_num(d))));
        out->answer_alt = cu_fmt_dec(rat_to_double(r), s, NULL);
        out->value = rat_to_double(r);
        out->has_value = true;
        cw_simplify_hcf(top, d, st);
        return;
    }
    if (r.d == 1) {
        steps_add(st, "Already a whole number", "", NULL);
        if (neg) r = rat_neg(r);
        finish_rat(r, s, out);
        return;
    }
    if (rat_cmp(r, rat_int(1)) < 0 && !rat_is_neg(r)) {
        steps_add(st, "Already a proper fraction", "The top is smaller than the bottom", NULL);
        if (neg) r = rat_neg(r);
        finish_rat(r, s, out);
        out->answer = RD(r);
        return;
    }
    Rat a = rat_abs(r);
    if (rat_is_neg(r)) sign = neg ? "" : "-";
    int64_t w = a.n / a.d, rem = a.n - w * a.d;
    if (t->kind != E_DIV || t->kids[0]->kind != E_NUM || t->kids[1]->kind != E_NUM ||
        !rat_eq(t->kids[0]->r, rat_int(a.n)))
        steps_add(st, "Write as an improper fraction", "", "%s=%s", ex_print(t), RD(r));
    steps_add(st, "Divide the top by the bottom",
              asprintf_a("%lld \xC3\xB7 %lld = %lld remainder %lld", (long long)a.n, (long long)a.d, (long long)w,
                         (long long)rem),
              "%s%s=%s%lld_%lld/%lld", sign, ex_print(ex_div2(ex_int(a.n), ex_int(a.d))), sign, (long long)w,
              (long long)rem, (long long)a.d);
    out->answer = asprintf_a("%s%lld_%lld/%lld", sign, (long long)w, (long long)rem, (long long)a.d);
    double v = rat_to_double(a) * (sign[0] ? -1 : 1);
    out->answer_alt = cu_fmt_dec(v, s, NULL);
    out->value = v;
    out->has_value = true;
}
