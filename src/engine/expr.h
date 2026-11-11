/* Expression trees.
 *
 * One tree type serves three jobs:
 *   1. the parse of what the user typed (display-faithful: keeps DIV, NEG,
 *      user brackets, decimals as typed),
 *   2. the maths the engine works on (after expr_simplify: canonical form),
 *   3. what the UI lays out in natural display (steps and results are printed
 *      to DISPLAY SYNTAX with expr_print, then re-parsed by the UI).
 * Nodes are arena-allocated (arena.h) and never freed individually.
 * Full syntax: docs/SYNTAX.md.
 */
#ifndef ENGINE_EXPR_H
#define ENGINE_EXPR_H

#include <stdbool.h>
#include "num.h"

typedef enum {
    E_NUM,    /* exact rational r. NF_DECIMAL: show as decimal (typed "2.5") */
    E_REAL,   /* approximate double v */
    E_VAR,    /* variable, name in name[] ("x", "y", "θ" as UTF-8) */
    E_CONST,  /* op = C_PI | C_E */
    E_ADD,    /* n-ary sum, kids >= 2 */
    E_MUL,    /* n-ary product, kids >= 2 */
    E_DIV,    /* kids[0] / kids[1] (shown as a stacked fraction) */
    E_NEG,    /* -kids[0] */
    E_POW,    /* kids[0] ^ kids[1] */
    E_FUNC,   /* op = F_*, kids = arguments */
    E_REL,    /* op = R_*, kids[0] REL kids[1] (chains allowed: kids >= 2, same op list in ops[]) */
    E_LIST,   /* comma list (solutions, simultaneous systems) */
    E_TEXT,   /* literal text in text (from "..." in display syntax) */
    E_PM,     /* kids[0] ± kids[1]; one kid = ±kids[0] */
    E_PCT,    /* kids[0] % */
    E_FACT,   /* kids[0] ! */
    E_DEG,    /* kids[0] ° (degrees marker) */
    E_MIXED,  /* mixed number: kids[0] whole, kids[1] numerator, kids[2] denominator */
    E_RECUR,  /* recurring decimal: text holds "0.1(27)" form, r holds exact value */
} ExprKind;

enum { C_PI = 1, C_E = 2 };

enum {
    F_SIN = 1, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN,
    F_SINH, F_COSH, F_TANH,
    F_LN, F_LOG,      /* LOG: 1 arg = log10, 2 args = log base kids[0] of kids[1] */
    F_SQRT,           /* √x */
    F_ROOT,           /* root(n, x): n-th root of x, kids[0] = n */
    F_ABS,
    F_FLOOR, F_CEIL, F_ROUND,
    F_NCR, F_NPR,     /* nCr(n, r), nPr(n, r) */
    F_EXP,            /* e^x written exp(x) */
    F_FUNC_COUNT
};

enum { R_EQ = 1, R_LT, R_LE, R_GT, R_GE, R_NE };

/* flags */
enum {
    NF_DECIMAL = 1 << 0,   /* E_NUM: display as decimal */
    EF_PAREN   = 1 << 1,   /* user wrote brackets round this node: printer keeps them */
    EF_COMMA   = 1 << 2,   /* parsed list: on the E_LIST, its separators are as typed;
                              on a list item, a comma (not a space) followed it */
};

#define EXPR_MAX_REL 4

typedef struct Expr {
    ExprKind kind;
    int op;                 /* C_*, F_*, R_* */
    unsigned flags;
    int nkids;
    struct Expr **kids;
    Rat r;                  /* E_NUM, E_RECUR */
    double v;               /* E_REAL */
    char name[12];          /* E_VAR */
    const char *text;       /* E_TEXT, E_RECUR */
    int ops[EXPR_MAX_REL];  /* E_REL chains: ops[i] between kids[i] and kids[i+1]; ops[0] == op */
} Expr;

/* ---- construction (arena) ---- */
Expr *ex_num(Rat r);
Expr *ex_int(long long n);
Expr *ex_real(double v);
Expr *ex_var(const char *name);
Expr *ex_const(int c);
Expr *ex_text(const char *s);
Expr *ex_node(ExprKind k, int nkids, Expr **kids);   /* copies the kid pointer array */
Expr *ex_add2(Expr *a, Expr *b);
Expr *ex_sub2(Expr *a, Expr *b);                       /* a + NEG(b) */
Expr *ex_mul2(Expr *a, Expr *b);
Expr *ex_div2(Expr *a, Expr *b);
Expr *ex_pow2(Expr *a, Expr *b);
Expr *ex_neg(Expr *a);
Expr *ex_func1(int f, Expr *a);
Expr *ex_func2(int f, Expr *a, Expr *b);
Expr *ex_rel(int op, Expr *l, Expr *r);
Expr *ex_clone(const Expr *e);                         /* deep copy */

/* ---- inspection ---- */
bool ex_is_num(const Expr *e);              /* E_NUM */
bool ex_is_int(const Expr *e, long long *v);
bool ex_is_zero(const Expr *e);
bool ex_is_one(const Expr *e);
bool ex_equal(const Expr *a, const Expr *b);           /* structural */
int  ex_compare(const Expr *a, const Expr *b);         /* total order for canonical sorting */
bool ex_has_var(const Expr *e, const char *name);      /* name NULL = any variable */
int  ex_vars(const Expr *e, char names[][12], int max); /* distinct variable names, sorted */
bool ex_is_numeric(const Expr *e);                     /* no variables at all */
/* E_LIST: does a comma (not a space) go between kids[i-1] and kids[i]? A typed
 * list keeps its own separators; a built one puts words ("or") between items
 * without commas. Shared by the printer and the UI layout. */
bool ex_list_comma(const Expr *list, int i);

/* ---- parse (parse.c) ----
 * Display-faithful parse. On error returns NULL and writes a short message
 * ("Missing )", "Unexpected +" ...) plus the byte offset of the problem. */
Expr *ex_parse(const char *src, char *err, int errcap, int *err_pos);

/* ---- print (print.c) ----
 * DISPLAY SYNTAX, minimal brackets, re-parseable by ex_parse so that
 * ex_parse(ex_print(e)) is ex_equal to e up to EF_PAREN. Uses "*" only where
 * implicit multiplication would be ambiguous (between two numbers).
 * Returns an arena string. */
const char *ex_print(const Expr *e);
/* Plain one-line text for history / copy (same as ex_print today). */
const char *ex_print_plain(const Expr *e);

/* ---- simplify (simplify.c) ----
 * Canonical form: flatten ADD/MUL, fold exact numbers, collect like terms,
 * combine powers of the same base, DIV -> MUL with POW -1, NEG -> MUL by -1,
 * sort terms (highest degree first, then alphabetical), exact surds
 * (√12 -> 2√3), exact trig for special angles (sin 30° = 1/2) in degree mode.
 * Never turns an exact value into a decimal. */
Expr *ex_simplify(const Expr *e);

/* Undo canonical internals for display: MUL by -1 -> NEG, x^-1 -> 1/x,
 * negative coefficients -> subtraction, rational coefficients -> fractions. */
Expr *ex_tidy(const Expr *e);

/* ---- numeric evaluation (eval.c) ---- */
typedef struct {
    bool degrees;               /* trig in degrees */
    const char *var_names[8];   /* variable bindings for graphing / substitution */
    double var_values[8];
    int nvars;
} EvalEnv;

/* Returns false (and sets *out = NAN) on domain error / unbound variable. */
bool ex_eval(const Expr *e, const EvalEnv *env, double *out);

/* Substitute variable `name` with `val` (a new tree). */
Expr *ex_subst(const Expr *e, const char *name, const Expr *val);

#endif
