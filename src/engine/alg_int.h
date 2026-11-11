/* Internal helpers shared by expand.c, factor*.c and algfrac*.c (owner: expand/factor agent).
 *
 * Factor engine state (FCtx): a running factorisation  num · mono · br[0] · br[1] · ...
 * Display/order rule for the factors (see fac_render):
 *   number first, then the monomial letters (alphabetical), then the brackets in
 *   DISCOVERY ORDER: when a bracket is split further it is removed and its pieces are
 *   appended at the END of the list. Each method emits its pieces in textbook order:
 *   HCF -> (rest); two squares -> (a+b)(a-b); cubes -> (linear)(quadratic);
 *   quadratics -> by coefficient of the first letter, then smaller number first, + before -;
 *   grouping -> (common bracket)(other); factor theorem -> (x-a)(quotient).
 *   Equal brackets are merged into a power at the first one's position.
 *   So 2x^2y-8y+3x^2-12 -> (x^2-4)(2y+3) -> (2y+3)(x+2)(x-2) and x^4-16 -> (x^2+4)(x+2)(x-2).
 */
#ifndef ENGINE_ALG_INT_H
#define ENGINE_ALG_INT_H

#include <stdbool.h>
#include "algebra.h"
#include "arena.h"
#include "poly.h"

#define FAC_MAXBR 32
#define FAC_MAXPC 8

typedef struct {
    Poly p;
    unsigned char asc;      /* display terms in ascending order ("5-x", "4+2x+x^2") */
} FBr;

typedef struct {
    Rat num;                /* numeric factor */
    Poly mono;              /* monomial factor, coefficient 1 */
    FBr br[FAC_MAXBR];
    int nbr;
    Steps *st;              /* may be NULL (silent) */
    bool any;               /* at least one split happened */
    bool proven;            /* set by methods: the bracket being tried is irreducible over Q */
    bool noroots;           /* fac_root searched every candidate and found no rational root */
    const char *why;        /* note explaining why the last failed bracket does not factorise */
    Poly typed[16];         /* the original's terms in the order the user typed them */
    int ntyped;
    Poly tg;                /* the top-level common factor taken out (1 if none) */
    bool tg_set;
} FCtx;

/* A method's proposed split of bracket i. */
typedef struct {
    Poly pc[FAC_MAXPC];
    unsigned char asc[FAC_MAXPC];
    int n;
    const char *title, *note;
    Steps pre;              /* lines shown before the main line, at the same depth */
    Steps det;              /* detail lines shown one level under the main line */
} FSplit;

typedef bool (*FMethod)(FCtx *c, int i, FSplit *s);

/* ---- factor methods (factor_*.c); each returns false if it does not apply ---- */
bool fac_hcf(FCtx *c, int i, FSplit *s);
bool fac_squares(FCtx *c, int i, FSplit *s);
bool fac_cubes(FCtx *c, int i, FSplit *s);
bool fac_psq(FCtx *c, int i, FSplit *s);     /* (a+b)^2 - c^2 with four terms */
bool fac_quad(FCtx *c, int i, FSplit *s);
bool fac_group(FCtx *c, int i, FSplit *s);
bool fac_root(FCtx *c, int i, FSplit *s);
bool fac_quadfactor(FCtx *c, int i, FSplit *s);

/* Runs every method until nothing splits. p must be non-zero. typed may be NULL. */
bool fac_run(FCtx *c, const Poly *p, const Expr *typed, Steps *st);
/* The current product as a display tree; bracket `over` (or -1) is shown as `with`. */
Expr *fac_render(const FCtx *c, int over, Expr *with);

/* ---- expand (expand.c, expand_term.c) ----
 * One product term k·mono·br[0]^pw·br[1]^pw... as the user typed it. */
#define EXP_MAXBR 16

typedef struct {
    Poly p;
    const Expr *disp;       /* the bracket as typed (NULL: print p) */
    int pw;
} EBr;

typedef struct {
    Rat k;
    Poly mono;
    EBr br[EXP_MAXBR];
    int nbr;
} ETerm;

/* Multiplies out one term, writing GCSE working into st; *out is the collected result. */
bool exp_term(ETerm *t, Steps *st, Poly *out);

/* ---- algebraic fractions (algfrac.c, algfrac_sum.c, algfrac_nf.c) ----
 * A polynomial as k · mono · br[0]^mult[0] · ...; every bracket has a positive leading
 * coefficient (any sign lives in k), so (3-x) and (x-3) are seen as the same factor. */
typedef struct {
    Rat k;
    Poly mono;
    Poly br[FAC_MAXBR];
    int mult[FAC_MAXBR];    /* 0 = cancelled away */
    int n;
    bool split;             /* the factorisation changed something */
} NFac;

bool nf_of(const Poly *p, NFac *f);                  /* silent factorisation */
void nf_one(NFac *f);
bool nf_add_br(NFac *f, const Poly *b, int m);
bool nf_poly(const NFac *f, Poly *out);              /* multiplied back out */
Expr *nf_expr(const NFac *f);                        /* display: number, letters, brackets */
bool nf_is_one(const NFac *f);
bool nf_lcm(const NFac *a, const NFac *b, NFac *out);
bool nf_div(const NFac *a, const NFac *b, NFac *out); /* a/b, b a factor of a */
bool alg_frac_of(const Expr *e, Poly *num, Poly *den); /* any rational expression -> num/den */
/* Cancels the common factors of n/d with a step whose maths starts "left=". *out is the answer.
 * *did says whether anything cancelled. False on overflow. */
bool afr_cancel(NFac *n, NFac *d, Expr *left, Steps *st, Expr **out, bool *did);
Expr *afr_over(const NFac *n, const NFac *d);        /* n/d display, minus sign in front */
bool afr_sum(const Expr *e, Steps *st, Expr **out, char *err, int errcap);

/* ---- poly / display utilities (alg_util.c) ---- */
const char *alg_mainvar(const Poly *p);          /* highest power, ties alphabetical; NULL if constant */
Expr *alg_pexpr(const Poly *p);                  /* tidy tree, descending in its main variable */
Expr *alg_pexpr_asc(const Poly *p);              /* same, terms reversed */
const char *alg_pstr(const Poly *p);
bool alg_term(const Poly *p, int i, Rat *coef, Poly *mono, Poly *whole);
int  alg_lead_index(const Poly *p);              /* first term of the display order */
Rat  alg_lead_coef(const Poly *p);
bool alg_hcf(const Poly *p, Rat *content, Poly *mono);
bool alg_mono_root(const Poly *mono, int k, Poly *out);
int  alg_mono_degree(const Poly *mono);
Expr *alg_km_expr(Rat k, const Poly *mono);      /* k·mono as a tidy tree ("3xy", "-1", "x^2") */
Expr *alg_prod(Expr **kids, int n);              /* MUL of kids (flattens MUL kids); 1 if n == 0 */
Expr *alg_render(Rat k, const Poly *mono, const Poly *br, const int *mult, const unsigned char *asc, int n);
const char *alg_rstr(Rat r);                     /* "3", "-1/2" */
const char *alg_rpar(Rat r);                     /* "3", "(-1/2)" */
void alg_steps_copy(Steps *dst, const Steps *src);
bool alg_display_terms(const Poly *p, Poly *terms, int max, int *n); /* terms in display order */
Expr *alg_sum_of(const Poly *terms, int n);      /* uncollected sum of the given terms */
bool alg_poly_of(const Expr *e, Poly *out);
Rat  alg_rat1(void);

#endif
