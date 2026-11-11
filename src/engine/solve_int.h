/* Internal interface shared by the solve / complete-the-square / rearrange files.
 * Every function writes GCSE-style working through sv_step* (NULL Steps is fine). */
#ifndef ENGINE_SOLVE_INT_H
#define ENGINE_SOLVE_INT_H

#include <stdbool.h>
#include <stdint.h>
#include "algebra.h"
#include "poly.h"

#define SV_MAXDEG 12
#define SV_MAXROOTS 16

typedef struct {
    const CalcSettings *s;   /* may be NULL */
    Steps *st;               /* may be NULL */
    char *err;
    int errcap;
    char var[12];            /* the unknown */
    const char *last;        /* last maths line shown (to skip repeats) */
} SvCtx;

/* One real root: exact value A + B·√m (B = 0 or m = 1 for a rational), or numeric v. */
typedef struct {
    bool exact;
    Rat A, B;
    int64_t m;
    double v;
    bool pm_next;            /* this root and the next are A ± B√m: show as one ± answer */
} SvRoot;

typedef struct {
    int n;
    SvRoot r[SV_MAXROOTS];
    bool all;                /* every value is a solution */
    bool numeric;            /* some roots were found numerically */
} SvRoots;

/* linear result */
enum { SV_ONE = 0, SV_NONE, SV_ALL };
typedef struct { int kind; int op; Rat val; } SvLin;

/* ---- solve_util.c ---- */
void  sv_step(SvCtx *c, const char *title, const char *note, const char *math);
void  sv_step_new(SvCtx *c, const char *title, const char *note, const char *math);  /* skip if same as last line */
void  sv_step_rel(SvCtx *c, const char *title, const char *note, int op, const Expr *l, const Expr *r);
bool  sv_fail(SvCtx *c, const char *msg);
const char *sv_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
Expr *sv_pexpr(const Poly *p, const char *var);          /* tidy display of a polynomial */
Expr *sv_nice(Expr *e);                                    /* sum: put a positive term first */
Expr *sv_lin_expr(Rat a, Rat b, const char *var);         /* a·var + b for display */
Poly  sv_lin_poly(Rat a, Rat b, const char *var);
bool  sv_upoly(const Poly *p, const char *var, Rat *c, int *deg);   /* deg <= SV_MAXDEG */
int   sv_flip(int op);
Expr *sv_chain(int n, Expr **kids, const int *ops);
bool  sv_sqrt_split(Rat d, Rat *s, int64_t *m);           /* √d = s√m, m square-free */
Expr *sv_surd_expr(Rat A, Rat B, int64_t m, bool pm);     /* A ± B√m  or  A + B√m */
Expr *sv_root_expr(const SvRoot *r);
double sv_root_val(const SvRoot *r);
void  sv_add_rat(SvRoots *R, Rat v);
void  sv_add_real(SvRoots *R, double v);
const char *sv_dec(double v, int dp);
int   sv_dp(const SvCtx *c);                               /* decimal places for decimal lines (default 2) */
Expr *sv_answer(SvCtx *c, const SvRoots *R);
const char *sv_or_list(SvCtx *c, const SvRoots *R, bool decimals);   /* x=2 "or" x=3 */
Expr *sv_real(double v);
bool  sv_lcm_dens(const Poly *p, int64_t *m);              /* lcm of coefficient denominators */
bool  sv_has_decimal(const Expr *e);
const char *sv_rat_str(Rat r);

/* ---- solve_prep.c ---- */
/* Clears fractions, expands brackets, collects like terms (each shown only if it changes the line).
 * L, R must convert with poly_from_expr. */
bool sv_prepare(SvCtx *c, int op, const Expr *L, const Expr *R, Poly *PL, Poly *PR);
/* Brings PL op PR to P op 0 with a positive leading coefficient and coprime integer
 * coefficients; *op is flipped when both sides are multiplied by a negative. */
bool sv_to_zero(SvCtx *c, int *op, const Poly *PL, const Poly *PR, Poly *P);

/* ---- solve_frac.c ---- */
bool sv_frac_form(const Expr *e, Poly *N, Poly *D);        /* e = N/D */
bool sv_rational_eq(SvCtx *c, const Expr *L, const Expr *R, Expr **out);

/* ---- solve_linear.c ---- */
bool sv_linear(SvCtx *c, int op, const Poly *PL, const Poly *PR, SvLin *res);

/* ---- solve_quad.c ---- */
bool sv_quad_core(SvCtx *c, Rat a, Rat b, Rat k, SvRoots *R);   /* a x^2 + b x + k = 0 already shown */
void sv_quad_numeric(SvCtx *c, double a, double b, double k, SvRoots *R);

/* ---- solve_poly.c ---- */
bool sv_poly_solve(SvCtx *c, const Poly *PL, const Poly *PR, SvRoots *R);   /* any degree */

/* ---- solve_num.c ---- */
int  sv_upoly_real_roots(const double *a, int deg, double *out, int max);
int  sv_expr_roots(SvCtx *c, const Expr *f, double *out, int max);

/* ---- solve.c ---- */
bool sv_equation(SvCtx *c, const Expr *L, const Expr *R, Expr **out);

/* ---- solve_nonpoly.c ---- */
bool sv_nonpoly(SvCtx *c, const Expr *L, const Expr *R, Expr **out);
bool sv_numeric_fallback(SvCtx *c, const Expr *L, const Expr *R, const char *why, Expr **out);

/* ---- solve_ineq.c ---- */
bool sv_inequality(SvCtx *c, int op, const Expr *L, const Expr *R, Expr **out);
bool sv_double_ineq(SvCtx *c, const Expr *e, Expr **out);

/* ---- solve_sim.c / solve_simq.c ---- */
bool sv_system(SvCtx *c, const Expr *e, Expr **out);
bool sv_system_linquad(SvCtx *c, Expr **eqs, char vars[][12], Expr **out);

/* ---- rearrange.c / rearrange_lin.c ---- */
int   rr_count(const Expr *e, const char *v);        /* occurrences of variable v */
Expr *rr_par(const Expr *e);                         /* copy, bracketed if a sum or negative */
Expr *rr_neg(const Expr *e);                         /* -e written neatly */
Expr *rr_div(Expr *K, const Expr *d);
Expr *rr_mul(Expr *K, const Expr *m);
bool  rr_collect(SvCtx *c, const Expr *L, const Expr *R, Expr **out);
/* rearrange_quad.c: Q = 0 is a quadratic in c->var (other letters are constants) */
bool  rr_quadratic(SvCtx *c, const Poly *Q, Expr **out);

#endif
