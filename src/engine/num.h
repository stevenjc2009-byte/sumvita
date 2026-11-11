/* Exact rational numbers on int64, with overflow detection.
 *
 * Every operation returns false on overflow (or divide by zero) and leaves
 * *out untouched; the caller then falls back to double. A Rat is always
 * normalised: d > 0, gcd(|n|, d) == 1, zero is 0/1.
 */
#ifndef ENGINE_NUM_H
#define ENGINE_NUM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t n;
    int64_t d;
} Rat;

#define RAT(n_, d_) rat_make((n_), (d_))

Rat     rat_make(int64_t n, int64_t d);     /* normalises; d==0 gives 0/1 */
Rat     rat_int(int64_t n);
bool    rat_is_int(Rat a);
bool    rat_is_zero(Rat a);
bool    rat_is_one(Rat a);
bool    rat_is_neg(Rat a);
int     rat_cmp(Rat a, Rat b);              /* -1, 0, 1 (exact, via __int128-free cross mult with overflow fallback) */
bool    rat_eq(Rat a, Rat b);

bool rat_add(Rat a, Rat b, Rat *out);
bool rat_sub(Rat a, Rat b, Rat *out);
bool rat_mul(Rat a, Rat b, Rat *out);
bool rat_div(Rat a, Rat b, Rat *out);       /* false if b == 0 */
bool rat_pow_int(Rat a, int64_t e, Rat *out); /* negative e allowed; 0^neg false */
Rat  rat_neg(Rat a);
Rat  rat_abs(Rat a);
bool rat_inv(Rat a, Rat *out);

double rat_to_double(Rat a);

/* Exact conversion of a finite decimal string ("3.25", "-0.125", ".5", "12").
 * false if it does not fit. */
bool rat_from_decimal_str(const char *s, Rat *out);

/* Best rational approximation with denominator <= max_den (continued fractions).
 * Used to recover fractions from doubles: true only when |x - n/d| < 1e-12*max(1,|x|). */
bool rat_from_double(double x, int64_t max_den, Rat *out);

/* Exact root: true when a is a perfect k-th power of a rational (k >= 2). */
bool rat_root_exact(Rat a, int k, Rat *out);

/* If the decimal expansion terminates, true. */
bool rat_terminates(Rat a);

/* Integer helpers. */
int64_t i64_gcd(int64_t a, int64_t b);      /* always >= 0; gcd(0,0) = 0 */
bool    i64_lcm(int64_t a, int64_t b, int64_t *out);
bool    i64_mul_ok(int64_t a, int64_t b, int64_t *out);
bool    i64_add_ok(int64_t a, int64_t b, int64_t *out);
bool    i64_isqrt_exact(int64_t a, int64_t *root);   /* a >= 0 perfect square */
int64_t i64_isqrt(int64_t a);                        /* floor sqrt, a >= 0 */

/* Prime factorisation of |n| (n >= 2). Fills up to max pairs; returns count. */
int i64_factorize(int64_t n, int64_t *primes, int *exps, int max);

/* Formatting into caller buffers (ASCII, no locale). */
void rat_to_str(Rat a, char *buf, int cap);           /* "-7/2", "3" */
/* Decimal text of a rational: exact if it terminates within max_dp digits,
 * otherwise rounded to max_dp decimal places and trailing zeros removed. */
void rat_to_decimal_str(Rat a, int max_dp, char *buf, int cap);
/* Recurring form, e.g. 1/7 -> "0.(142857)". false if period > 60 digits. */
bool rat_to_recurring_str(Rat a, char *buf, int cap);

/* Double formatting the calculator way: up to sig significant figures,
 * scientific "1.23×10^15" style handled by the caller; this gives "%.*g"-like
 * text without "e" (uses "E" only if |x| >= 1e15 or < 1e-9). */
void dbl_to_str(double x, int sig, char *buf, int cap);

#endif
