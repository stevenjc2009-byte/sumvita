/* Internal to poly*.c. */
#ifndef ENGINE_POLY_INT_H
#define ENGINE_POLY_INT_H

#include "poly.h"

/* Arena array of n terms (at least 1), zeroed. */
PolyTerm *poly_terms(int n);
/* Sort, combine like terms, drop zeros and unused variables. false on overflow / too many terms. */
bool poly_norm(Poly *p);
/* Rewrites a and b over the union of their variables. false if more than POLY_MAXV. */
bool poly_unify(const Poly *a, const Poly *b, Poly *a2, Poly *b2);
/* Index of name in p->v, or -1. */
int poly_var_index(const Poly *p, const char *name);
/* Lexicographic comparison of exponent vectors: >0 when a is higher. */
int poly_term_cmp(const PolyTerm *a, const PolyTerm *b);

#endif
