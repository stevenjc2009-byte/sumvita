/* Shared helpers for the GCSE tools: field parsing, number formatting, table macros. */
#ifndef TOOLS_UTIL_H
#define TOOLS_UTIL_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tools.h"
#include "../engine/arena.h"
#include "../engine/expr.h"
#include "../engine/num.h"

#define PI_D 3.14159265358979323846
#define TU_MAXLIST 64

/* ---- table macros ---- */
#define RUN(name) static void name(const char *const *v __attribute__((unused)), \
    const CalcSettings *s __attribute__((unused)), CalcResult *o __attribute__((unused)))
#define ST(t, n, ...) steps_add(&o->steps, (t), (n), __VA_ARGS__)
#define F_NUM(l, h, d)  { l, h, d, TF_NUMBER, NULL, false }
#define F_OPT(l, h)     { l, h, "", TF_NUMBER, NULL, true }
#define F_LIST(l, h, d) { l, h, d, TF_LIST, NULL, false }
#define F_EXPR(l, h, d) { l, h, d, TF_EXPR, NULL, false }
#define F_CH(l, ch, d)  { l, "", d, TF_CHOICE, ch, false }
#define F_UNIT          { "Units", "", "cm", TF_CHOICE, "mm|cm|m|km|in|ft", false }
#define TOOL(id_, cat_, name_, desc_, nf_, run_, ...) \
    { id_, cat_, name_, desc_, nf_, { __VA_ARGS__ }, run_ }

/* category lists (each file owns one array) */
const Tool *tools_number(int *n);
const Tool *tools_ratio(int *n);
const Tool *tools_geometry(int *n);
const Tool *tools_solid(int *n);
const Tool *tools_trig(int *n);
const Tool *tools_prob(int *n);
const Tool *tools_stats(int *n);
const Tool *tools_convert(int *n);
const Tool *tools_algebra(int *n);
const Tool *tools_coord(int *n);

/* ---- result helpers ---- */
void tu_begin(CalcResult *o);                       /* reset result, ok = false */
void tu_fail(CalcResult *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void tu_ans(CalcResult *o, const char *answer, const char *alt);   /* ok = true */
void tu_val(CalcResult *o, double x);               /* value / has_value */
void tu_text(CalcResult *o, const char *title, const char *note);  /* text-only step */
/* exact (π / surd) form vs decimal form, ordered by settings.prefer_exact; exact may be NULL */
void tu_pair(CalcResult *o, const CalcSettings *s, const char *decimal, const char *exact);

/* ---- field parsing (all return false and set o->error on failure) ---- */
bool tu_blank(const char *txt);
bool tu_num(CalcResult *o, const char *label, const char *txt, double *x);
bool tu_rat(CalcResult *o, const char *label, const char *txt, Rat *r);
bool tu_list(CalcResult *o, const char *label, const char *txt, double *a, int max, int *n);
bool tu_ratlist(CalcResult *o, const char *label, const char *txt, Rat *a, int max, int *n);
bool tu_int(CalcResult *o, const char *label, const char *txt, int64_t *x);   /* whole number */
int  tu_choice(const char *choices, const char *val);   /* index in pipe list, 0 if none */
Expr *tu_expr(CalcResult *o, const char *label, const char *txt);
/* f(x) with x bound; false on domain error */
bool tu_eval_x(const Expr *e, const char *var, double x, double *out);

/* ---- formatting (arena strings) ---- */
const char *tu_sf(double x, int sf);                /* x to sf significant figures, zeros trimmed */
const char *tu_dp(double x, int dp);                /* fixed decimal places */
double tu_rsf(double x, int sf);                    /* numeric rounding to sf */
const char *tu_w(double x);                         /* working value: 6 s.f. trimmed */
/* answer rounding honouring settings: dp setting wins, then sf, then tool default.
 * def_dp < 0 means the tool defaults to def_sf. */
const char *tu_f(double x, const CalcSettings *s, int def_sf, int def_dp);
const char *tu_rs(Rat r);                           /* "7/2" */
const char *tu_u(const char *num, const char *unit);        /* 12.3 "cm" */
const char *tu_unit_pow(const char *unit, int pow);         /* cm, cm², cm³ */
const char *tu_pi(double coef);                     /* "25π" / "(250/3)π", NULL if not rational */
const char *tu_poly(const double *c, int deg, const char *var);   /* highest power first */
const char *tu_ratio_str(const int64_t *p, int n);  /* 3:5:2 (no quotes) */

#endif
