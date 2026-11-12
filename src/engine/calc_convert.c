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

static Expr *recur_of(Rat r)
{
    Expr *e = amalloc(sizeof(Expr));
    e->kind = E_RECUR;
    e->r = r;
    return e;
}

/* The GCSE method. x = 0.0(4): 10x = 0.(4), 100x = 4.(4), 100x - 10x = 90x = 4,
 * x = 4/90 = 2/45. With nothing before the repeat (0.(3)) one multiply is enough:
 * 10x = 3.(3), 10x - x = 9x = 3. */
static bool recurring(const Expr *t, CalcResult *out)
{
    const char *txt = t->text, *lp = txt ? strchr(txt, '(') : NULL, *dot = txt ? strchr(txt, '.') : NULL;
    if (!lp || !dot || rat_is_neg(t->r) || t->r.d > 100000000) return false;
    int a = (int)(lp - dot - 1), rl = (int)strlen(lp) - 2;
    if (rl < 1 || rl > 9 || a < 0 || a + rl > 15) return false;
    int64_t lo = 1, hi;
    for (int i = 0; i < a; i++) lo *= 10;
    hi = lo;
    for (int i = 0; i < rl; i++) hi *= 10;
    Rat x = t->r, xlo, xhi, k;
    if (!rat_mul(x, rat_int(lo), &xlo) || !rat_mul(x, rat_int(hi), &xhi) || !rat_sub(xhi, xlo, &k) || k.d != 1)
        return false;
    int nd = a + 2 * rl + 1;
    if (nd < 5) nd = 5;
    Steps *st = &out->steps;
    steps_add(st, "Call the number x", asprintf_a("x = %s", dots(x, nd)), "x=%s", ex_print(t));
    if (a > 0)
        steps_add(st, asprintf_a("Multiply by %lld", (long long)lo),
                  asprintf_a("Move the point past the %s repeat: %lldx = %s", a == 1 ? "digit that doesn't" : "digits that don't",
                             (long long)lo, dots(xlo, nd - a)),
                  "%lldx=%s", (long long)lo, ex_print(recur_of(xlo)));
    steps_add(st, asprintf_a("Multiply by %lld", (long long)hi),
              asprintf_a("%d digit%s repeat%s, so move the point %d more place%s: %lldx = %s", rl, rl == 1 ? "" : "s",
                         rl == 1 ? "s" : "", rl, rl == 1 ? "" : "s", (long long)hi, dots(xhi, nd - a - rl)),
              "%lldx=%s", (long long)hi, ex_print(recur_of(xhi)));
    const char *lox = a > 0 ? asprintf_a("%lldx", (long long)lo) : "x";
    steps_add(st, "Subtract to remove the repeating part",
              asprintf_a("%lldx - %s = %lldx", (long long)hi, lox, (long long)(hi - lo)), "%lldx=%lld",
              (long long)(hi - lo), (long long)k.n);
    steps_add(st, asprintf_a("Divide by %lld", (long long)(hi - lo)), "", "x=%lld/%lld", (long long)k.n,
              (long long)(hi - lo));
    cw_simplify_hcf(k, rat_int(hi - lo), st);
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
        !rat_eq(t->kids[0]->r, rat_int(a.n))) {
        const char *typed = ex_print(t), *imp = RD(r);
        if (strcmp(typed, imp) != 0) steps_add(st, "Write as an improper fraction", "", "%s=%s", typed, imp);
    }
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
