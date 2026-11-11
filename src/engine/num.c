#include "num.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int64_t i64_gcd(int64_t a, int64_t b)
{
    uint64_t x = a < 0 ? (uint64_t)0 - (uint64_t)a : (uint64_t)a;
    uint64_t y = b < 0 ? (uint64_t)0 - (uint64_t)b : (uint64_t)b;
    while (y) {
        uint64_t t = x % y;
        x = y;
        y = t;
    }
    return (int64_t)x;
}

bool i64_mul_ok(int64_t a, int64_t b, int64_t *out) { return !__builtin_mul_overflow(a, b, out); }
bool i64_add_ok(int64_t a, int64_t b, int64_t *out) { return !__builtin_add_overflow(a, b, out); }

bool i64_lcm(int64_t a, int64_t b, int64_t *out)
{
    if (a == 0 || b == 0) { *out = 0; return true; }
    int64_t g = i64_gcd(a, b);
    int64_t r;
    if (!i64_mul_ok(llabs(a) / g, llabs(b), &r)) return false;
    *out = r;
    return true;
}

int64_t i64_isqrt(int64_t a)
{
    if (a <= 0) return 0;
    int64_t r = (int64_t)sqrt((double)a);
    while (r > 0 && (r > a / r)) r--;
    while ((r + 1) <= a / (r + 1)) r++;
    return r;
}

bool i64_isqrt_exact(int64_t a, int64_t *root)
{
    if (a < 0) return false;
    int64_t r = i64_isqrt(a);
    if (r * r != a) return false;
    if (root) *root = r;
    return true;
}

int i64_factorize(int64_t n, int64_t *primes, int *exps, int max)
{
    if (n < 0) n = -n;
    int k = 0;
    for (int64_t p = 2; p <= n / p && k < max; p += (p == 2 ? 1 : 2)) {
        if (n % p) continue;
        int e = 0;
        while (n % p == 0) { n /= p; e++; }
        primes[k] = p;
        exps[k] = e;
        k++;
    }
    if (n > 1 && k < max) { primes[k] = n; exps[k] = 1; k++; }
    return k;
}

Rat rat_make(int64_t n, int64_t d)
{
    Rat r = {0, 1};
    if (d == 0 || n == 0) return r;
    if (d < 0) {
        if (n == INT64_MIN || d == INT64_MIN) {   /* cannot negate: approximate by halving */
            n /= 2; d /= 2;
            if (d == 0) return r;
        }
        n = -n; d = -d;
    }
    int64_t g = i64_gcd(n, d);
    r.n = n / g;
    r.d = d / g;
    return r;
}

Rat  rat_int(int64_t n) { Rat r = {n, 1}; return r; }
bool rat_is_int(Rat a) { return a.d == 1; }
bool rat_is_zero(Rat a) { return a.n == 0; }
bool rat_is_one(Rat a) { return a.n == 1 && a.d == 1; }
bool rat_is_neg(Rat a) { return a.n < 0; }
bool rat_eq(Rat a, Rat b) { return a.n == b.n && a.d == b.d; }
Rat  rat_neg(Rat a) { if (a.n != INT64_MIN) a.n = -a.n; return a; }
Rat  rat_abs(Rat a) { return a.n < 0 ? rat_neg(a) : a; }
double rat_to_double(Rat a) { return (double)a.n / (double)a.d; }

int rat_cmp(Rat a, Rat b)
{
    int64_t l, r;
    if (i64_mul_ok(a.n, b.d, &l) && i64_mul_ok(b.n, a.d, &r))
        return l < r ? -1 : (l > r ? 1 : 0);
    double x = rat_to_double(a), y = rat_to_double(b);
    return x < y ? -1 : (x > y ? 1 : 0);
}

bool rat_add(Rat a, Rat b, Rat *out)
{
    int64_t g = i64_gcd(a.d, b.d);
    int64_t da = a.d / g, db = b.d / g;
    int64_t t1, t2, n, d;
    if (!i64_mul_ok(a.n, db, &t1) || !i64_mul_ok(b.n, da, &t2)) return false;
    if (!i64_add_ok(t1, t2, &n)) return false;
    if (!i64_mul_ok(a.d, db, &d)) return false;
    *out = rat_make(n, d);
    return true;
}

bool rat_sub(Rat a, Rat b, Rat *out)
{
    if (b.n == INT64_MIN) return false;
    return rat_add(a, rat_neg(b), out);
}

bool rat_mul(Rat a, Rat b, Rat *out)
{
    int64_t g1 = i64_gcd(a.n, b.d), g2 = i64_gcd(b.n, a.d);
    if (g1 == 0) g1 = 1;
    if (g2 == 0) g2 = 1;
    int64_t n, d;
    if (!i64_mul_ok(a.n / g1, b.n / g2, &n)) return false;
    if (!i64_mul_ok(a.d / g2, b.d / g1, &d)) return false;
    *out = rat_make(n, d);
    return true;
}

bool rat_inv(Rat a, Rat *out)
{
    if (a.n == 0 || a.n == INT64_MIN) return false;
    *out = rat_make(a.d, a.n);
    return true;
}

bool rat_div(Rat a, Rat b, Rat *out)
{
    Rat ib;
    if (!rat_inv(b, &ib)) return false;
    return rat_mul(a, ib, out);
}

bool rat_pow_int(Rat a, int64_t e, Rat *out)
{
    if (e < 0) {
        Rat ia;
        if (!rat_inv(a, &ia)) return false;
        if (e == INT64_MIN) return false;
        return rat_pow_int(ia, -e, out);
    }
    Rat r = rat_int(1), b = a;
    while (e) {
        if (e & 1) { if (!rat_mul(r, b, &r)) return false; }
        e >>= 1;
        if (e && !rat_mul(b, b, &b)) return false;
    }
    *out = r;
    return true;
}

static bool i64_root_exact(int64_t a, int k, int64_t *root)
{
    if (a < 0) {
        if (k % 2 == 0) return false;
        int64_t r;
        if (!i64_root_exact(-a, k, &r)) return false;
        *root = -r;
        return true;
    }
    if (a <= 1) { *root = a; return true; }
    int64_t r = (int64_t)llround(pow((double)a, 1.0 / k));
    for (int64_t c = r - 1; c <= r + 1; c++) {
        if (c < 0) continue;
        Rat p;
        if (rat_pow_int(rat_int(c), k, &p) && p.n == a) { *root = c; return true; }
    }
    return false;
}

bool rat_root_exact(Rat a, int k, Rat *out)
{
    int64_t rn, rd;
    if (k < 2) return false;
    if (!i64_root_exact(a.n, k, &rn) || !i64_root_exact(a.d, k, &rd)) return false;
    *out = rat_make(rn, rd);
    return true;
}

bool rat_terminates(Rat a)
{
    int64_t d = a.d;
    while (d % 2 == 0) d /= 2;
    while (d % 5 == 0) d /= 5;
    return d == 1;
}

bool rat_from_decimal_str(const char *s, Rat *out)
{
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    int64_t n = 0, d = 1;
    int seen_dot = 0, digits = 0;
    for (; *s; s++) {
        if (*s == '.') {
            if (seen_dot) return false;
            seen_dot = 1;
            continue;
        }
        if (*s < '0' || *s > '9') return false;
        if (!i64_mul_ok(n, 10, &n) || !i64_add_ok(n, *s - '0', &n)) return false;
        if (seen_dot && !i64_mul_ok(d, 10, &d)) return false;
        digits++;
    }
    if (!digits) return false;
    *out = rat_make(neg ? -n : n, d);
    return true;
}

bool rat_from_double(double x, int64_t max_den, Rat *out)
{
    if (!isfinite(x) || fabs(x) > 9.0e15) return false;
    int neg = x < 0;
    double v = fabs(x);
    int64_t p0 = 0, q0 = 1, p1 = 1, q1 = 0;
    double f = v;
    for (int i = 0; i < 64; i++) {
        double a = floor(f);
        if (a > 9.0e15) break;
        int64_t ai = (int64_t)a;
        int64_t p2, q2, t;
        if (!i64_mul_ok(ai, p1, &t) || !i64_add_ok(t, p0, &p2)) break;
        if (!i64_mul_ok(ai, q1, &t) || !i64_add_ok(t, q0, &q2)) break;
        if (q2 > max_den) break;
        p0 = p1; q0 = q1; p1 = p2; q1 = q2;
        double err = fabs(v - (double)p1 / (double)q1);
        if (err < 1e-12 * (v > 1 ? v : 1)) {
            *out = rat_make(neg ? -p1 : p1, q1);
            return true;
        }
        double frac = f - a;
        if (frac < 1e-15) break;
        f = 1.0 / frac;
    }
    return false;
}

void rat_to_str(Rat a, char *buf, int cap)
{
    if (a.d == 1) snprintf(buf, (size_t)cap, "%lld", (long long)a.n);
    else snprintf(buf, (size_t)cap, "%lld/%lld", (long long)a.n, (long long)a.d);
}

void rat_to_decimal_str(Rat a, int max_dp, char *buf, int cap)
{
    if (max_dp < 0) max_dp = 0;
    if (max_dp > 30) max_dp = 30;
    char tmp[96];
    int pos = 0;
    uint64_t n = a.n < 0 ? (uint64_t)0 - (uint64_t)a.n : (uint64_t)a.n;
    uint64_t d = (uint64_t)a.d;
    uint64_t ip = n / d, rem = n % d;
    char digs[40];
    int nd = 0;
    for (int i = 0; i < max_dp + 1 && rem; i++) {
        /* rem < d <= INT64_MAX, so rem*10 fits in uint64 */
        rem *= 10;
        digs[nd++] = (char)('0' + rem / d);
        rem %= d;
    }
    /* round half up on digit max_dp+1 */
    if (nd > max_dp) {
        int up = digs[max_dp] >= '5';
        nd = max_dp;
        for (int i = nd - 1; up && i >= 0; i--) {
            if (digs[i] == '9') digs[i] = '0';
            else { digs[i]++; up = 0; }
        }
        if (up) ip++;
    }
    while (nd > 0 && digs[nd - 1] == '0') nd--;
    int is_zero = (ip == 0 && nd == 0);
    if (a.n < 0 && !is_zero) tmp[pos++] = '-';
    pos += snprintf(tmp + pos, sizeof tmp - (size_t)pos, "%llu", (unsigned long long)ip);
    if (nd > 0) {
        tmp[pos++] = '.';
        memcpy(tmp + pos, digs, (size_t)nd);
        pos += nd;
    }
    tmp[pos] = '\0';
    snprintf(buf, (size_t)cap, "%s", tmp);
}

bool rat_to_recurring_str(Rat a, char *buf, int cap)
{
    uint64_t n = a.n < 0 ? (uint64_t)0 - (uint64_t)a.n : (uint64_t)a.n;
    uint64_t d = (uint64_t)a.d;
    if (d > 100000000ull) return false;       /* keep the remainder table small */
    uint64_t ip = n / d, rem = n % d;
    enum { MAXD = 80 };
    uint64_t seen_rem[MAXD + 1];
    char digs[MAXD + 1];
    int nd = 0, start = -1;
    while (rem && nd < MAXD) {
        for (int i = 0; i < nd; i++)
            if (seen_rem[i] == rem) { start = i; break; }
        if (start >= 0) break;
        seen_rem[nd] = rem;
        rem *= 10;
        digs[nd++] = (char)('0' + rem / d);
        rem %= d;
    }
    if (rem && start < 0) return false;        /* period too long */
    if (start >= 0 && nd - start > 60) return false;
    char out[160];
    int p = snprintf(out, sizeof out, "%s%llu", a.n < 0 ? "-" : "", (unsigned long long)ip);
    if (nd) {
        out[p++] = '.';
        for (int i = 0; i < nd; i++) {
            if (i == start) out[p++] = '(';
            out[p++] = digs[i];
        }
        if (start >= 0) out[p++] = ')';
    }
    out[p] = '\0';
    snprintf(buf, (size_t)cap, "%s", out);
    return true;
}

void dbl_to_str(double x, int sig, char *buf, int cap)
{
    if (isnan(x)) { snprintf(buf, (size_t)cap, "undefined"); return; }
    if (isinf(x)) { snprintf(buf, (size_t)cap, x > 0 ? "infinity" : "-infinity"); return; }
    if (sig < 1) sig = 1;
    if (sig > 15) sig = 15;
    if (x == 0) { snprintf(buf, (size_t)cap, "0"); return; }
    double ax = fabs(x);
    char tmp[64];
    if (ax >= 1e15 || ax < 1e-9) {
        snprintf(tmp, sizeof tmp, "%.*E", sig - 1, x);
        /* strip zeros in the mantissa: 1.230000E+05 -> 1.23E+05 */
        char *e = strchr(tmp, 'E');
        if (e) {
            char mant[40], ex[24];
            size_t ml = (size_t)(e - tmp);
            memcpy(mant, tmp, ml); mant[ml] = '\0';
            snprintf(ex, sizeof ex, "%s", e);
            if (strchr(mant, '.')) {
                size_t l = strlen(mant);
                while (l && mant[l - 1] == '0') mant[--l] = '\0';
                if (l && mant[l - 1] == '.') mant[--l] = '\0';
            }
            int ev = atoi(ex + 1);
            snprintf(buf, (size_t)cap, "%sE%d", mant, ev);
            return;
        }
    }
    snprintf(tmp, sizeof tmp, "%.*g", sig, x);
    if (strchr(tmp, 'e')) {                 /* %g chose exponent form inside range: redo fixed */
        int dp = sig - 1 - (int)floor(log10(ax));
        if (dp < 0) dp = 0;
        if (dp > 20) dp = 20;
        snprintf(tmp, sizeof tmp, "%.*f", dp, x);
        if (strchr(tmp, '.')) {
            size_t l = strlen(tmp);
            while (l && tmp[l - 1] == '0') tmp[--l] = '\0';
            if (l && tmp[l - 1] == '.') tmp[--l] = '\0';
        }
    }
    if (strcmp(tmp, "-0") == 0) snprintf(tmp, sizeof tmp, "0");
    snprintf(buf, (size_t)cap, "%s", tmp);
}
