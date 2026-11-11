/* Fuller working for single operations inside Evaluate (called by calc_steps.c):
 * fractions (common denominator, multiply across, flip and multiply, HCF),
 * mixed numbers to improper, negative and fractional indices, surd
 * simplification, percentage of an amount, nCr / nPr formulas, n!. */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_util.h"

static bool val_rat(const CVal *v, Rat *r) { return v->exact && cu_rational(v->e, r); }
static const char *P(const Expr *e) { return ex_print(e); }
static const char *RD(Rat r) { return ex_print(cu_rat_disp(r)); }
/* a number as the user typed it when it is a plain number, else its exact value */
static Expr *as_typed(const Expr *orig, Rat r)
{
    if (orig->kind == E_NUM) return (Expr *)orig;
    Expr *d = cu_rat_disp(r);
    if (r.d != 1 || rat_is_neg(r)) d->flags |= EF_PAREN;
    return d;
}
/* n/d exactly as written, not simplified */
static Expr *frac_raw(int64_t n, int64_t d)
{
    Expr *f = ex_div2(ex_int(n < 0 ? -n : n), ex_int(d));
    return n < 0 ? ex_neg(f) : f;
}

bool cw_simplify_hcf(Rat shown_n, Rat shown_d, Steps *st)
{
    int64_t n = shown_n.n, d = shown_d.n, h = i64_gcd(n, d);
    if (d == 0 || h <= 1) return false;
    Rat r = rat_make(n, d);
    steps_add(st, asprintf_a("Simplify by the HCF %lld", (long long)h), "Divide the top and bottom by it", "%s=%s",
              P(frac_raw(n, d)), RD(r));
    return true;
}

bool cw_surd_step(int64_t n, Steps *st)
{
    int64_t k, m;
    if (n < 4 || !cu_sqrt_split(n, &k, &m) || k == 1) return false;
    if (m == 1) {
        steps_add(st, "Square root", asprintf_a("%lld = %lld\xC2\xB2", (long long)n, (long long)k), "\xE2\x88\x9A%lld=%lld",
                  (long long)n, (long long)k);
        return true;
    }
    steps_add(st, "Simplify the surd", asprintf_a("%lld is the biggest square factor", (long long)(k * k)),
              "\xE2\x88\x9A%lld=\xE2\x88\x9A(%lld\xC3\x97%lld)=%lld\xE2\x88\x9A%lld", (long long)n, (long long)(k * k),
              (long long)m, (long long)k, (long long)m);
    return true;
}

static bool mixed_steps(const Expr *e, Steps *st)
{
    bool any = false;
    for (int i = 0; i < e->nkids; i++) {
        const Expr *k = e->kids[i];
        if (k->kind == E_NEG) k = k->kids[0];
        if (k->kind != E_MIXED || k->nkids != 3) continue;
        Rat w = k->kids[0]->r, n = k->kids[1]->r, d = k->kids[2]->r, top, val;
        if (!rat_mul(w, d, &top) || !rat_add(top, n, &top) || !rat_div(top, d, &val)) continue;
        steps_add(st, "Change the mixed number to an improper fraction", "Whole number × bottom + top",
                  "%s=(%s\xC3\x97%s+%s)/%s=%s", P(k), cu_rat_str(w), cu_rat_str(d), cu_rat_str(n), cu_rat_str(d),
                  P(frac_raw(top.n, d.n)));
        any = true;
    }
    return any;
}

static bool frac_add(const Rat *r, int n, Steps *st)
{
    int64_t L = 1, S = 0;
    for (int i = 0; i < n; i++)
        if (!i64_lcm(L, r[i].d, &L)) return false;
    Expr **a = amalloc(sizeof(Expr *) * (size_t)n), **b = amalloc(sizeof(Expr *) * (size_t)n);
    bool same = true;
    for (int i = 0; i < n; i++) {
        int64_t k;
        if (!i64_mul_ok(r[i].n, L / r[i].d, &k) || !i64_add_ok(S, k, &S)) return false;
        a[i] = cu_rat_disp(r[i]);
        b[i] = frac_raw(k, L);
        same = same && r[i].d == L;
    }
    const char *lhs = P(ex_node(E_ADD, n, a)), *mid = P(ex_node(E_ADD, n, b));
    if (!same)
        steps_add(st, asprintf_a("Use a common denominator of %lld", (long long)L), "Make the bottoms the same",
                  "%s=%s", lhs, mid);
    steps_add(st, "Add the numerators", "Keep the bottom the same", "%s=%s", same ? lhs : mid, P(frac_raw(S, L)));
    cw_simplify_hcf(rat_int(S), rat_int(L), st);
    return true;
}

static bool frac_mul(const Rat *r, int n, Steps *st, const char *lhs)
{
    int64_t N = 1, D = 1;
    for (int i = 0; i < n; i++)
        if (!i64_mul_ok(N, r[i].n, &N) || !i64_mul_ok(D, r[i].d, &D)) return false;
    if (!lhs) {             /* "3/4×2/9": same value however it is grouped */
        lhs = "";
        for (int i = 0; i < n; i++) {
            Expr *a = cu_rat_disp(r[i]);
            a->flags |= r[i].n < 0 ? EF_PAREN : 0;
            lhs = asprintf_a("%s%s%s", lhs, i ? "\xC3\x97" : "", P(a));
        }
    }
    steps_add(st, "Multiply the tops and multiply the bottoms", "", "%s=%s", lhs, P(frac_raw(N, D)));
    cw_simplify_hcf(rat_int(N), rat_int(D), st);
    return true;
}

static bool index_steps(Rat b, Rat x, Steps *st)
{
    Expr *base = cu_rat_disp(b);
    if (b.d != 1 || rat_is_neg(b)) base->flags |= EF_PAREN;
    const char *lhs = P(ex_pow2(base, ex_num(x)));
    Rat ax = rat_abs(x), val;
    if (x.d == 1) {
        if (x.n >= 0) return false;
        Rat pos;
        if (!rat_pow_int(b, ax.n, &pos) || !rat_inv(pos, &val)) return false;
        if (b.d != 1) {
            Rat flip;
            if (!rat_inv(b, &flip)) return false;
            Expr *fb = cu_rat_disp(flip);
            fb->flags |= EF_PAREN;
            steps_add(st, "A negative power: flip the fraction", "", "%s=%s=%s", lhs,
                      P(ex_pow2(fb, ex_num(ax))), RD(val));
        } else {
            steps_add(st, "A negative power means 1 over", "", "%s=1/%s=1/%s", lhs, P(ex_pow2(base, ex_num(ax))),
                      cu_rat_str(pos));
        }
        return true;
    }
    if (rat_is_neg(b) || x.d > 64) return false;
    Rat root, p;
    if (!rat_root_exact(b, (int)x.d, &root) || !rat_pow_int(root, ax.n, &p)) return false;
    Expr *rt = x.d == 2 ? ex_func1(F_SQRT, cu_rat_disp(b)) : ex_func2(F_ROOT, ex_int(x.d), cu_rat_disp(b));
    rt->flags |= EF_PAREN;
    Expr *rd = cu_rat_disp(root);
    if (root.d != 1) rd->flags |= EF_PAREN;
    const char *rooted = ax.n == 1 ? P(rt) : P(ex_pow2(rt, ex_int(ax.n)));
    const char *powed = ax.n == 1 ? RD(root) : P(ex_pow2(rd, ex_int(ax.n)));
    const char *note = asprintf_a("The bottom of the power (%lld) is the root%s", (long long)x.d,
                                  ax.n == 1 ? "" : ", the top is the power");
    if (x.n < 0) {
        Rat v;
        if (!rat_inv(p, &v)) return false;
        steps_add(st, "Negative fractional power", asprintf_a("%s. Negative means 1 over", note),
                  "%s=1/%s=1/%s=%s", lhs, rooted, powed, RD(v));
    } else {
        steps_add(st, "Fractional power", note, "%s=%s=%s%s%s", lhs, rooted, powed, ax.n == 1 ? "" : "=",
                  ax.n == 1 ? "" : RD(p));
    }
    return true;
}

static bool small_fact(int64_t n, char *buf, size_t cap)
{
    if (n < 2 || n > 12) return false;
    size_t o = 0;
    for (int64_t i = n; i >= 1 && o + 8 < cap; i--) o += (size_t)snprintf(buf + o, cap - o, "%s%lld", i == n ? "" : "\xC3\x97", (long long)i);
    return true;
}

static int64_t fact(int64_t n) { int64_t f = 1; for (int64_t i = 2; i <= n; i++) f *= i; return f; }

static bool combi_steps(int f, int64_t n, int64_t r, const char *lhs, const CVal *val, Steps *st)
{
    const char *ans = P(val->e);
    if (n < 0 || r < 0 || r > n) return false;
    if (f == F_NCR) {
        const char *form = asprintf_a("%lld!/(%lld!\xC3\x97%lld!)", (long long)n, (long long)r, (long long)(n - r));
        if (n <= 20)
            steps_add(st, "Use nCr = n!/(r!(n-r)!)", "", "%s=%s=%lld/(%lld\xC3\x97%lld)=%s", lhs, form,
                      (long long)fact(n), (long long)fact(r), (long long)fact(n - r), ans);
        else
            steps_add(st, "Use nCr = n!/(r!(n-r)!)", "", "%s=%s=%s", lhs, form, ans);
    } else {
        const char *form = asprintf_a("%lld!/%lld!", (long long)n, (long long)(n - r));
        if (n <= 20)
            steps_add(st, "Use nPr = n!/(n-r)!", "", "%s=%s=%lld/%lld=%s", lhs, form, (long long)fact(n),
                      (long long)fact(n - r), ans);
        else
            steps_add(st, "Use nPr = n!/(n-r)!", "", "%s=%s=%s", lhs, form, ans);
    }
    return true;
}

bool cw_node(const Expr *e, const CVal *kids, int nk, const CVal *val, bool under_mul, Steps *st)
{
    Rat a, b, r[16];
    bool all_rat = nk <= 16;
    for (int i = 0; i < nk && all_rat; i++) all_rat = val_rat(&kids[i], &r[i]);
    bool frac = false;
    for (int i = 0; i < nk && all_rat; i++) frac = frac || r[i].d != 1;
    bool dec = cu_has_decimal(e);

    switch (e->kind) {
    case E_PCT:
        if (under_mul || !all_rat) return false;
        steps_add(st, "Percent means out of 100", "", "%s=%s", P(e), P(ex_div2(as_typed(e->kids[0], r[0]), ex_int(100))));
        if (r[0].d == 1) cw_simplify_hcf(r[0], rat_int(100), st);
        return true;
    case E_MUL:
        if (nk == 2 && e->kids[0]->kind == E_PCT && all_rat && val_rat(val, &a)) {
            Rat p;
            if (!rat_mul(r[0], rat_int(100), &p)) return false;
            const Expr *pk = e->kids[0]->kids[0];
            const char *amt = P(as_typed(e->kids[1], r[1]));
            steps_add(st, "Find the percentage", "Percent means out of 100", "%s of %s=%s\xC3\x97%s=%s",
                      P(e->kids[0]), amt, P(ex_div2(as_typed(pk, p), ex_int(100))), amt, RD(a));
            return true;
        }
        if (!all_rat || dec) return false;
        {
            bool m = mixed_steps(e, st);
            if (!frac) return m;
            return frac_mul(r, nk, st, NULL) || m;
        }
    case E_ADD:
        if (!all_rat || dec) return false;
        {
            bool m = mixed_steps(e, st);
            if (!frac) return m;
            return frac_add(r, nk, st) || m;
        }
    case E_DIV:
        if (!all_rat || dec || nk != 2 || rat_is_zero(r[1])) return false;
        mixed_steps(e, st);
        if (!frac) {
            if (!val_rat(val, &a) || a.d == 1) return false;
            return cw_simplify_hcf(r[0], r[1], st);
        }
        {
            Rat flip, two[2];
            if (!rat_inv(r[1], &flip)) return false;
            Expr *x = cu_rat_disp(r[0]), *y = cu_rat_disp(r[1]), *z = frac_raw(flip.n, flip.d);
            x->flags |= EF_PAREN;
            y->flags |= EF_PAREN;
            z->flags |= EF_PAREN;
            const char *f2 = asprintf_a("%s\xC3\x97%s", P(x), P(z));
            steps_add(st, "Flip the second fraction and multiply", "Dividing by a fraction = multiplying by its reciprocal",
                      "%s\xC3\xB7%s=%s", P(x), P(y), f2);
            two[0] = r[0];
            two[1] = flip;
            return frac_mul(two, 2, st, f2) || true;
        }
    case E_POW:
        if (!all_rat || nk != 2) return false;
        return index_steps(r[0], r[1], st);
    case E_FUNC:
        if (e->op == F_SQRT && all_rat && nk == 1 && r[0].d == 1) return cw_surd_step(r[0].n, st);
        if ((e->op == F_NCR || e->op == F_NPR) && all_rat && nk == 2 && r[0].d == 1 && r[1].d == 1) {
            Expr *sh = ex_func2(e->op, cu_rat_disp(r[0]), cu_rat_disp(r[1]));
            return combi_steps(e->op, r[0].n, r[1].n, P(sh), val, st);
        }
        return false;
    case E_FACT:
        if (all_rat && r[0].d == 1) {
            char buf[128];
            if (!small_fact(r[0].n, buf, sizeof buf) || !val_rat(val, &b)) return false;
            steps_add(st, "Factorial", "Multiply every whole number down to 1", "%s!=%s=%s", cu_rat_str(r[0]), buf,
                      cu_rat_str(b));
            return true;
        }
        return false;
    default:
        return false;
    }
}
