/* Factor theorem (rational roots p/q, then divide) and, when there are no linear factors,
 * a trial search for a quadratic factor (x^4+4 = (x^2-2x+2)(x^2+2x+2)). One variable only. */
#include "alg_int.h"

#include <stdlib.h>
#include <string.h>

#define MAXDEG 64
#define MAXDIV 64

/* Positive divisors of |n| in ascending order; false if there are more than max. */
static bool divisors(int64_t n, int64_t *d, int max, int *nd)
{
    int64_t pr[24];
    int ex[24];
    *nd = 0;
    n = llabs(n);
    if (n == 0) return false;
    d[(*nd)++] = 1;
    if (n == 1) return true;
    int np = i64_factorize(n, pr, ex, 24);
    for (int i = 0; i < np; i++) {
        int base = *nd;
        int64_t pk = 1;
        for (int e = 1; e <= ex[i]; e++) {
            pk *= pr[i];
            for (int j = 0; j < base; j++) {
                if (*nd >= max) return false;
                d[(*nd)++] = d[j] * pk;
            }
        }
    }
    for (int i = 1; i < *nd; i++)       /* insertion sort */
        for (int j = i; j > 0 && d[j - 1] > d[j]; j--) { int64_t t = d[j]; d[j] = d[j - 1]; d[j - 1] = t; }
    return true;
}

static bool int_upoly(const Poly *q, Rat *a, int *deg)
{
    if (q->nv != 1 || !poly_to_upoly(q, q->v[0], a, MAXDEG, deg)) return false;
    for (int i = 0; i <= *deg; i++)
        if (!rat_is_int(a[i])) return false;
    return a[0].n != 0;
}

bool fac_root(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Rat a[MAXDEG + 1];
    int deg, np, nq;
    int64_t dp[MAXDIV], dq[MAXDIV];
    if (!int_upoly(q, a, &deg) || deg < 2) return false;
    bool full = divisors(a[0].n, dp, MAXDIV, &np);
    full = divisors(a[deg].n, dq, MAXDIV, &nq) && full;
    const char *v = q->v[0];
    const char *tries[8];
    int ntry = 0, total = 0;
    for (int iq = 0; iq < nq; iq++) {
        for (int ip = 0; ip < np; ip++) {
            if (i64_gcd(dp[ip], dq[iq]) != 1) continue;
            for (int sg = 1; sg >= -1; sg -= 2) {
                Rat r = rat_make(sg * dp[ip], dq[iq]), val;
                if (!upoly_eval(a, deg, r, &val)) { full = false; continue; }
                if (ntry == 8) { memmove(tries, tries + 1, sizeof(tries[0]) * 7); ntry = 7; }
                tries[ntry++] = asprintf_a("f(%s)=%s", alg_rstr(r), alg_rstr(val));
                total++;
                if (!rat_is_zero(val)) continue;
                /* (q·x - p) is a factor */
                Poly L, Q, x = poly_var(v), qx, nl, nq2;
                if (!poly_scale(&x, rat_int(dq[iq]), &qx)) return false;
                Poly pc = poly_const(rat_int(sg * dp[ip]));
                if (!poly_sub(&qx, &pc, &L) || !poly_div_exact(q, &L, &Q)) return false;
                if (c->br[i].asc) {     /* keep 2-x style: (2-x)(4+2x+x^2) */
                    if (!poly_neg(&L, &nl) || !poly_neg(&Q, &nq2)) return false;
                    L = nl;
                    Q = nq2;
                }
                s->pc[0] = L;
                s->pc[1] = Q;
                s->n = 2;
                Expr *Le = c->br[i].asc ? alg_pexpr_asc(&L) : alg_pexpr(&L);
                Expr *Qe = c->br[i].asc ? alg_pexpr_asc(&Q) : alg_pexpr(&Q);
                const char *Ls = ex_print(Le);
                const char *list = tries[0];
                for (int j = 1; j < ntry; j++) list = asprintf_a("%s, %s", list, tries[j]);
                s->title = "Use the factor theorem";
                s->note = asprintf_a("f(%s)=0, so (%s) is a factor.", alg_rstr(r), Ls);
                steps_add(&s->det, "Try values in f(x)",
                          total > ntry ? "Try factors of the constant term (over factors of the first coefficient) until f(a)=0."
                                       : "If f(a)=0 then (x-a) is a factor (the factor theorem).",
                          "%s", list);
                steps_add(&s->det, asprintf_a("Divide by (%s)", Ls), "Divide (synthetic division) to find the other factor.",
                          "%s=%s", ex_print(c->br[i].asc ? alg_pexpr_asc(q) : alg_pexpr(q)),
                          ex_print(alg_prod((Expr *[]){Le, Qe}, 2)));
                return true;
            }
        }
    }
    if (full) {
        c->noroots = true;
        if (deg <= 3) {
            c->proven = true;
            if (!c->why) c->why = "No fraction p/q (p a factor of the constant, q of the first coefficient) gives f(x)=0, so there are no linear factors.";
        }
    }
    return false;
}

bool fac_quadfactor(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Rat a[MAXDEG + 1], f1, fm1;
    int deg, nA, nC, nF;
    int64_t dA[MAXDIV], dC[MAXDIV], dF[MAXDIV];
    if (!c->noroots || !int_upoly(q, a, &deg) || deg < 4) return false;
    if (!upoly_eval(a, deg, rat_int(1), &f1) || !upoly_eval(a, deg, rat_int(-1), &fm1)) return false;
    if (rat_is_zero(f1) || rat_is_zero(fm1)) return false;
    bool full = divisors(a[deg].n, dA, MAXDIV, &nA);
    full = divisors(a[0].n, dC, MAXDIV, &nC) && full;
    full = divisors(f1.n, dF, MAXDIV, &nF) && full;
    const char *v = q->v[0];
    for (int ia = 0; ia < nA; ia++)
        for (int ic = 0; ic < 2 * nC; ic++)
            for (int id = 0; id < 2 * nF; id++) {
                int64_t A = dA[ia], C = ic < nC ? dC[ic] : -dC[ic - nC];
                int64_t d = id < nF ? dF[id] : -dF[id - nF];
                int64_t B = d - A - C, e = A - B + C, D, sq, B2, AC;
                if (e == 0 || fm1.n % e) continue;
                if (!i64_mul_ok(B, B, &B2) || !i64_mul_ok(4 * A, C, &AC) || !i64_add_ok(B2, -AC, &D)) continue;
                if (D >= 0 && i64_isqrt_exact(D, &sq)) continue;
                Rat qc[3] = {rat_int(C), rat_int(B), rat_int(A)};
                Poly Qd = poly_from_upoly(qc, 2, v), R;
                if (!poly_div_exact(q, &Qd, &R)) continue;
                s->pc[0] = Qd;
                s->pc[1] = R;
                s->n = 2;
                s->title = "Find a quadratic factor";
                s->note = "There are no linear factors, so try quadratics ax^2+bx+c with a and c dividing the end coefficients.";
                steps_add(&s->det, "Divide by the quadratic", "It divides exactly, so it is a factor.", "%s=%s",
                          alg_pstr(q), ex_print(alg_prod((Expr *[]){alg_pexpr(&Qd), alg_pexpr(&R)}, 2)));
                return true;
            }
    if (full && deg <= 5) {
        c->proven = true;
        if (!c->why) c->why = "It has no linear or quadratic factors with whole-number coefficients.";
    }
    return false;
}
