/* Adding algebraic fractions: factorise the denominators, use the lowest common
 * denominator (its brackets in order of first appearance), add the numerators,
 * simplify the numerator, then cancel anything left in common. */
#include "alg_int.h"

#include <stdio.h>
#include <string.h>

/* N·m as typed working, e.g. 2(x+1) or (x+2)(x-1); *neg gets the sign, the result is positive. */
static Expr *num_times(const Poly *N, const NFac *m, bool *neg)
{
    NFac *t = amalloc(sizeof *t);
    *t = *m;
    Rat c;
    Poly mono, mm;
    if (poly_is_zero(N)) {
        t->k = rat_int(0);
    } else if (N->n == 1 && alg_term(N, 0, &c, &mono, NULL) && rat_mul(t->k, c, &t->k) && poly_mul(&t->mono, &mono, &mm)) {
        t->mono = mm;
    } else if (t->n < FAC_MAXBR) {
        memmove(&t->br[1], &t->br[0], sizeof(Poly) * (size_t)t->n);
        memmove(&t->mult[1], &t->mult[0], sizeof(int) * (size_t)t->n);
        t->br[0] = *N;
        t->mult[0] = 1;
        t->n++;
    }
    *neg = rat_is_neg(t->k);
    t->k = rat_abs(t->k);
    return nf_expr(t);
}

static Expr *sterm(Expr *num, bool neg, Expr *den)
{
    Expr *f = den ? ex_div2(num, den) : num;
    return neg ? ex_neg(f) : f;
}

static Expr *sum_of(Expr **v, int n) { return n == 1 ? v[0] : ex_node(E_ADD, n, v); }

static bool fail(char *err, int errcap, const char *msg)
{
    snprintf(err, (size_t)errcap, "%s", msg);
    return false;
}

bool afr_sum(const Expr *e, Steps *st, Expr **out, char *err, int errcap)
{
    int nk = e->nkids;
    Poly *N = amalloc(sizeof(Poly) * (size_t)nk), *D = amalloc(sizeof(Poly) * (size_t)nk);
    NFac *df = amalloc(sizeof(NFac) * (size_t)nk), *L = amalloc(sizeof(NFac)), *tmp = amalloc(sizeof(NFac));
    NFac *mul = amalloc(sizeof(NFac) * (size_t)nk);
    Expr **terms = amalloc(sizeof(Expr *) * (size_t)nk), **nums = amalloc(sizeof(Expr *) * (size_t)nk);
    bool split = false;
    for (int i = 0; i < nk; i++) {
        if (!alg_frac_of(e->kids[i], &N[i], &D[i])) return fail(err, errcap, "Can only simplify algebraic fractions");
        if (poly_is_zero(&D[i])) return fail(err, errcap, "Can't divide by zero");
        if (!nf_of(&D[i], &df[i])) return fail(err, errcap, "Too big to simplify");
        split = split || df[i].split;
    }
    if (split) {
        for (int i = 0; i < nk; i++) {
            bool neg = rat_is_neg(alg_lead_coef(&N[i]));
            Poly a;
            if (!poly_scale(&N[i], rat_int(neg ? -1 : 1), &a)) return fail(err, errcap, "Too big to simplify");
            terms[i] = sterm(alg_pexpr(&a), neg, nf_is_one(&df[i]) ? NULL : nf_expr(&df[i]));
        }
        steps_add(st, "Factorise the denominators", "Factorised denominators show what they have in common.", "%s",
                  ex_print(sum_of(terms, nk)));
    }
    *L = df[0];
    for (int i = 1; i < nk; i++) {
        if (!nf_lcm(L, &df[i], tmp)) return fail(err, errcap, "Too big to simplify");
        *L = *tmp;
    }
    Expr *Le = nf_expr(L);
    Poly sum = poly_zero(), mp, t, s;
    for (int i = 0; i < nk; i++) {
        bool neg;
        if (!nf_div(L, &df[i], &mul[i]) || !nf_poly(&mul[i], &mp) || !poly_mul(&N[i], &mp, &t) || !poly_add(&sum, &t, &s))
            return fail(err, errcap, "Too big to simplify");
        sum = s;
        Expr *ne = num_times(&N[i], &mul[i], &neg);
        terms[i] = sterm(ne, neg, Le);
        nums[i] = neg ? ex_neg(ne) : ne;
    }
    steps_add(st, "Use a common denominator", asprintf_a("The lowest common denominator is %s.", ex_print(Le)), "%s",
              ex_print(sum_of(terms, nk)));
    const char *added = ex_print(ex_div2(sum_of(nums, nk), Le));
    steps_add(st, "Add the numerators", "The bottoms are the same, so add the tops.", "%s", added);
    if (poly_is_zero(&sum)) {
        steps_add(st, "Simplify the numerator", "The top collects to 0, so the answer is 0.", "0");
        *out = ex_int(0);
        return true;
    }
    Expr *simple = ex_div2(alg_pexpr(&sum), Le);
    if (strcmp(ex_print(simple), added) != 0)
        steps_add(st, "Simplify the numerator", "Multiply out the brackets and collect like terms.", "%s", ex_print(simple));
    NFac *nn = amalloc(sizeof *nn);
    bool did;
    if (!nf_of(&sum, nn)) return fail(err, errcap, "Too big to simplify");
    *tmp = *L;
    if (!afr_cancel(nn, tmp, ex_div2(nf_expr(nn), Le), st, out, &did)) return fail(err, errcap, "Too big to simplify");
    if (!did) *out = simple;
    Poly n0, d0, rn, rd, x, y;
    if (!alg_frac_of(e, &n0, &d0) || !alg_frac_of(*out, &rn, &rd) || !poly_mul(&rn, &d0, &x) || !poly_mul(&n0, &rd, &y) ||
        !poly_equal(&x, &y))
        return fail(err, errcap, "Simplify check failed");
    return true;
}
