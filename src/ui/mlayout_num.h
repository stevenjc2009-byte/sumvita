/* mlayout_num.h - number layout helpers shared by mlayout_expr.c. */
#ifndef UI_MLAYOUT_NUM_H
#define UI_MLAYOUT_NUM_H

#include "ui/mlayout.h"

/* "-1.5" -> "−1.5" (U+2212 minus) */
void mlx_fix_minus(const char *in, char *out, int cap);
/* approximate value: "1.5", or "1.5x10^-7" with a raised exponent */
Frag mlx_real(double v, int px);
/* E_RECUR: text "0.1(27)" drawn with dots over the first and last repeating digit */
Frag mlx_recur(const struct Expr *e, int px);

#endif
