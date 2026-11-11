/* Internal to the simplifier (simplify*.c). Canonical form:
 *   ADD: flat, like terms collected, numeric constant last, terms ordered by
 *        degree (highest first) then alphabetically.
 *   MUL: flat, at most one numeric coefficient (first), then numeric-base
 *        powers (surds), constants, variables alphabetically, then the rest.
 *   POW: x^0, x^1, 1^x folded; numeric powers exact; √n -> a·b^(1/2).
 *   No DIV, NEG, SQRT/ROOT, PCT, MIXED, RECUR (all rewritten). */
#ifndef ENGINE_SIMPLIFY_INT_H
#define ENGINE_SIMPLIFY_INT_H

#include <stdbool.h>
#include "expr.h"

/* A number that is exact (Rat) until an operation overflows, then double. */
typedef struct {
    bool exact;
    bool dec;       /* came from a typed decimal: keep NF_DECIMAL on the result */
    Rat r;
    double v;
} Num;

bool  is_numexpr(const Expr *e);          /* E_NUM or E_REAL */
Num   num_of(const Expr *e);              /* e must be is_numexpr */
Num   num_rat(Rat r);
Num   num_add(Num a, Num b);
Num   num_mul(Num a, Num b);
double num_val(Num a);
bool  num_is(Num a, int64_t k);           /* exactly the integer k (1.0 counts for inexact) */
Expr *num_expr(Num a);

/* Kids are already simplified. Arrays are not modified. */
Expr *simp_add_list(Expr **kids, int n);
Expr *simp_mul_list(Expr **kids, int n);
Expr *simp_pow(Expr *base, Expr *exp);

/* Split a simplified term into numeric coefficient and the rest (NULL if none). */
Num   term_split(Expr *t, Expr **rest);

#endif
