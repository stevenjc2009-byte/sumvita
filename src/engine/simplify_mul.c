/* Simplifier: products and powers (index laws, exact powers and roots, surds). */
#include <math.h>
#include <stdlib.h>

#include "arena.h"
#include "expr.h"
#include "simplify_int.h"

#define SURD_LIMIT 1000000000000LL   /* factorise nothing bigger (trial division) */
#define MUL_ROUNDS 6

static Expr *pow_raw(Expr *b, Expr *e)
{
    Expr *k[2] = {b, e};
    return ex_node(E_POW, 2, k);
}

/* n^p for integer n >= 1 and non-integer p = a/q: exact root, or k·A·B^(1/q') */
static Expr *int_root_pow(int64_t n, Rat p)
{
    if (n == 1) return ex_int(1);
    Rat r, out;
    if (p.d <= 64 && rat_root_exact(rat_int(n), (int)p.d, &r)) {
        if (rat_pow_int(r, p.n, &out)) return ex_num(out);
        return ex_real(pow(rat_to_double(r), (double)p.n));
    }
    int64_t q = p.d, k = p.n / q, rr = p.n % q;
    if (rr < 0) { rr += q; k--; }
    int64_t m = 1;
    for (int64_t i = 0; i < rr; i++)
        if (!i64_mul_ok(m, n, &m) || m >= SURD_LIMIT) return pow_raw(ex_int(n), ex_num(p));
    int64_t pr[64];
    int ex[64];
    int np = i64_factorize(m, pr, ex, 64);
    Rat coef;
    if (k < -64 || k > 64 || !rat_pow_int(rat_int(n), k, &coef)) return pow_raw(ex_int(n), ex_num(p));
    int64_t g = q, B = 1;
    for (int i = 0; i < np; i++) {
        for (int64_t a = ex[i] / q; a > 0; a--)
            if (!rat_mul(coef, rat_int(pr[i]), &coef)) return pow_raw(ex_int(n), ex_num(p));
        g = i64_gcd(g, ex[i] % q);
    }
    for (int i = 0; i < np; i++)
        for (int64_t b = (ex[i] % q) / g; b > 0; b--) B *= pr[i];   /* B <= m: no overflow */
    if (B == 1) return ex_num(coef);
    Expr *s = pow_raw(ex_int(B), ex_num(rat_make(1, q / g)));
    if (rat_is_one(coef)) return s;
    Expr *kk[2] = {ex_num(coef), s};
    return ex_node(E_MUL, 2, kk);
}

/* numeric base ^ numeric exponent */
static Expr *pow_num(Expr *b, Expr *e)
{
    Num nb = num_of(b), ne = num_of(e);
    if (!nb.exact || !ne.exact) {
        double v = pow(num_val(nb), num_val(ne));
        return isnan(v) ? pow_raw(b, e) : ex_real(v);
    }
    Rat x = nb.r, p = ne.r, out;
    if (p.d == 1) {
        if (p.n <= 4096 && p.n >= -4096 && rat_pow_int(x, p.n, &out)) {
            Num r = {true, nb.dec, out, 0};
            return num_expr(r);
        }
        return ex_real(pow(rat_to_double(x), (double)p.n));
    }
    if (rat_is_neg(x)) {
        if (p.d % 2 == 0) return pow_raw(b, e);         /* not real: leave it */
        Expr *r = pow_num(ex_num(rat_neg(x)), e);
        return (p.n % 2) ? simp_mul_list((Expr *[]){ex_int(-1), r}, 2) : r;
    }
    if (x.d != 1) {
        Expr *k[2] = {int_root_pow(x.n, p), int_root_pow(x.d, rat_neg(p))};
        return simp_mul_list(k, 2);
    }
    return int_root_pow(x.n, p);
}

/* (b^a)^n -> b^(an) is safe unless a has an even numerator and n is not an integer */
static bool can_nest(const Expr *a, Num n)
{
    if (n.exact && n.r.d == 1) return true;
    return a->kind == E_NUM && a->r.n % 2 != 0;
}

Expr *simp_pow(Expr *b, Expr *e)
{
    if (ex_is_zero(e)) return ex_int(1);
    if (ex_is_one(e)) return b;
    if (ex_is_one(b)) return ex_int(1);
    if (!is_numexpr(e)) return pow_raw(b, e);
    Num ne = num_of(e);
    if (ex_is_zero(b)) return num_val(ne) > 0 ? ex_int(0) : pow_raw(b, e);
    if (is_numexpr(b)) return pow_num(b, e);
    if (b->kind == E_POW && is_numexpr(b->kids[1]) && can_nest(b->kids[1], ne))
        return simp_pow(b->kids[0], num_expr(num_mul(num_of(b->kids[1]), ne)));
    if (b->kind == E_MUL) {
        bool ok = (ne.exact && ne.r.d == 1) || !is_numexpr(b->kids[0]) || num_val(num_of(b->kids[0])) > 0;
        if (ok) {
            Expr **k = amalloc(sizeof(Expr *) * (size_t)b->nkids);
            for (int i = 0; i < b->nkids; i++) k[i] = simp_pow(b->kids[i], e);
            return simp_mul_list(k, b->nkids);
        }
    }
    return pow_raw(b, e);
}

/* ---- products ---- */

static int count_flat(Expr **k, int n)
{
    int c = 0;
    for (int i = 0; i < n; i++) c += k[i]->kind == E_MUL ? count_flat(k[i]->kids, k[i]->nkids) : 1;
    return c;
}

static void push_flat(Expr **k, int n, Expr **out, int *m)
{
    for (int i = 0; i < n; i++) {
        if (k[i]->kind == E_MUL) push_flat(k[i]->kids, k[i]->nkids, out, m);
        else out[(*m)++] = k[i];
    }
}

static Expr *fac_base(const Expr *f) { return f->kind == E_POW ? f->kids[0] : (Expr *)f; }
static Expr *fac_exp(const Expr *f) { return f->kind == E_POW ? f->kids[1] : NULL; }

static int fac_rank(const Expr *b)
{
    switch (b->kind) {
    case E_NUM: case E_REAL: return 0;   /* surds */
    case E_CONST: return 1;
    case E_VAR: return 2;
    default: return 3;
    }
}

static int fac_cmp(const void *pa, const void *pb)
{
    const Expr *a = *(Expr *const *)pa, *b = *(Expr *const *)pb;
    const Expr *ba = fac_base(a), *bb = fac_base(b);
    int ra = fac_rank(ba), rb = fac_rank(bb);
    if (ra != rb) return ra < rb ? -1 : 1;
    int c = ex_compare(ba, bb);
    return c ? c : ex_compare(fac_exp(a), fac_exp(b));
}

static bool pos_int(const Expr *e, int64_t *v)
{
    if (!e || e->kind != E_NUM || e->r.d != 1 || e->r.n < 2) return false;
    *v = e->r.n;
    return true;
}

/* (c/d)·(k·S) with S a sum whose integer content shares a factor with d: cancel it */
static Expr *cancel_into_sum(Num *coef, Expr *s)
{
    if (!coef->exact || coef->r.d == 1 || s->kind != E_ADD) return s;
    int64_t g = 0;
    for (int i = 0; i < s->nkids; i++) {
        Expr *rest;
        Num c = term_split(s->kids[i], &rest);
        if (!c.exact || c.r.d != 1) return s;
        g = i64_gcd(g, c.r.n);
    }
    int64_t h = i64_gcd(g, coef->r.d);
    if (h <= 1) return s;
    Expr **t = amalloc(sizeof(Expr *) * (size_t)s->nkids);
    for (int i = 0; i < s->nkids; i++) t[i] = simp_mul_list((Expr *[]){ex_num(rat_make(1, h)), s->kids[i]}, 2);
    *coef = num_mul(*coef, num_rat(rat_int(h)));
    return simp_add_list(t, s->nkids);
}

static Expr *mul_core(Expr **kids, int n, int round)
{
    int cap = count_flat(kids, n);
    Expr **f = amalloc(sizeof(Expr *) * (size_t)(cap + 1));
    int m = 0;
    push_flat(kids, n, f, &m);

    Num coef = num_rat(rat_int(1));
    Expr **base = amalloc(sizeof(Expr *) * (size_t)(m + 1));
    Expr ***exps = amalloc(sizeof(Expr **) * (size_t)(m + 1));
    int *nexp = amalloc(sizeof(int) * (size_t)(m + 1));
    int ng = 0;
    for (int i = 0; i < m; i++) {
        if (is_numexpr(f[i])) { coef = num_mul(coef, num_of(f[i])); continue; }
        Expr *b = fac_base(f[i]), *e = fac_exp(f[i]);
        int j = 0;
        while (j < ng && !ex_equal(base[j], b)) j++;
        if (j == ng) { base[ng] = b; exps[ng] = amalloc(sizeof(Expr *) * (size_t)m); nexp[ng] = 0; ng++; }
        exps[j][nexp[j]++] = e ? e : ex_int(1);
    }
    if (coef.exact && coef.r.n == 0) return ex_int(0);

    Expr **ex = amalloc(sizeof(Expr *) * (size_t)(ng + 1));
    for (int j = 0; j < ng; j++) ex[j] = nexp[j] == 1 ? exps[j][0] : simp_add_list(exps[j], nexp[j]);
    /* √2·√3 -> √6: merge integer bases that share the same fractional exponent */
    for (int j = 0; j < ng; j++) {
        int64_t bj, bk;
        if (!base[j] || !pos_int(base[j], &bj) || ex[j]->kind != E_NUM || ex[j]->r.d == 1) continue;
        for (int k = j + 1; k < ng; k++) {
            if (!base[k] || !pos_int(base[k], &bk) || !ex_equal(ex[j], ex[k])) continue;
            int64_t p;
            if (!i64_mul_ok(bj, bk, &p) || p >= SURD_LIMIT) continue;
            bj = p;
            base[j] = ex_int(p);
            base[k] = NULL;
        }
    }

    Expr **res = amalloc(sizeof(Expr *) * (size_t)(ng + 1));
    int room = 2;
    for (int j = 0; j < ng; j++) {
        res[j] = base[j] ? simp_pow(base[j], ex[j]) : NULL;
        if (res[j]) room += res[j]->kind == E_MUL ? res[j]->nkids : 1;
    }
    Expr **out = amalloc(sizeof(Expr *) * (size_t)room);
    int no = 0;
    bool again = false;
    for (int j = 0; j < ng; j++) {
        Expr *r = res[j];
        if (!r) continue;
        if (is_numexpr(r)) { coef = num_mul(coef, num_of(r)); continue; }
        if (r->kind == E_MUL) {
            again = true;
            for (int i = 0; i < r->nkids; i++) out[no++] = r->kids[i];
            continue;
        }
        if (r->kind == E_POW && !ex_equal(r->kids[0], base[j])) again = true;
        out[no++] = r;
    }
    if (coef.exact && coef.r.n == 0) return ex_int(0);
    if (again && round < MUL_ROUNDS) {
        out[no++] = num_expr(coef);
        return mul_core(out, no, round + 1);
    }

    int nadd = 0, at = -1;
    for (int i = 0; i < no; i++)
        if (out[i]->kind == E_ADD) { nadd++; at = i; }
    if (nadd == 1) out[at] = cancel_into_sum(&coef, out[at]);

    if (no == 0) return num_expr(coef);
    if (no == 1 && num_is(coef, 1)) return out[0];
    if (no == 1 && out[0]->kind == E_ADD && num_is(coef, -1)) {   /* -(a+b) -> -a-b */
        Expr *s = out[0], **t = amalloc(sizeof(Expr *) * (size_t)s->nkids);
        for (int i = 0; i < s->nkids; i++) t[i] = simp_mul_list((Expr *[]){ex_int(-1), s->kids[i]}, 2);
        return simp_add_list(t, s->nkids);
    }
    qsort(out, (size_t)no, sizeof(Expr *), fac_cmp);
    if (num_is(coef, 1)) return ex_node(E_MUL, no, out);
    Expr **all = amalloc(sizeof(Expr *) * (size_t)(no + 1));
    all[0] = num_expr(coef);
    for (int i = 0; i < no; i++) all[i + 1] = out[i];
    return ex_node(E_MUL, no + 1, all);
}

Expr *simp_mul_list(Expr **kids, int n)
{
    if (n == 0) return ex_int(1);
    return mul_core(kids, n, 0);
}
