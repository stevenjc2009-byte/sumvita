/* Decimal answer text (10 s.f. by default, or the dp / sf setting), prime
 * factorisation with a factor tree, and standard form both ways. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "calc_more.h"
#include "calc_util.h"

#define TIMES "\xC3\x97"

/* "1.2E15" -> "1.2×10^15" */
static const char *sci(const char *mant, int e) { return asprintf_a("%s" TIMES "10^%d", mant, e); }

static void strip_zeros(char *s)
{
    if (!strchr(s, '.')) return;
    size_t l = strlen(s);
    while (l && s[l - 1] == '0') s[--l] = 0;
    if (l && s[l - 1] == '.') s[--l] = 0;
}

const char *cu_fmt_dec(double v, const CalcSettings *s, bool *rounded)
{
    char buf[96];
    const char *out;
    double back = v;
    if (!isfinite(v)) { if (rounded) *rounded = false; return "\"undefined\""; }
    if (v == 0) v = 0;          /* no "-0" */
    if (s && s->dp >= 0) {
        int dp = s->dp > 15 ? 15 : s->dp;
        if (fabs(v) >= 1e15) {
            snprintf(buf, sizeof buf, "%.*e", dp, v);
            char *E = strchr(buf, 'e');
            *E = 0;
            out = sci(buf, atoi(E + 1));
            back = strtod(buf, NULL) * pow(10, atoi(E + 1));
        } else {
            snprintf(buf, sizeof buf, "%.*f", dp, v);
            if (strspn(buf, "-0.") == strlen(buf) && buf[0] == '-') memmove(buf, buf + 1, strlen(buf));
            out = astrdup(buf);
            back = strtod(buf, NULL);
        }
    } else if (s && s->sf > 0) {
        int sf = s->sf > 15 ? 15 : s->sf;
        snprintf(buf, sizeof buf, "%.*e", sf - 1, v);
        char *E = strchr(buf, 'e');
        int ex = atoi(E + 1);
        *E = 0;
        double m = strtod(buf, NULL);
        back = m * pow(10, ex);
        if (ex >= -9 && ex < 15) {
            int dec = sf - 1 - ex;
            snprintf(buf, sizeof buf, "%.*f", dec > 0 ? dec : 0, back);
            out = astrdup(buf);
        } else {
            out = sci(buf, ex);
        }
    } else {
        dbl_to_str(v, 10, buf, sizeof buf);
        char *E = strchr(buf, 'E');
        if (E) {
            *E = 0;
            int ex = atoi(E + 1);
            back = strtod(buf, NULL) * pow(10, ex);
            out = sci(buf, ex);
        } else {
            back = strtod(buf, NULL);
            out = astrdup(buf);
        }
    }
    if (rounded) *rounded = fabs(back - v) > 1e-15 * fabs(v);
    return out;
}

const char *cu_round_title(const CalcSettings *s)
{
    if (s && s->dp >= 0) return asprintf_a("Round to %d decimal place%s", s->dp, s->dp == 1 ? "" : "s");
    if (s && s->sf > 0) return asprintf_a("Round to %d significant figure%s", s->sf, s->sf == 1 ? "" : "s");
    return "Round to 10 significant figures";
}

const char *cu_exact_dec(Rat r)
{
    if (!rat_terminates(r)) return NULL;
    int64_t d = r.d;
    int dp = 0;
    while (d > 1 && dp < 40) {
        if (d % 10 == 0) d /= 10;
        else if (d % 2 == 0) d /= 2;
        else d /= 5;
        dp++;
    }
    if (dp > 30) return NULL;
    char buf[96];
    rat_to_decimal_str(r, 30, buf, sizeof buf);
    return astrdup(buf);
}

/* ---- prime factors ---- */

void calc_prime_factors(const Expr *t, CalcResult *out)
{
    Rat r;
    if (!cu_rational(t, &r) || r.d != 1) {
        double v;                       /* a whole number past int64 is kept as a decimal value */
        EvalEnv env = {.degrees = true};
        if (ex_eval(t, &env, &v) && isfinite(v) && fabs(v) > 1e12 && v == floor(v))
            cu_fail(out, "Too big to factorise (largest is 10^12)");
        else
            cu_fail(out, "Prime factors need a whole number");
        return;
    }
    int64_t n = r.n;
    if (n < 2) { cu_fail(out, "Prime factors need a whole number 2 or more"); return; }
    if (n > 1000000000000LL) { cu_fail(out, "Too big to factorise (largest is 10^12)"); return; }
    int64_t pr[48];
    int ex[48], np = 0;
    int64_t m = n;
    while (m > 1 && np < 48) {
        int64_t p = 0;
        for (int64_t q = 2; q <= m / q; q += (q == 2 ? 1 : 2))
            if (m % q == 0) { p = q; break; }
        if (!p) p = m;
        if (p != m)
            steps_add(&out->steps, "Factor tree: split off a prime", asprintf_a("%lld is prime", (long long)p),
                      "%lld=%lld" TIMES "%lld", (long long)m, (long long)p, (long long)(m / p));
        if (np && pr[np - 1] == p) ex[np - 1]++;
        else { pr[np] = p; ex[np] = 1; np++; }
        m /= p;
    }
    char ans[400];
    size_t o = 0;
    for (int i = 0; i < np && o + 48 < sizeof ans; i++) {
        o += (size_t)snprintf(ans + o, sizeof ans - o, "%s%lld", i ? TIMES : "", (long long)pr[i]);
        if (ex[i] > 1) o += (size_t)snprintf(ans + o, sizeof ans - o, "^%d", ex[i]);
    }
    if (np == 1 && ex[0] == 1)
        steps_add(&out->steps, asprintf_a("%lld is prime", (long long)n), "Its only factors are 1 and itself", NULL);
    else
        steps_add(&out->steps, "Write the primes in index form", "Count how many times each prime appears",
                  "%lld=%s", (long long)n, ans);
    out->answer = astrdup(ans);
    out->value = (double)n;
    out->has_value = true;
}

/* ---- standard form ---- */

/* "6.02" shifted n places -> "602000000000000000000000"; NULL if too long to show */
const char *cu_shift_point(const char *m, int n)
{
    if (n > 38 || n < -38) return NULL;
    bool neg = m[0] == '-';
    if (neg) m++;
    char dig[48], buf[96];
    int nd = 0, ip = -1, o = 0;
    for (const char *p = m; *p && nd < 40; p++) {
        if (*p == '.') ip = nd;
        else dig[nd++] = *p;
    }
    if (ip < 0) ip = nd;
    int pos = ip + n;                   /* digits before the point */
    if (neg) buf[o++] = '-';
    if (pos <= 0) {
        buf[o++] = '0';
        buf[o++] = '.';
        for (int i = 0; i < -pos; i++) buf[o++] = '0';
        for (int i = 0; i < nd; i++) buf[o++] = dig[i];
    } else {
        for (int i = 0; i < pos; i++) buf[o++] = i < nd ? dig[i] : '0';
        if (pos < nd) {
            buf[o++] = '.';
            for (int i = pos; i < nd; i++) buf[o++] = dig[i];
        }
    }
    buf[o] = 0;
    strip_zeros(buf);
    char *s = buf + (neg ? 1 : 0);      /* "03" -> "3" */
    while (s[0] == '0' && s[1] && s[1] != '.') memmove(s, s + 1, strlen(s));
    return strlen(buf) > 44 ? NULL : astrdup(buf);
}

/* a×10^n with a an E_NUM and n an integer; also 10^n alone */
bool cu_std_parts(const Expr *e, Rat *a, int *n)
{
    const Expr *p = e;
    *a = rat_int(1);
    if (e->kind == E_MUL && e->nkids == 2 && e->kids[0]->kind == E_NUM) {
        *a = e->kids[0]->r;
        p = e->kids[1];
    }
    if (p->kind != E_POW || p->kids[0]->kind != E_NUM || !rat_eq(p->kids[0]->r, rat_int(10))) return false;
    const Expr *x = p->kids[1];
    if (x->kind == E_NEG) {
        if (x->kids[0]->kind != E_NUM || x->kids[0]->r.d != 1) return false;
        *n = (int)-x->kids[0]->r.n;
    } else {
        if (x->kind != E_NUM || x->r.d != 1 || x->r.n > 300 || x->r.n < -300) return false;
        *n = (int)x->r.n;
    }
    return true;
}

static bool std_ok(Rat a) { Rat b = rat_abs(a); return rat_cmp(b, rat_int(1)) >= 0 && rat_cmp(b, rat_int(10)) < 0; }

/* value -> "a×10^n" text (exact mantissa when it terminates, else rounded) */
static const char *to_std(double v, const Rat *exact, const CalcSettings *s, int *nout)
{
    int n = (int)floor(log10(fabs(v)));
    Rat a, p;
    if (exact) {
        for (int tries = 0; tries < 3; tries++) {
            if (!rat_pow_int(rat_int(10), n, &p) || !rat_div(*exact, p, &a)) break;
            if (rat_cmp(rat_abs(a), rat_int(1)) < 0) { n--; continue; }
            if (rat_cmp(rat_abs(a), rat_int(10)) >= 0) { n++; continue; }
            const char *d = cu_exact_dec(a);
            if (d && strlen(d) <= 18) { *nout = n; return sci(d, n); }
            break;
        }
    }
    double m = v / pow(10, n);
    CalcSettings def;
    calc_settings_default(&def);
    if (!s) s = &def;
    CalcSettings t = *s;
    if (t.dp < 0 && t.sf <= 0) t.sf = 10;
    const char *ms = cu_fmt_dec(m, &t, NULL);
    if (fabs(strtod(ms, NULL)) >= 10) { n++; ms = cu_fmt_dec(v / pow(10, n), &t, NULL); }
    char buf[64];
    snprintf(buf, sizeof buf, "%s", ms);
    if (t.dp < 0) strip_zeros(buf);
    *nout = n;
    return sci(buf, n);
}

/* Already a×10^n with 1 <= |a| < 10: the answer stays in standard form; the
 * ordinary number (when it is short enough to write out) is the other form. */
static void std_already(const Expr *t, Rat a, int n, CalcResult *out)
{
    const char *m = cu_exact_dec(a);
    if (!m) m = cu_rat_str(a);
    const char *ans = sci(m, n), *typed = ex_print(t);
    steps_add(&out->steps, "Already in standard form", "The first number is from 1 to 10, times a power of 10",
              strcmp(typed, ans) ? "%s=%s" : NULL, typed, ans);
    const char *d = cu_shift_point(m, n);
    if (d)
        steps_add(&out->steps, asprintf_a("Move the decimal point %d place%s %s", abs(n), abs(n) == 1 ? "" : "s",
                                          n >= 0 ? "right" : "left"),
                  n >= 0 ? "As an ordinary number: a positive power makes it bigger"
                         : "As an ordinary number: a negative power makes it smaller",
                  "%s=%s", ans, d);
    out->answer = ans;
    out->answer_alt = d ? d : "";
    out->value = rat_to_double(a) * pow(10, n);
    out->has_value = true;
}

void calc_std_form(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    Rat a, val;
    int n;
    if (cu_std_parts(t, &a, &n) && std_ok(a)) { std_already(t, a, n, out); return; }
    if (!cu_domain_ok(t, s->degrees, out)) return;
    CVal v;
    if (!cu_value(t, s->degrees, &v)) { cu_fail(out, "Maths error"); return; }
    if (v.v == 0) { out->answer = "0"; out->has_value = true; return; }
    bool exact = v.exact && cu_rational(v.e, &val);
    cu_std_muldiv_steps(t, &out->steps, &a, &n);
    int ne;
    const char *ans = to_std(v.v, exact ? &val : NULL, s, &ne);
    const char *plain = exact && cu_exact_dec(val) ? cu_exact_dec(val) : cu_fmt_dec(v.v, s, NULL);
    steps_add(&out->steps, "Write as a number from 1 to 10 times a power of 10",
              asprintf_a("The decimal point moves %d place%s, so the power is %d", abs(ne), abs(ne) == 1 ? "" : "s", ne),
              "%s=%s", plain, ans);
    out->answer = ans;
    out->answer_alt = strcmp(plain, ans) ? plain : "";
    out->value = v.v;
    out->has_value = true;
}
