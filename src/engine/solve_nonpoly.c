/* Equations that are not polynomials: unknown in a denominator, √(...) = k,
 * b^(...) = k by matching powers (or logs), otherwise solved numerically. */
#include "solve_ext.h"
#include <math.h>
#include <string.h>

bool sv_numeric_fallback(SvCtx *c, const Expr *L, const Expr *R, const char *why, Expr **out)
{
    Expr *f = ex_add2(ex_clone(L), ex_neg(ex_clone(R)));
    bool trig = sv_has_trig(f, c->var), deg = c->s ? c->s->degrees : true;
    double top = deg ? 360 : 2 * M_PI, roots[SV_MAXROOTS];
    int n = trig ? sv_expr_roots_range(c, f, 0, top, roots, SV_MAXROOTS) : sv_expr_roots(c, f, roots, SV_MAXROOTS);
    if (trig) {      /* angles from 0 up to (not including) a full turn */
        int m = 0;
        for (int i = 0; i < n; i++)
            if (roots[i] < top - 1e-9 * top) roots[m++] = fabs(roots[i]) < 1e-9 ? 0 : roots[i];
        n = m;
    }
    bool huge = n > 6;
    for (int i = 0; i < n; i++) huge = huge || fabs(roots[i]) > 1e6;
    if (huge) return sv_fail(c, "I couldn't solve this equation.");
    const char *list = "";
    Expr *items[SV_MAXROOTS];
    for (int i = 0; i < n; i++) {
        items[i] = ex_rel(R_EQ, ex_var(c->var), sv_numexpr(c, roots[i]));
        list = i ? sv_fmt("%s \"or\" %s", list, ex_print(items[i])) : ex_print(items[i]);
    }
    sv_step(c, "Solve numerically",
            why ? why : trig ? (deg ? "This equation can't be solved exactly, so it was solved numerically for 0 â¤ x < 360."
                                    : "This equation can't be solved exactly, so it was solved numerically for 0 â¤ x < 2Ï.")
                             : "This equation can't be solved exactly by rearranging, so it was solved numerically.",
            n ? list : NULL);
    if (n == 0) {
        sv_step(c, "No solutions found", trig ? "No solutions found between 0 and a full turn."
                                              : "No real solutions between -1000000 and 1000000.", NULL);
        *out = ex_text("no real solutions found");
        return true;
    }
    *out = n == 1 ? items[0] : ex_node(E_LIST, n, items);
    return true;
}

static bool exact_num(const Expr *e, Rat *r)
{
    return sv_exact_num(e, r);
}

/* b = base^i with i as large as possible (base not itself a perfect power) */
static void root_base(Rat b, Rat *base, int *i)
{
    *base = b;
    *i = 1;
    for (int k = 6; k >= 2; k--) {
        Rat r;
        if (rat_root_exact(b, k, &r)) {
            int j;
            Rat rb;
            root_base(r, &rb, &j);
            *base = rb;
            *i = k * j;
            return;
        }
    }
}

static bool powers(SvCtx *c, const Expr *F, const Expr *K, Expr **out)
{
    Rat b, k, base, p;
    int ib;
    const Expr *h = F->kids[1];
    if (!exact_num(F->kids[0], &b) || !exact_num(K, &k)) return false;
    if (!rat_is_neg(b) && !rat_is_zero(b) && !rat_is_one(b) && !(rat_is_neg(k) || rat_is_zero(k))) {
        root_base(b, &base, &ib);
        for (int j = -63; j <= 63; j++) {
            if (!rat_pow_int(base, j, &p) || !rat_eq(p, k)) continue;
            Expr *eb = ex_num(base);
            Expr *lhs = ib == 1 ? ex_clone(F) : ex_pow2(eb, ex_mul2(ex_int(ib), ex_clone(h)));
            sv_step(c, ib == 1 ? sv_fmt("Write %s as a power of %s", sv_rat_str(k), sv_rat_str(base))
                               : sv_fmt("Write both sides as powers of %s", sv_rat_str(base)),
                    NULL, ex_print(ex_rel(R_EQ, lhs, ex_pow2(eb, ex_int(j)))));
            Expr *hl = ib == 1 ? ex_clone(h) : ex_mul2(ex_int(ib), ex_clone(h));
            sv_step(c, "The powers must be equal", NULL, ex_print(ex_rel(R_EQ, hl, ex_int(j))));
            return sv_equation(c, hl, ex_int(j), out);
        }
    }
    if (rat_is_neg(k) || rat_is_zero(k)) {
        if (!rat_is_neg(b)) {
            sv_step(c, "No solutions", sv_fmt("%s to any power is always positive.", sv_rat_str(b)), NULL);
            *out = ex_text("no real solutions");
            return true;
        }
        return false;
    }
    Poly hp;
    Rat hc[SV_MAXDEG + 1];
    int hd;
    if (rat_is_neg(b) || rat_is_one(b) || !poly_from_expr(h, &hp) || !sv_upoly(&hp, c->var, hc, &hd) || hd != 1)
        return false;
    double v = log(rat_to_double(k)) / log(rat_to_double(b));
    double x = (v - rat_to_double(hc[0])) / rat_to_double(hc[1]);
    sv_step(c, "Take logs of both sides", sv_fmt("log(%s,%s) means the power of %s that gives %s.",
            sv_rat_str(b), sv_rat_str(k), sv_rat_str(b), sv_rat_str(k)),
            ex_print(ex_rel(R_EQ, ex_clone(h), ex_func2(F_LOG, ex_num(b), ex_num(k)))));
    sv_step(c, "Solve", NULL, sv_fmt("%s=%s", c->var, sv_numdec(c, x)));
    *out = ex_rel(R_EQ, ex_var(c->var), sv_numexpr(c, x));
    return true;
}

/* b1^f = b2^g where both bases are powers of one number: equate the powers */
static bool same_base(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    Rat b1, b2, base1, base2;
    int i1, i2;
    if (L->kind != E_POW || R->kind != E_POW || ex_has_var(L->kids[0], c->var) || ex_has_var(R->kids[0], c->var)) return false;
    if (!exact_num(L->kids[0], &b1) || !exact_num(R->kids[0], &b2)) return false;
    if (rat_cmp(b1, rat_int(0)) <= 0 || rat_cmp(b2, rat_int(0)) <= 0 || rat_is_one(b1) || rat_is_one(b2)) return false;
    root_base(b1, &base1, &i1);
    root_base(b2, &base2, &i2);
    if (!rat_eq(base1, base2)) return false;
    Expr *e1 = ex_clone(L->kids[1]), *e2 = ex_clone(R->kids[1]);
    e1->flags &= ~(unsigned)EF_PAREN;
    e2->flags &= ~(unsigned)EF_PAREN;
    Expr *h1 = i1 == 1 ? e1 : ex_mul2(ex_int(i1), e1);
    Expr *h2 = i2 == 1 ? e2 : ex_mul2(ex_int(i2), e2);
    if (i1 != 1 || i2 != 1)
        sv_step(c, sv_fmt("Write both sides as powers of %s", sv_rat_str(base1)), NULL,
                ex_print(ex_rel(R_EQ, ex_pow2(ex_num(base1), ex_clone(h1)), ex_pow2(ex_num(base1), ex_clone(h2)))));
    sv_step(c, "The powers must be equal", "The bases are the same, so the powers are equal.", ex_print(ex_rel(R_EQ, h1, h2)));
    return sv_equation(c, h1, h2, out);
}

bool sv_nonpoly(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    if (!sv_rational_eq(c, L, R, out)) return false;
    if (*out) return true;
    bool lv = ex_has_var(L, c->var), rv = ex_has_var(R, c->var);
    if (lv && rv) {
        if (same_base(c, L, R, out) || sv_surd_eq(c, L, R, out)) return true;
        return sv_numeric_fallback(c, L, R, NULL, out);
    }
    if (sv_trig_eq(c, L, R, out)) return true;
    const Expr *F = lv ? L : R, *K = lv ? R : L;
    if (F->kind == E_FUNC && F->op == F_SQRT && F->nkids == 1) {
        Rat k;
        bool ex = exact_num(K, &k);
        if (ex && rat_is_neg(k)) {
            sv_step(c, "No solutions", "A square root can't be negative.", NULL);
            *out = ex_text("no real solutions");
            return true;
        }
        Expr *k2;
        Rat sq;
        if (!ex || !rat_mul(k, k, &sq)) k2 = ex_pow2(ex_clone(K), ex_int(2));
        else k2 = ex_num(sq);
        sv_step(c, "Square both sides", NULL, ex_print(ex_rel(R_EQ, ex_clone(F->kids[0]), k2)));
        return sv_equation(c, F->kids[0], k2, out);
    }
    if (F->kind == E_POW && !ex_has_var(F->kids[0], c->var) && powers(c, F, K, out)) return true;
    return sv_numeric_fallback(c, L, R, NULL, out);
}
