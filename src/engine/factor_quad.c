/* Quadratics A·U^2 + B·U·V + C·V^2 where U, V are monomials:
 * x^2+5x+6 (U=x, V=1), x^2+5xy+6y^2 (V=y), x^4-5x^2+4 (U=x^2), 6x^2+x-2 (ac method). */
#include "alg_int.h"

#include <stdlib.h>

/* a·U + b·V */
static bool lin(const Poly *U, const Poly *V, int64_t a, int64_t b, Poly *out)
{
    Poly x, y;
    return poly_scale(U, rat_int(a), &x) && poly_scale(V, rat_int(b), &y) && poly_add(&x, &y, out);
}

/* k·M as a single-term polynomial */
static bool term(const Poly *M, int64_t k, Poly *out) { return poly_scale(M, rat_int(k), out); }

static int64_t gcd_pos(int64_t a, int64_t b)
{
    int64_t g = i64_gcd(a, b);
    return g ? g : 1;
}

/* true when linear factor (a1,b1) should be written before (a2,b2) */
static bool lin_before(int64_t a1, int64_t b1, int64_t a2, int64_t b2)
{
    if (a1 != a2) return a1 < a2;
    int64_t x = llabs(b1), y = llabs(b2);
    if (x != y) return x < y;
    return b1 > b2;
}

static const char *ri(int64_t v) { return alg_rstr(rat_int(v)); }
static const char *rp(int64_t v) { return alg_rpar(rat_int(v)); }

bool fac_quad(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Poly t[3], m[3], U, V, UV;
    Rat k[3];
    int n;
    if (q->n != 3 || !alg_display_terms(q, t, 3, &n) || n != 3) return false;
    for (int j = 0; j < 3; j++)
        if (!alg_term(&t[j], 0, &k[j], &m[j], NULL) || !rat_is_int(k[j])) return false;
    if (!alg_mono_root(&m[0], 2, &U) || !alg_mono_root(&m[2], 2, &V) || !poly_mul(&U, &V, &UV) ||
        !poly_equal(&UV, &m[1]))
        return false;
    int64_t A = k[0].n, B = k[1].n, C = k[2].n, B2, AC, AC4, D, sq;
    if (A <= 0) return false;
    if (!i64_mul_ok(B, B, &B2) || !i64_mul_ok(A, C, &AC) || !i64_mul_ok(AC, 4, &AC4) || !i64_add_ok(B2, -AC4, &D))
        return false;
    bool simple = alg_mono_degree(&U) <= 1 && alg_mono_degree(&V) <= 1;
    if (D < 0 || !i64_isqrt_exact(D, &sq)) {
        if (simple) {
            c->proven = true;
            c->why = asprintf_a("b^2-4ac = %s is not a square number, so there are no rational factors.", ri(D));
        }
        return false;
    }
    int64_t r = (B + sq) / 2, r2 = (B - sq) / 2;
    const char *Us = alg_pstr(&U);
    if (alg_mono_degree(&U) > 1)
        steps_add(&s->det, asprintf_a("Treat it as a quadratic in %s", Us),
                  asprintf_a("Think of %s as a single letter.", Us), NULL);
    s->title = r == r2 ? "Perfect square" : "Factorise the quadratic";
    Poly f1, f2;
    if (A == 1) {
        if (!lin(&U, &V, 1, r, &f1) || !lin(&U, &V, 1, r2, &f2)) return false;
        bool sw = !lin_before(1, r, 1, r2);
        s->pc[0] = sw ? f2 : f1;
        s->pc[1] = sw ? f1 : f2;
        s->note = r == r2 ? "Both brackets are the same, so write it as a square."
                          : "Find two numbers that multiply to c and add to b.";
        steps_add(&s->det, "Find the two numbers",
                  asprintf_a("They multiply to %s and add to %s: %s and %s.", ri(C), ri(B), ri(sw ? r2 : r), ri(sw ? r : r2)),
                  "%s×%s=%s, %s+%s=%s", rp(sw ? r2 : r), rp(sw ? r : r2), ri(C), ri(sw ? r2 : r), rp(sw ? r : r2), ri(B));
        s->n = 2;
        return true;
    }
    /* ac method: A·U^2 + r·UV + r2·UV + C·V^2 = g1·U(...) + g2·V(...) */
    int64_t g1 = gcd_pos(A, r), g2 = gcd_pos(r2, C);
    if (r2 / g2 != A / g1) g2 = -g2;
    if (r2 / g2 != A / g1 || C / g2 != r / g1) return false;
    Poly br, other, sp[4], gu, gv;
    if (!lin(&U, &V, A / g1, r / g1, &br) || !lin(&U, &V, g1, g2, &other)) return false;
    if (!term(&m[0], A, &sp[0]) || !term(&m[1], r, &sp[1]) || !term(&m[1], r2, &sp[2]) || !term(&m[2], C, &sp[3]))
        return false;
    if (!term(&U, g1, &gu) || !term(&V, g2, &gv)) return false;
    bool sw = !lin_before(A / g1, r / g1, g1, g2);
    s->pc[0] = sw ? other : br;
    s->pc[1] = sw ? br : other;
    s->n = 2;
    s->note = "Split the middle term using two numbers that multiply to ac and add to b.";
    steps_add(&s->det, "Multiply a and c", "Look for two numbers that multiply to ac and add to b.",
              "%s×%s=%s", ri(A), rp(C), ri(AC));
    steps_add(&s->det, "Find the two numbers",
              asprintf_a("%s and %s multiply to %s and add to %s.", ri(r), ri(r2), ri(AC), ri(B)),
              "%s×%s=%s, %s+%s=%s", rp(r), rp(r2), ri(AC), ri(r), rp(r2), ri(B));
    steps_add(&s->det, "Split the middle term", "Write the middle term as two terms using those numbers.", "%s",
              ex_print(alg_sum_of(sp, 4)));
    Expr *be = alg_pexpr(&br);
    Expr *ge = poly_is_const(&gv, NULL) ? ex_int(g2) : alg_pexpr(&gv);
    Expr *pairs = ex_add2(alg_prod((Expr *[]){alg_pexpr(&gu), be}, 2), ex_mul2(ge, be));
    steps_add(&s->det, "Factorise each pair", "Take the common factor out of the first two and the last two terms.",
              "%s", ex_print(pairs));
    steps_add(&s->det, "Take out the common bracket", asprintf_a("(%s) is in both parts.", ex_print(be)), "%s",
              ex_print(alg_prod((Expr *[]){be, alg_pexpr(&other)}, 2)));
    return true;
}
