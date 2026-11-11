/* Algebraic fractions: factorise the top, factorise the bottom, cancel common factors.
 * Sums of fractions go to algfrac_sum.c. Every answer is checked by cross-multiplying. */
#include "alg_int.h"

#include <stdio.h>
#include <string.h>

Expr *afr_over(const NFac *n, const NFac *d)
{
    if (nf_is_one(d)) return nf_expr(n);
    bool lone = poly_is_const(&n->mono, NULL);
    for (int i = 0; i < n->n && lone; i++)
        if (n->mult[i]) lone = false;
    if (rat_is_neg(n->k) && !lone) {
        NFac *a = amalloc(sizeof *a);
        *a = *n;
        a->k = rat_abs(a->k);
        return ex_neg(ex_div2(nf_expr(a), nf_expr(d)));
    }
    return ex_div2(nf_expr(n), nf_expr(d));
}

static const char *join(const char **v, int n)
{
    const char *s = v[0];
    for (int i = 1; i < n; i++) s = asprintf_a("%s%s%s", s, i == n - 1 ? " and " : ", ", v[i]);
    return s;
}

bool afr_cancel(NFac *n, NFac *d, Expr *left, Steps *st, Expr **out, bool *did)
{
    const char *names[FAC_MAXBR + 2];
    int nn = 0;
    Rat K, cn;
    if (!rat_div(n->k, d->k, &K)) return false;
    Rat p = rat_int(K.n), q = rat_int(K.d);
    if (rat_is_int(n->k) && rat_is_int(d->k) && rat_div(n->k, p, &cn) && !rat_is_one(cn))
        names[nn++] = alg_rstr(cn);
    n->k = p;
    d->k = q;
    Poly s, g, a, b;
    if (!poly_add(&n->mono, &d->mono, &s)) return false;
    g = poly_monomial_gcd(&s);
    if (!poly_is_const(&g, NULL)) {
        if (!poly_div_exact(&n->mono, &g, &a) || !poly_div_exact(&d->mono, &g, &b)) return false;
        n->mono = a;
        d->mono = b;
        names[nn++] = alg_pstr(&g);
    }
    for (int i = 0; i < n->n; i++)
        for (int j = 0; j < d->n && n->mult[i]; j++) {
            if (!d->mult[j] || !poly_equal(&n->br[i], &d->br[j])) continue;
            int m = n->mult[i] < d->mult[j] ? n->mult[i] : d->mult[j];
            n->mult[i] -= m;
            d->mult[j] -= m;
            const char *bs = alg_pstr(&n->br[i]);
            if (nn < FAC_MAXBR + 2) names[nn++] = m > 1 ? asprintf_a("(%s)^%d", bs, m) : asprintf_a("(%s)", bs);
        }
    *did = nn > 0;
    *out = afr_over(n, d);
    if (nn)
        steps_add(st, asprintf_a(nn == 1 ? "Cancel the common factor %s" : "Cancel the common factors %s", join(names, nn)),
                  "Dividing the top and the bottom by the same factor does not change the value.", "%s=%s",
                  ex_print(left), ex_print(*out));
    return true;
}

/* num/den of the whole input; a top-level quotient is read as top over bottom. */
static bool top_frac(const Expr *e, Poly *N, Poly *D)
{
    Poly an, ad, bn, bd, n;
    if (e->kind == E_NEG) return top_frac(e->kids[0], &n, D) && poly_neg(&n, N);
    if (e->kind == E_DIV) {
        if (!alg_frac_of(e->kids[0], &an, &ad) || !alg_frac_of(e->kids[1], &bn, &bd)) return false;
        return poly_mul(&an, &bd, N) && poly_mul(&ad, &bn, D);
    }
    return alg_frac_of(e, N, D);
}

static bool fail(char *err, int errcap, const char *msg)
{
    snprintf(err, (size_t)errcap, "%s", msg);
    return false;
}

bool alg_simplify_fraction(const Expr *e, Steps *st, Expr **out, char *err, int errcap)
{
    Poly N, D, rn, rd, x, y;
    Rat c;
    if (!e || e->kind == E_REL || e->kind == E_LIST) return fail(err, errcap, "Can only simplify algebraic fractions");
    if (e->kind == E_ADD) {
        bool sum = false;
        for (int i = 0; i < e->nkids; i++) {
            if (!alg_frac_of(e->kids[i], &x, &y)) return fail(err, errcap, "Can only simplify algebraic fractions");
            if (!poly_is_const(&y, NULL)) sum = true;
        }
        if (sum) return afr_sum(e, st, out, err, errcap);
    }
    if (!top_frac(e, &N, &D)) return fail(err, errcap, "Can only simplify algebraic fractions");
    if (poly_is_zero(&D)) return fail(err, errcap, "Can't divide by zero");
    if (poly_is_zero(&N)) {
        steps_add(st, "The top is 0", "0 divided by anything (except 0) is 0.", "0");
        *out = ex_int(0);
        return true;
    }
    if (poly_is_const(&D, &c) && rat_is_one(c)) {
        *out = ex_tidy(e);
        steps_add(st, "Nothing to simplify", "There is no fraction with letters on the bottom to cancel.", "%s", ex_print(*out));
        return true;
    }
    NFac *nf = amalloc(sizeof *nf), *df = amalloc(sizeof *df);
    if (!nf_of(&N, nf) || !nf_of(&D, df)) return fail(err, errcap, "Too big to simplify");
    Expr *top = nf_expr(nf), *bot = nf_expr(df);
    /* also shown when only a minus sign moved out: 3-x = -(x-3) */
    if (nf->split || (rat_is_neg(nf->k) && nf->n))
        steps_add(st, "Factorise the top", "Write the top as a product so common factors show.", "%s", ex_print(top));
    if (df->split || (rat_is_neg(df->k) && df->n))
        steps_add(st, "Factorise the bottom", "Write the bottom as a product too.", "%s", ex_print(bot));
    bool did;
    Expr *left = ex_div2(top, bot);
    if (!afr_cancel(nf, df, left, st, out, &did)) return fail(err, errcap, "Too big to simplify");
    if (!did) {
        steps_add(st, "Nothing cancels", "The top and the bottom have no common factor.", "%s", ex_print(left));
        *out = ex_tidy(e);
        return true;
    }
    if (!top_frac(*out, &rn, &rd) || !poly_mul(&rn, &D, &x) || !poly_mul(&N, &rd, &y) || !poly_equal(&x, &y))
        return fail(err, errcap, "Simplify check failed");
    return true;
}
