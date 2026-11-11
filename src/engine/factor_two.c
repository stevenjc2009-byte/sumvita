/* Two-term factorising: difference of two squares, sum / difference of two cubes.
 * Also a perfect square minus a square (four terms): x^2-6x+9-y^2 = (x-3)^2-y^2. */
#include "alg_int.h"

#include <string.h>

/* k·mono as a polynomial */
static bool km_poly(Rat k, const Poly *mono, Poly *out) { return poly_scale(mono, k, out); }

/* root of a positive term k·mono, k-th power */
static bool term_root(Rat k, const Poly *mono, int deg, Poly *out)
{
    Rat r;
    Poly m;
    if (rat_is_neg(k) || !rat_root_exact(k, deg, &r) || !alg_mono_root(mono, deg, &m)) return false;
    return km_poly(rat_abs(r), &m, out);
}

/* Split q into its positive term P and the other term N (|N| in *nabs). */
static bool two_terms(const Poly *q, Rat *kp, Poly *mp, Rat *kn, Poly *mn, bool *opposite)
{
    if (q->n != 2) return false;
    Rat k0, k1;
    Poly m0, m1;
    if (!alg_term(q, 0, &k0, &m0, NULL) || !alg_term(q, 1, &k1, &m1, NULL)) return false;
    *opposite = rat_is_neg(k0) != rat_is_neg(k1);
    if (*opposite) {
        bool first = !rat_is_neg(k0);
        *kp = first ? k0 : k1; *mp = first ? m0 : m1;
        *kn = rat_abs(first ? k1 : k0); *mn = first ? m1 : m0;
    } else {
        int li = alg_lead_index(q);
        *kp = li == 0 ? k0 : k1; *mp = li == 0 ? m0 : m1;
        *kn = li == 0 ? k1 : k0; *mn = li == 0 ? m1 : m0;
    }
    return true;
}

static Expr *power_of(const Poly *a, int k)
{
    Expr *e = alg_pexpr(a);
    return ex_pow2(e, ex_int(k));
}

bool fac_squares(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Rat kp, kn;
    Poly mp, mn, a, b, plus, minus;
    bool opp;
    if (!two_terms(q, &kp, &mp, &kn, &mn, &opp) || !opp) return false;
    if (!term_root(kp, &mp, 2, &a) || !term_root(kn, &mn, 2, &b)) return false;
    if (!poly_add(&a, &b, &plus) || !poly_sub(&a, &b, &minus)) return false;
    s->pc[0] = plus;
    s->pc[1] = minus;
    s->n = 2;
    const char *as = alg_pstr(&a), *bs = alg_pstr(&b);
    s->title = "Difference of two squares";
    s->note = asprintf_a("a^2-b^2=(a+b)(a-b) with a=%s and b=%s.", as, bs);
    /* before the line that factorises it, so the working reads top to bottom */
    steps_add(&s->pre, "Write it as one square minus another", "Both terms are perfect squares.", "%s",
              ex_print(fac_render(c, i, ex_sub2(power_of(&a, 2), power_of(&b, 2)))));
    return true;
}

bool fac_cubes(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Rat kp, kn;
    Poly mp, mn, a, b, a2, ab, b2, lin, quad, t;
    bool opp;
    if (!two_terms(q, &kp, &mp, &kn, &mn, &opp)) return false;
    if (!opp && rat_is_neg(kp)) return false;
    if (!term_root(kp, &mp, 3, &a) || !term_root(rat_abs(kn), &mn, 3, &b)) return false;
    if (!poly_mul(&a, &a, &a2) || !poly_mul(&a, &b, &ab) || !poly_mul(&b, &b, &b2)) return false;
    /* a^3 - b^3 = (a-b)(a^2+ab+b^2);  a^3 + b^3 = (a+b)(a^2-ab+b^2) */
    if (opp) {
        if (!poly_sub(&a, &b, &lin) || !poly_add(&a2, &ab, &t) || !poly_add(&t, &b2, &quad)) return false;
    } else {
        if (!poly_add(&a, &b, &lin) || !poly_sub(&a2, &ab, &t) || !poly_add(&t, &b2, &quad)) return false;
    }
    s->pc[0] = lin;
    s->pc[1] = quad;
    s->n = 2;
    const char *as = alg_pstr(&a), *bs = alg_pstr(&b);
    s->title = opp ? "Difference of two cubes" : "Sum of two cubes";
    s->note = opp ? asprintf_a("a^3-b^3=(a-b)(a^2+ab+b^2) with a=%s and b=%s.", as, bs)
                  : asprintf_a("a^3+b^3=(a+b)(a^2-ab+b^2) with a=%s and b=%s.", as, bs);
    Expr *w = opp ? ex_sub2(power_of(&a, 3), power_of(&b, 3)) : ex_add2(power_of(&a, 3), power_of(&b, 3));
    steps_add(&s->pre, "Write it with two cubes", "Both terms are perfect cubes.", "%s", ex_print(fac_render(c, i, w)));
    return true;
}

/* Is S a three-term perfect square (a+b)^2 or (a-b)^2? *root = a±b, where a is the
 * root of the square term that comes first in display order (x^2-6x+9 -> x-3). */
static bool square_trinomial(const Poly *S, Poly *root)
{
    Poly t[3];
    int n;
    if (S->n != 3 || !alg_display_terms(S, t, 3, &n) || n != 3) return false;
    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++) {
            Rat ki, kj;
            Poly mi, mj, a, b, ab, two, neg;
            if (!alg_term(&t[i], 0, &ki, &mi, NULL) || !alg_term(&t[j], 0, &kj, &mj, NULL)) return false;
            if (!term_root(ki, &mi, 2, &a) || !term_root(kj, &mj, 2, &b)) continue;
            if (!poly_mul(&a, &b, &ab) || !poly_scale(&ab, rat_int(2), &two) || !poly_neg(&two, &neg)) return false;
            if (poly_equal(&two, &t[3 - i - j])) return poly_add(&a, &b, root);
            if (poly_equal(&neg, &t[3 - i - j])) return poly_sub(&a, &b, root);
        }
    return false;
}

/* (a±b) written out with a's own terms first: "x-3+y", "x-(y-1)" */
static Expr *a_pm_b(const Poly *a, const Poly *b, bool minus)
{
    Expr *be = alg_pexpr(b);
    if (be->kind == E_ADD) be->flags |= EF_PAREN;
    Expr *e = ex_add2(alg_pexpr(a), minus ? ex_neg(be) : be);
    e->flags |= EF_PAREN;
    return e;
}

/* Four terms that are a perfect square trinomial and a single square term:
 *   (x+y)^2-3^2 = (x+y+3)(x+y-3)   or   x^2-(y-1)^2 = (x+y-1)(x-y+1). */
bool fac_psq(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Poly t[4], A, B, P, R;
    int n;
    if (q->n != 4 || !alg_display_terms(q, t, 4, &n) || n != 4) return false;
    for (int j = 0; j < 4; j++) {
        Rat k;
        Poly m;
        if (!alg_term(&t[j], 0, &k, &m, NULL) || !term_root(rat_abs(k), &m, 2, &B)) continue;
        bool sub = rat_is_neg(k);   /* trinomial - B^2, else B^2 - trinomial */
        if (!(sub ? poly_sub(q, &t[j], &R) : poly_sub(&t[j], q, &R))) return false;
        if (!square_trinomial(&R, &P)) continue;
        if (sub) {
            A = P;
        } else {
            A = B;
            B = P;
        }
        if (!poly_add(&A, &B, &s->pc[0]) || !poly_sub(&A, &B, &s->pc[1])) return false;
        s->n = 2;
        const char *as = alg_pstr(&A), *bs = alg_pstr(&B);
        steps_add(&s->pre, "Spot the perfect square",
                  asprintf_a("%s=(%s)^2, so this is one square minus another.", alg_pstr(&R), alg_pstr(&P)), "%s",
                  ex_print(fac_render(c, i, ex_sub2(power_of(&A, 2), power_of(&B, 2)))));
        s->title = "Difference of two squares";
        s->note = asprintf_a("a^2-b^2=(a+b)(a-b) with a=%s and b=%s.", as, bs);
        /* the brackets before tidying, only when tidying changes them */
        Expr *raw = ex_mul2(a_pm_b(&A, &B, false), a_pm_b(&A, &B, true));
        Expr *tidy = ex_mul2(alg_pexpr(&s->pc[0]), alg_pexpr(&s->pc[1]));
        if (strcmp(ex_print(raw), ex_print(tidy)) != 0)
            steps_add(&s->pre, "Write down (a+b)(a-b)", "Then tidy each bracket.", "%s",
                      ex_print(fac_render(c, i, raw)));
        return true;
    }
    return false;
}
