/* Algebra operations called by calc.c. Each writes its working into `st`
 * (may be NULL) and its answer into *out (arena). On failure returns false
 * with a short user-facing message in err ("Can't factorise this any further"
 * is NOT a failure: return true with the input unchanged and a step saying so).
 *
 * Owners:
 *   expand.c, factor.c  -> alg_expand, alg_factor
 *   solve.c (+ helpers) -> alg_solve, alg_complete_square, alg_rearrange
 */
#ifndef ENGINE_ALGEBRA_H
#define ENGINE_ALGEBRA_H

#include <stdbool.h>
#include "calc.h"
#include "expr.h"
#include "steps.h"

bool alg_expand(const Expr *e, Steps *st, Expr **out, char *err, int errcap);
bool alg_factor(const Expr *e, Steps *st, Expr **out, char *err, int errcap);
/* Algebraic fraction P/Q (or a sum of them): factorise top and bottom, cancel common
 * factors, combine sums over a common denominator. Owner: algfrac.c (expand/factor agent).
 * Returns false only on error; nothing to cancel = true with the input tidied. */
bool alg_simplify_fraction(const Expr *e, Steps *st, Expr **out, char *err, int errcap);

/* e: an equation, inequality (incl. chains), or an E_LIST system of equations.
 * out: solutions, e.g. LIST(REL(x=2), REL(x=-3)), or x=(3±√5)/2 forms,
 * or an inequality / interval set, or LIST(x=.., y=..). */
bool alg_solve(const Expr *e, const CalcSettings *s, Steps *st, Expr **out, char *err, int errcap);
bool alg_complete_square(const Expr *e, Steps *st, Expr **out, char *err, int errcap);
bool alg_rearrange(const Expr *e, const char *var, Steps *st, Expr **out, char *err, int errcap);

#endif
