/* Multivariate polynomials with exact rational coefficients.
 *
 * Variables are kept in alphabetical order (v[0..nv-1]); terms are sorted
 * lexicographically by exponent vector, highest first, like terms combined,
 * no zero coefficients, and variables that no term uses are dropped.
 * Storage is arena-allocated (arena.h). Every function that can overflow
 * int64 (or exceed POLY_MAXV variables / POLY_MAXTERMS terms / POLY_MAXEXP)
 * returns false and leaves *out unusable; callers fall back to other methods.
 */
#ifndef ENGINE_POLY_H
#define ENGINE_POLY_H

#include <stdbool.h>
#include "expr.h"
#include "num.h"

#define POLY_MAXV 8
#define POLY_MAXTERMS 4096
#define POLY_MAXEXP 1000

typedef struct {
    Rat c;
    unsigned short e[POLY_MAXV];   /* exponent of v[i]; unused slots are 0 */
} PolyTerm;

typedef struct {
    int nv;
    char v[POLY_MAXV][12];
    int n;                         /* number of terms; 0 = the zero polynomial */
    PolyTerm *t;
} Poly;

/* ---- construction and conversion (poly.c) ---- */
/* The zero polynomial. */
Poly poly_zero(void);
/* The constant polynomial c. */
Poly poly_const(Rat c);
/* The polynomial consisting of one variable. */
Poly poly_var(const char *name);
/* Converts an expression; false for anything that is not a polynomial with exact coefficients (REAL, π, e, functions, x^-1, division by a variable, ...). */
bool poly_from_expr(const Expr *e, Poly *out);
/* Builds a canonical-style tree (coefficient first, negative coefficients as negative numbers), terms by descending degree in mainvar (NULL: lexicographic) then alphabetically; pass through ex_tidy for display. */
Expr *poly_to_expr(const Poly *p, const char *mainvar);

/* ---- arithmetic and inspection (poly_ops.c) ---- */
/* out = a + b. */
bool poly_add(const Poly *a, const Poly *b, Poly *out);
/* out = a - b. */
bool poly_sub(const Poly *a, const Poly *b, Poly *out);
/* out = a × b. */
bool poly_mul(const Poly *a, const Poly *b, Poly *out);
/* out = a^k for 0 <= k <= POLY_MAXEXP. */
bool poly_pow(const Poly *a, int k, Poly *out);
/* out = k × a. */
bool poly_scale(const Poly *a, Rat k, Poly *out);
/* out = -a. */
bool poly_neg(const Poly *a, Poly *out);
/* True when a and b are the same polynomial. */
bool poly_equal(const Poly *a, const Poly *b);
/* True for the zero polynomial. */
bool poly_is_zero(const Poly *p);
/* True when p has no variables; stores its value in *c if c is not NULL. */
bool poly_is_const(const Poly *p, Rat *c);
/* Highest total degree of any term; -1 for the zero polynomial. */
int poly_total_degree(const Poly *p);
/* Highest power of var; 0 if var does not occur, -1 for the zero polynomial. */
int poly_degree(const Poly *p, const char *var);
/* out = coefficient of var^k (a polynomial in the other variables). */
bool poly_coeff(const Poly *p, const char *var, int k, Poly *out);
/* Substitutes var = x; out is a polynomial in the remaining variables. */
bool poly_eval(const Poly *p, const char *var, Rat x, Poly *out);

/* ---- factors and division (poly_div.c) ---- */
/* Rational content: positive gcd(numerators)/lcm(denominators), so p/content has coprime integer coefficients; 0 for the zero polynomial. */
bool poly_content(const Poly *p, Rat *out);
/* Greatest common monomial factor (coefficient 1, lowest power of each variable); 1 for the zero polynomial. */
Poly poly_monomial_gcd(const Poly *p);
/* out = p / m for a single-term m that divides every term; false otherwise. */
bool poly_div_monomial(const Poly *p, const Poly *m, Poly *out);
/* out = a / b when b divides a exactly; false when it does not (or b is zero). */
bool poly_div_exact(const Poly *a, const Poly *b, Poly *out);

/* ---- univariate view: c[i] is the coefficient of var^i, deg is the highest i (poly_div.c) ---- */
/* Coefficients of p in var; false if p has another variable or degree > maxdeg. Zero gives deg 0, c[0] = 0. */
bool poly_to_upoly(const Poly *p, const char *var, Rat *c, int maxdeg, int *deg);
/* The polynomial c[deg]·var^deg + ... + c[0]. */
Poly poly_from_upoly(const Rat *c, int deg, const char *var);
/* *out = value at x (Horner): the remainder theorem's f(a). */
bool upoly_eval(const Rat *c, int deg, Rat x, Rat *out);
/* a = b·q + r with deg r < deg b; b[db] must be non-zero. q needs da-db+1 slots (1 if da < db), r needs db slots (1 if db == 0). */
bool upoly_divmod(const Rat *a, int da, const Rat *b, int db, Rat *q, int *dq, Rat *r, int *dr);
/* Synthetic division by (var - root): q gets da coefficients (degree da-1), *rem = a(root). Needs da >= 1. */
bool upoly_div_linear(const Rat *a, int da, Rat root, Rat *q, Rat *rem);

#endif
