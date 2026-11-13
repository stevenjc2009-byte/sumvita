/* v1.2 solver additions: trig equations, matching powers, squaring surd equations,
 * decimal answers at the app's usual precision. Shared by solve_*.c. */
#ifndef ENGINE_SOLVE_EXT_H
#define ENGINE_SOLVE_EXT_H

#include "solve_int.h"

/* ---- solve_fmt.c ---- */
const char *sv_numdec(SvCtx *c, double v);                 /* decimal text, same rounding as calc answers */
Expr *sv_numexpr(SvCtx *c, double v);                      /* that decimal as a number node */
const char *sv_or_list_num(SvCtx *c, const SvRoots *R);    /* x=2.144040433 "or" ... */
/* e = a*T + d with T a call of fop (F_SQRT), or of sin/cos/tan when fop is 0; a, d exact */
bool sv_lindec(SvCtx *c, const Expr *e, int fop, Rat *a, Rat *d, const Expr **T);
bool sv_exact_num(const Expr *e, Rat *r);
/* Trial and improvement working for a root of a[0] + a[1]x + ... = 0 (one line per decimal place) */
void sv_trial_steps(SvCtx *c, const double *a, int deg, double root);

/* ---- solve_trig.c ---- */
bool sv_trig_eq(SvCtx *c, const Expr *L, const Expr *R, Expr **out);
bool sv_has_trig(const Expr *e, const char *var);

/* ---- solve_surd.c ---- */
bool sv_surd_eq(SvCtx *c, const Expr *L, const Expr *R, Expr **out);

/* ---- solve_util.c ---- */
/* adds the pair A + B√m and A - B√m (the first marked to print as A ± B√m) */
void sv_add_pair(SvRoots *R, Rat A, Rat B, int64_t m);

/* ---- solve_num.c ---- */
/* roots of f on [lo, hi] only (uniform scan) */
int  sv_expr_roots_range(SvCtx *c, const Expr *f, double lo, double hi, double *out, int max);

#endif
