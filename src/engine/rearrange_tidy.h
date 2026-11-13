/* Final tidy pass on a rearranged answer (rearrange_tidy.c). */
#ifndef ENGINE_REARRANGE_TIDY_H
#define ENGINE_REARRANGE_TIDY_H

#include "expr.h"

/* If K is a quotient that can be written more simply (nested fractions cleared,
 * common factors cancelled or taken out, products in the print order), returns the
 * tidy tree; otherwise NULL. The value of the tidy tree equals K. */
Expr *rr_tidy(const Expr *K);

#endif
