/* SurdSum: exact numbers of the form c0 + c1·√b1 + c2·√b2 + ... (c rational,
 * b squarefree). Enough for GCSE surds: simplify, collect, expand brackets,
 * rationalise a one- or two-term denominator. Every operation reports overflow
 * (false) so the caller can fall back to ex_simplify / decimals. */
#include <math.h>
#include <string.h>

#include "arena.h"
#include "calc_util.h"

bool cu_sqrt_split(int64_t n, int64_t *k, int64_t *m)
{
    if (n < 0) return false;
    if (n == 0) { *k = 0; *m = 1; return true; }
    int64_t kk = 1, mm = 1, rest = n;
    for (int64_t p = 2; p <= 100000 && p <= rest / p; p += (p == 2 ? 1 : 2)) {
        if (rest % p) continue;
        int e = 0;
        while (rest % p == 0) { rest /= p; e++; }
        for (int i = 0; i < e / 2; i++) kk *= p;
        if (e % 2) mm *= p;
    }
    int64_t root;
    if (rest > 1 && i64_isqrt_exact(rest, &root)) {
        if (!i64_mul_ok(kk, root, &kk)) return false;
    } else if (!i64_mul_ok(mm, rest, &mm)) {
        return false;
    }
    *k = kk;
    *m = mm;
    return true;
}

void ss_rat(Rat r, SurdSum *out)
{
    out->n = 0;
    if (!rat_is_zero(r)) { out->t[0].c = r; out->t[0].b = 1; out->n = 1; }
}

bool ss_has_surd(const SurdSum *s)
{
    for (int i = 0; i < s->n; i++)
        if (s->t[i].b != 1) return true;
    return false;
}

bool ss_rational(const SurdSum *s, Rat *r)
{
    if (ss_has_surd(s)) return false;
    *r = s->n ? s->t[0].c : rat_int(0);
    return true;
}

double ss_value(const SurdSum *s)
{
    double v = 0;
    for (int i = 0; i < s->n; i++) v += rat_to_double(s->t[i].c) * sqrt((double)s->t[i].b);
    return v;
}

/* add one term into s (merging like surds), keeping order by b */
static bool put(SurdSum *s, Rat c, int64_t b)
{
    if (rat_is_zero(c)) return true;
    int i = 0;
    while (i < s->n && s->t[i].b < b) i++;
    if (i < s->n && s->t[i].b == b) {
        Rat sum;
        if (!rat_add(s->t[i].c, c, &sum)) return false;
        if (rat_is_zero(sum)) {
            memmove(&s->t[i], &s->t[i + 1], sizeof(SurdTerm) * (size_t)(s->n - i - 1));
            s->n--;
        } else {
            s->t[i].c = sum;
        }
        return true;
    }
    if (s->n >= SS_MAX) return false;
    memmove(&s->t[i + 1], &s->t[i], sizeof(SurdTerm) * (size_t)(s->n - i));
    s->t[i].c = c;
    s->t[i].b = b;
    s->n++;
    return true;
}

bool ss_add(const SurdSum *a, const SurdSum *b, SurdSum *out)
{
    SurdSum r = *a;
    for (int i = 0; i < b->n; i++)
        if (!put(&r, b->t[i].c, b->t[i].b)) return false;
    *out = r;
    return true;
}

bool ss_mul(const SurdSum *a, const SurdSum *b, SurdSum *out)
{
    SurdSum r = {.n = 0};
    for (int i = 0; i < a->n; i++)
        for (int j = 0; j < b->n; j++) {
            int64_t b1 = a->t[i].b, b2 = b->t[j].b, g = i64_gcd(b1, b2), m;
            Rat c;
            if (!rat_mul(a->t[i].c, b->t[j].c, &c) || !rat_mul(c, rat_int(g), &c)) return false;
            if (!i64_mul_ok(b1 / g, b2 / g, &m)) return false;
            if (!put(&r, c, m)) return false;
        }
    *out = r;
    return true;
}

static bool ss_scale(const SurdSum *a, Rat k, SurdSum *out)
{
    SurdSum r = {.n = 0};
    for (int i = 0; i < a->n; i++) {
        Rat c;
        if (!rat_mul(a->t[i].c, k, &c) || !put(&r, c, a->t[i].b)) return false;
    }
    *out = r;
    return true;
}

bool ss_div(const SurdSum *a, const SurdSum *b, SurdSum *out)
{
    Rat r, inv;
    if (b->n == 0) return false;
    if (ss_rational(b, &r)) return rat_inv(r, &inv) && ss_scale(a, inv, out);
    SurdSum conj, top, bot;
    if (b->n == 1) {
        conj.n = 1;
        conj.t[0].c = rat_int(1);
        conj.t[0].b = b->t[0].b;
    } else if (b->n == 2) {
        conj = *b;
        conj.t[1].c = rat_neg(conj.t[1].c);
    } else {
        return false;
    }
    if (!ss_mul(a, &conj, &top) || !ss_mul(b, &conj, &bot) || !ss_rational(&bot, &r)) return false;
    return rat_inv(r, &inv) && ss_scale(&top, inv, out);
}

static bool ss_sqrt(Rat r, SurdSum *out)
{
    if (rat_is_neg(r)) return false;
    int64_t pq, k, m;
    if (!i64_mul_ok(r.n, r.d, &pq) || !cu_sqrt_split(pq, &k, &m)) return false;
    out->n = 0;
    return put(out, rat_make(k, r.d), m);
}

static bool from(const Expr *e, SurdSum *o, int depth)
{
    if (depth > 60) return false;
    SurdSum a, b;
    Rat r;
    switch (e->kind) {
    case E_NUM: case E_RECUR: ss_rat(e->r, o); return true;
    case E_MIXED: {
        if (e->nkids != 3 || e->kids[2]->kind != E_NUM || rat_is_zero(e->kids[2]->r)) return false;
        Rat f;
        if (!rat_div(e->kids[1]->r, e->kids[2]->r, &f) || !rat_add(e->kids[0]->r, f, &r)) return false;
        ss_rat(r, o);
        return true;
    }
    case E_NEG:
        if (!from(e->kids[0], &a, depth + 1)) return false;
        return ss_scale(&a, rat_int(-1), o);
    case E_PCT:
        if (!from(e->kids[0], &a, depth + 1)) return false;
        return ss_scale(&a, RAT(1, 100), o);
    case E_ADD: case E_MUL:
        if (!from(e->kids[0], o, depth + 1)) return false;
        for (int i = 1; i < e->nkids; i++) {
            if (!from(e->kids[i], &b, depth + 1)) return false;
            a = *o;
            if (!(e->kind == E_ADD ? ss_add(&a, &b, o) : ss_mul(&a, &b, o))) return false;
        }
        return true;
    case E_DIV:
        return from(e->kids[0], &a, depth + 1) && from(e->kids[1], &b, depth + 1) && ss_div(&a, &b, o);
    case E_POW: {
        SurdSum x;
        Rat k;
        if (!from(e->kids[1], &x, depth + 1) || !ss_rational(&x, &k)) return false;
        if (k.d == 2) {
            if (!from(e->kids[0], &a, depth + 1) || !ss_rational(&a, &r) || !ss_sqrt(r, &a)) return false;
            k = rat_int(k.n);
        } else if (k.d == 1) {
            if (!from(e->kids[0], &a, depth + 1)) return false;
        } else {
            return false;
        }
        if (k.n > 64 || k.n < -64) return false;
        if (k.n < 0) {
            SurdSum one;
            ss_rat(rat_int(1), &one);
            if (!ss_div(&one, &a, &b)) return false;
            a = b;
            k.n = -k.n;
        }
        ss_rat(rat_int(1), o);
        for (int64_t i = 0; i < k.n; i++) {
            b = *o;
            if (!ss_mul(&b, &a, o)) return false;
        }
        return true;
    }
    case E_FUNC:
        if (e->op == F_SQRT && e->nkids == 1)
            return from(e->kids[0], &a, depth + 1) && ss_rational(&a, &r) && ss_sqrt(r, o);
        if (e->op == F_ROOT && e->nkids == 2) {
            Rat n, root;
            if (!from(e->kids[0], &a, depth + 1) || !ss_rational(&a, &n) || !from(e->kids[1], &b, depth + 1) ||
                !ss_rational(&b, &r))
                return false;
            if (n.d == 1 && n.n == 2) return ss_sqrt(r, o);
            if (n.d != 1 || n.n < 2 || n.n > 64) return false;
            if (rat_is_neg(r)) {
                if (n.n % 2 == 0 || !rat_root_exact(rat_neg(r), (int)n.n, &root)) return false;
                root = rat_neg(root);
            } else if (!rat_root_exact(r, (int)n.n, &root)) {
                return false;
            }
            ss_rat(root, o);
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool ss_from_expr(const Expr *e, SurdSum *out)
{
    SurdSum r;
    if (!e || !from(e, &r, 0)) return false;
    *out = r;
    return true;
}

/* integer coefficient k times √b, sign handled by caller */
static Expr *term_expr(int64_t k, int64_t b)
{
    if (b == 1) return ex_int(k);
    Expr *s = ex_func1(F_SQRT, ex_int(b));
    return k == 1 ? s : ex_mul2(ex_int(k), s);
}

Expr *ss_to_expr(const SurdSum *s)
{
    if (s->n == 0) return ex_int(0);
    int64_t L = 1;
    for (int i = 0; i < s->n; i++)
        if (!i64_lcm(L, s->t[i].c.d, &L)) return ex_real(ss_value(s));
    int order[SS_MAX], no = 0, first = 0;
    for (int i = 0; i < s->n; i++)
        if (!rat_is_neg(s->t[i].c)) { first = i; break; }
    order[no++] = first;
    for (int i = 0; i < s->n; i++)
        if (i != first) order[no++] = i;
    Expr *kids[SS_MAX];
    for (int j = 0; j < no; j++) {
        const SurdTerm *t = &s->t[order[j]];
        int64_t k;
        if (!i64_mul_ok(t->c.n, L / t->c.d, &k) || k == INT64_MIN) return ex_real(ss_value(s));
        Expr *x = term_expr(k < 0 ? -k : k, t->b);
        kids[j] = k < 0 ? ex_neg(x) : x;
    }
    if (no == 1) {
        Expr *x = kids[0];
        if (L == 1) return x;
        bool neg = x->kind == E_NEG;
        Expr *d = ex_div2(neg ? x->kids[0] : x, ex_int(L));
        return neg ? ex_neg(d) : d;
    }
    Expr *sum = ex_node(E_ADD, no, kids);
    return L == 1 ? sum : ex_div2(sum, ex_int(L));
}
