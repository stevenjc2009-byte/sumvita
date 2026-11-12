/* Internal to calc*.c, next to calc_util.h: working shared by an action and "=".
 *
 *   calc_number.c   cu_std_parts, cu_shift_point
 *   calc_stdwork.c  standard-form arithmetic working (× ÷ + −)
 *   calc_surd_exp.c surd brackets as written, expanding (a+√b)(c+√d) and (a+√b)^2
 *   calc_surd_act.c cu_surd_working: rationalise / expand, used by Surds and "="
 *   calc_oneline.c  one line of working for a lone sign / decimal sum
 * Everything allocates in the current arena. */
#ifndef ENGINE_CALC_MORE_H
#define ENGINE_CALC_MORE_H

#include "calc_util.h"

/* ---- calc_number.c ---- */
bool cu_std_parts(const Expr *e, Rat *a, int *n);       /* a×10^n (or 10^n), n an integer */
const char *cu_shift_point(const char *m, int n);       /* "6.02", 3 -> "6020"; NULL if too long */

/* ---- calc_stdwork.c ---- */
/* (a×10^n)×(b×10^m) or ÷: numbers then powers. true if steps were written;
 * *m×10^*p is the result (m not yet between 1 and 10). */
bool cu_std_muldiv_steps(const Expr *t, Steps *st, Rat *m, int *p);
/* "=" on two standard-form numbers (× ÷ + −): GCSE working ending with the
 * ordinary number. true if steps were written. */
bool cu_std_working(const Expr *t, Steps *st);

/* ---- calc_surd_exp.c ---- */
/* The terms of a bracket (or a single term) as written, in order, each c·√b with
 * b squarefree. Returns the count (1..max), 0 if it is not that shape. */
int cu_surd_terms(const Expr *e, SurdTerm *w, int max);
const char *cu_terms_text(const SurdTerm *w, int n);    /* "√3+1", "3-2√5" */
const char *cu_term_text(SurdTerm t);
/* (a+√b)(c+√d), k(a+√b), (a+√b)^2: expansion working. true if steps were written. */
bool cu_surd_expand(const Expr *t, const SurdSum *whole, Steps *st);

/* ---- calc_surd_act.c ---- */
/* Rationalise or expand: 1 steps written, 0 not that shape, -1 numbers too big.
 * A maths error (bottom is 0) sets out->ok = false and returns 1. */
int cu_surd_working(const Expr *t, Steps *st, CalcResult *out);

/* ---- calc_oneline.c ---- */
/* One step for a single operation the BIDMAS walk would not show: a negative
 * squared, -3^2, signs when multiplying, a decimal with a fraction, decimal × ÷. */
bool cu_one_line(const Expr *t, bool degrees, Steps *st);

#endif
