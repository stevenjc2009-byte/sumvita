/* Final tidy pass for a rearranged answer: a quotient is read as top over bottom
 * (clearing nested fractions through the LCD), both parts are factorised, common
 * factors cancel, and it is written back as number, π, then letters.
 * 1/(1/u+1/v) -> uv/(u+v);  (bx+b)/(x-1) -> b(x+1)/(x-1). */
#include "rearrange_tidy.h"
#include "alg_int.h"
#include "solve_int.h"
#include <string.h>

#define PI_LETTER "#"

static Expr *pi_letter(const Expr *e)
{
    if (e->kind == E_CONST && e->op == C_PI) return ex_var(PI_LETTER);
    Expr *c = ex_clone(e);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = pi_letter(e->kids[i]);
    return c;
}

/* only plain rational arithmetic: sums, products, quotients, whole-number powers */
static bool rational_tree(const Expr *e)
{
    switch (e->kind) {
    case E_NUM: case E_VAR: return true;
    case E_CONST: return e->op == C_PI;
    case E_ADD: case E_MUL: case E_DIV: case E_NEG: break;
    case E_POW: if (e->kids[1]->kind != E_NUM || !rat_is_int(e->kids[1]->r)) return false; break;
    default: return false;
    }
    for (int i = 0; i < e->nkids; i++)
        if (!rational_tree(e->kids[i])) return false;
    return true;
}

static bool top_bottom(const Expr *e, Poly *N, Poly *D)
{
    Poly an, ad, bn, bd;
    if (e->kind == E_DIV) {
        if (!alg_frac_of(e->kids[0], &an, &ad) || !alg_frac_of(e->kids[1], &bn, &bd)) return false;
        return poly_mul(&an, &bd, N) && poly_mul(&ad, &bn, D);
    }
    return alg_frac_of(e, N, D);
}

/* a fraction written inside a fraction: 1/(1/u+1/v) */
static bool nested(const Expr *e, bool inside)
{
    if (e->kind == E_DIV && inside) return true;
    for (int i = 0; i < e->nkids; i++)
        if (nested(e->kids[i], inside || e->kind == E_DIV)) return true;
    return false;
}

/* a sum with a common factor taken out: bx+b -> b(x+1) (not split into several brackets) */
static bool takes_factor(const Expr *top, const NFac *n)
{
    int br = 0;
    for (int i = 0; i < n->n; i++) br += n->mult[i] > 0;
    return top->kind == E_ADD && br <= 1 && (!poly_is_const(&n->mono, NULL) || !rat_is_one(rat_abs(n->k)));
}

/* a sum read -f+u is written u-f (a positive term first) */
static Expr *nice_sums(const Expr *e)
{
    Expr *c = ex_clone(e);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = nice_sums(e->kids[i]);
    return c->kind == E_ADD ? sv_nice(c) : c;
}

Expr *rr_tidy(const Expr *K)
{
    if (K->kind != E_DIV || !rational_tree(K)) return NULL;
    Expr *k = pi_letter(K);
    Poly N, D, rn, rd, x, y;
    if (!top_bottom(k, &N, &D) || poly_is_zero(&D) || poly_is_zero(&N)) return NULL;
    NFac *nf = amalloc(sizeof *nf), *df = amalloc(sizeof *df);
    if (!nf_of(&N, nf) || !nf_of(&D, df)) return NULL;
    Expr *out;
    bool did;
    bool worth = nested(K, false) || takes_factor(K->kids[0], nf) || strstr(ex_print(K->kids[0]), "×");
    if (!afr_cancel(nf, df, ex_var("x"), NULL, &out, &did)) return NULL;
    if (!worth && !did) return NULL;
    /* -fu/(f-u): put the minus sign into the bottom bracket instead, fu/(u-f) */
    if (rat_is_neg(nf->k) && !nf_is_one(df)) {
        for (int j = 0; j < df->n; j++) {
            Poly t;
            if (df->mult[j] % 2 == 0 || df->br[j].n < 2 || !poly_neg(&df->br[j], &t)) continue;
            df->br[j] = t;
            nf->k = rat_neg(nf->k);
            out = afr_over(nf, df);
            break;
        }
    }
    /* the tidy form must be the same fraction: top1·bottom0 = top0·bottom1 */
    if (!top_bottom(out, &rn, &rd) || !poly_mul(&rn, &D, &x) || !poly_mul(&N, &rd, &y) || !poly_equal(&x, &y)) return NULL;
    out = nice_sums(ex_subst(out, PI_LETTER, ex_const(C_PI)));
    if (ex_equal(out, K) || (out->kind == E_NEG && K->kind != E_NEG)) return NULL;
    return out;
}
