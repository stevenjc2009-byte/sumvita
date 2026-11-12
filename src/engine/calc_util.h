/* Internal to calc*.c: the pieces behind calc_run() (calc.h).
 *
 *   calc.c          calc_run dispatcher, AUTO, algebra hand-off, Simplify, small helpers
 *   calc_eval.c     values (exact or decimal), maths-error checks, Evaluate, answer text
 *   calc_steps.c    BIDMAS working for Evaluate (one step per operation)
 *   calc_work.c     special working: fractions, indices, surds, % of, nCr, n!
 *   calc_trig.c     Ans substitution, exact trig (degrees), inverse trig of exact values
 *   calc_surd.c     SurdSum: exact sums of rational multiples of square roots
 *   calc_surd_act.c the Surds action: simplify, rationalise the denominator
 *   calc_convert.c  -> Fraction (incl. recurring decimals), -> Mixed
 *   calc_convert2.c -> Decimal, -> %
 *   calc_number.c   decimal formatting (dp / sf), prime factors, standard form
 * Everything allocates in the current arena. */
#ifndef ENGINE_CALC_UTIL_H
#define ENGINE_CALC_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "calc.h"
#include "expr.h"
#include "num.h"
#include "steps.h"

/* ---- calc.c ---- */
void cu_fail(CalcResult *out, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
const char *cu_rat_str(Rat r);          /* "7/22", "-3" */
Expr *cu_rat_disp(Rat r);               /* display tree: n, n/d as E_DIV, NEG for negatives */
Expr *cu_pretty(const Expr *e);         /* (3/2)π -> 3π/2, (2/3)√3 -> 2√3/3 (after ex_tidy) */
bool cu_has_decimal(const Expr *e);     /* a typed decimal anywhere */
void calc_simplify(const Expr *t, const CalcSettings *s, CalcResult *out);

/* ---- calc_eval.c ---- */
typedef struct {
    Expr *e;        /* display-ready value: exact tree, or E_REAL */
    bool exact;
    double v;
} CVal;

bool cu_value(const Expr *t, bool degrees, CVal *out);   /* false: no value (maths error) */
bool cu_rational(const Expr *t, Rat *r);                 /* exact rational value of a numeric tree */
bool cu_domain_ok(const Expr *t, bool degrees, CalcResult *out);
/* dec_pref: a decimal was typed (answer as a decimal when exact); mixed_pref: a
 * mixed number was typed (an improper fraction answer is shown as a mixed number) */
void cu_set_answer(const CVal *v, bool dec_pref, bool mixed_pref, const CalcSettings *s, CalcResult *out);
void calc_eval(const Expr *t, const CalcSettings *s, CalcResult *out);

/* ---- calc_steps.c / calc_work.c ---- */
void calc_eval_steps(const Expr *t, bool degrees, Steps *st);
/* Special working for one operation. kids = values of e's kids. true if steps were written. */
bool cw_node(const Expr *e, const CVal *kids, int nk, const CVal *val, bool under_mul, Steps *st);
bool cw_surd_step(int64_t n, Steps *st);                 /* √72 = √(36×2) = 6√2 */
bool cw_simplify_hcf(Rat shown_n, Rat shown_d, Steps *st);/* 375/1000 = 3/8, HCF 125 */

/* ---- calc_trig.c ---- */
/* Ans -> value, exact trig values. NULL on a maths error (out->error set). */
Expr *cu_prepare(const Expr *e, const CalcSettings *s, CalcResult *out);

/* ---- calc_surd.c ---- */
#define SS_MAX 8
typedef struct { Rat c; int64_t b; } SurdTerm;          /* c·√b, b squarefree, b = 1: rational */
typedef struct { SurdTerm t[SS_MAX]; int n; } SurdSum;  /* n = 0: zero */

bool ss_from_expr(const Expr *e, SurdSum *out);
Expr *ss_to_expr(const SurdSum *s);
bool ss_rational(const SurdSum *s, Rat *r);
double ss_value(const SurdSum *s);
bool ss_add(const SurdSum *a, const SurdSum *b, SurdSum *out);
bool ss_mul(const SurdSum *a, const SurdSum *b, SurdSum *out);
bool ss_div(const SurdSum *a, const SurdSum *b, SurdSum *out);
void ss_rat(Rat r, SurdSum *out);
bool ss_has_surd(const SurdSum *s);
bool cu_sqrt_split(int64_t n, int64_t *k, int64_t *m);   /* n = k²·m, m squarefree (best effort) */
void calc_surd(const Expr *t, const CalcSettings *s, CalcResult *out);

/* ---- roots_eq.c ---- */
/* "x=-2 and x=-3" -> x^2+5x+6=0 with working. false: not that form (out untouched). */
bool calc_roots_eq(const Expr *t, CalcResult *out);

/* ---- calc_convert*.c ---- */
void calc_to_fraction(const Expr *t, const CalcSettings *s, CalcResult *out);
void calc_to_mixed(const Expr *t, const CalcSettings *s, CalcResult *out);
void calc_to_decimal(const Expr *t, const CalcSettings *s, CalcResult *out);
void calc_to_percent(const Expr *t, const CalcSettings *s, CalcResult *out);

/* ---- calc_number.c ---- */
const char *cu_fmt_dec(double v, const CalcSettings *s, bool *rounded);
const char *cu_round_title(const CalcSettings *s);       /* "Round to 10 significant figures" */
const char *cu_exact_dec(Rat r);                         /* "0.375"; NULL if it does not terminate */
void calc_prime_factors(const Expr *t, CalcResult *out);
void calc_std_form(const Expr *t, const CalcSettings *s, CalcResult *out);

#endif
