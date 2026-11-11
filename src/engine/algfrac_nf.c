/* Factor lists for algebraic fractions (NFac) and reading an expression as num/den. */
#include "alg_int.h"

#include <stdlib.h>
#include <string.h>

void nf_one(NFac *f)
{
    memset(f, 0, sizeof *f);
    f->k = rat_int(1);
    f->mono = poly_const(rat_int(1));
}

bool nf_add_br(NFac *f, const Poly *b, int m)
{
    for (int i = 0; i < f->n; i++)
        if (poly_equal(&f->br[i], b)) { f->mult[i] += m; return true; }
    if (f->n >= FAC_MAXBR) return false;
    f->br[f->n] = *b;
    f->mult[f->n++] = m;
    return true;
}

bool nf_poly(const NFac *f, Poly *out)
{
    Poly r, t, pw;
    if (!poly_scale(&f->mono, f->k, &r)) return false;
    for (int i = 0; i < f->n; i++) {
        if (!f->mult[i]) continue;
        if (!poly_pow(&f->br[i], f->mult[i], &pw) || !poly_mul(&r, &pw, &t)) return false;
        r = t;
    }
    *out = r;
    return true;
}

bool nf_of(const Poly *p, NFac *f)
{
    nf_one(f);
    if (poly_is_zero(p)) { f->k = rat_int(0); return true; }
    FCtx *c = amalloc(sizeof *c);
    if (!fac_run(c, p, NULL, NULL)) return false;
    f->k = c->num;
    f->mono = c->mono;
    f->split = c->any;
    for (int i = 0; i < c->nbr; i++) {
        Poly b = c->br[i].p, nb;
        if (rat_is_neg(alg_lead_coef(&b))) {
            if (!poly_neg(&b, &nb)) return false;
            b = nb;
            f->k = rat_neg(f->k);
        }
        if (!nf_add_br(f, &b, 1)) return false;
    }
    Poly chk;
    return nf_poly(f, &chk) && poly_equal(&chk, p);
}

Expr *nf_expr(const NFac *f)
{
    Poly br[FAC_MAXBR];
    int mult[FAC_MAXBR], n = 0;
    for (int i = 0; i < f->n; i++)
        if (f->mult[i]) { br[n] = f->br[i]; mult[n++] = f->mult[i]; }
    return alg_render(f->k, &f->mono, br, mult, NULL, n);
}

bool nf_is_one(const NFac *f)
{
    if (!rat_is_one(f->k) || !poly_is_const(&f->mono, NULL)) return false;
    for (int i = 0; i < f->n; i++)
        if (f->mult[i]) return false;
    return true;
}

static bool mono_gcd(const Poly *a, const Poly *b, Poly *g)
{
    Poly s;
    if (!poly_add(a, b, &s)) return false;
    *g = poly_monomial_gcd(&s);
    return true;
}

bool nf_lcm(const NFac *a, const NFac *b, NFac *out)
{
    Poly g, ab;
    nf_one(out);
    Rat x = rat_abs(a->k), y = rat_abs(b->k);
    if (rat_is_int(x) && rat_is_int(y)) {
        int64_t gg = i64_gcd(x.n, y.n), l;
        if (!gg || !i64_mul_ok(x.n / gg, y.n, &l)) return false;
        out->k = rat_int(l);
    } else if (!rat_mul(x, y, &out->k)) {
        return false;
    }
    if (!mono_gcd(&a->mono, &b->mono, &g) || !poly_mul(&a->mono, &b->mono, &ab) || !poly_div_exact(&ab, &g, &out->mono))
        return false;
    for (int i = 0; i < a->n; i++)
        if (a->mult[i] && !nf_add_br(out, &a->br[i], a->mult[i])) return false;
    for (int i = 0; i < b->n; i++) {
        if (!b->mult[i]) continue;
        int j;
        for (j = 0; j < out->n; j++)
            if (poly_equal(&out->br[j], &b->br[i])) break;
        if (j < out->n) { if (out->mult[j] < b->mult[i]) out->mult[j] = b->mult[i]; }
        else if (!nf_add_br(out, &b->br[i], b->mult[i])) return false;
    }
    return true;
}

bool nf_div(const NFac *a, const NFac *b, NFac *out)
{
    nf_one(out);
    if (!rat_div(a->k, b->k, &out->k) || !poly_div_exact(&a->mono, &b->mono, &out->mono)) return false;
    for (int i = 0; i < a->n; i++) {
        int m = a->mult[i];
        for (int j = 0; j < b->n; j++)
            if (poly_equal(&b->br[j], &a->br[i])) m -= b->mult[j];
        if (m < 0) return false;
        if (m && !nf_add_br(out, &a->br[i], m)) return false;
    }
    return true;
}

/* num/den for sums, products, quotients and whole powers of polynomials. */
bool alg_frac_of(const Expr *e, Poly *num, Poly *den)
{
    Poly an, ad, bn, bd, x, y;
    long long k;
    if (poly_from_expr(e, num)) { *den = poly_const(rat_int(1)); return true; }
    switch (e->kind) {
    case E_NEG:
        return alg_frac_of(e->kids[0], &an, den) && poly_neg(&an, num);
    case E_ADD:
    case E_MUL:
        if (!alg_frac_of(e->kids[0], &an, &ad)) return false;
        for (int i = 1; i < e->nkids; i++) {
            if (!alg_frac_of(e->kids[i], &bn, &bd)) return false;
            if (e->kind == E_MUL) {
                if (!poly_mul(&an, &bn, &x) || !poly_mul(&ad, &bd, &y)) return false;
                an = x;
            } else {
                Poly p, q;
                if (!poly_mul(&an, &bd, &p) || !poly_mul(&bn, &ad, &q) || !poly_add(&p, &q, &an) || !poly_mul(&ad, &bd, &y))
                    return false;
            }
            ad = y;
        }
        *num = an;
        *den = ad;
        return true;
    case E_DIV:
        if (!alg_frac_of(e->kids[0], &an, &ad) || !alg_frac_of(e->kids[1], &bn, &bd) || poly_is_zero(&bn)) return false;
        return poly_mul(&an, &bd, num) && poly_mul(&ad, &bn, den);
    case E_POW:
        if (!ex_is_int(e->kids[1], &k) || llabs(k) > 64 || !alg_frac_of(e->kids[0], &an, &ad)) return false;
        if (k < 0 && poly_is_zero(&an)) return false;
        return k >= 0 ? poly_pow(&an, (int)k, num) && poly_pow(&ad, (int)k, den)
                      : poly_pow(&ad, (int)-k, num) && poly_pow(&an, (int)-k, den);
    default:
        return false;
    }
}
