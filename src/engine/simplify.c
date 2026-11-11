/* ex_simplify (dispatcher, functions, numbers) and ex_subst.
 * Sums: simplify_add.c. Products and powers: simplify_mul.c. Display: simplify_tidy.c. */
#include <math.h>
#include <string.h>

#include "arena.h"
#include "expr.h"
#include "simplify_int.h"

/* ---- Num ---- */

bool is_numexpr(const Expr *e) { return e && (e->kind == E_NUM || e->kind == E_REAL); }

Num num_of(const Expr *e)
{
    Num n = {0};
    if (e->kind == E_NUM) {
        n.exact = true;
        n.r = e->r;
        n.dec = (e->flags & NF_DECIMAL) != 0;
    } else {
        n.v = e->v;
    }
    return n;
}

Num num_rat(Rat r) { Num n = {true, false, r, 0}; return n; }
double num_val(Num a) { return a.exact ? rat_to_double(a.r) : a.v; }

static Num num_real(double v, bool dec) { Num n = {false, dec, {0, 1}, v}; return n; }

Num num_add(Num a, Num b)
{
    Rat r;
    if (a.exact && b.exact && rat_add(a.r, b.r, &r)) { Num n = {true, a.dec || b.dec, r, 0}; return n; }
    return num_real(num_val(a) + num_val(b), a.dec || b.dec);
}

Num num_mul(Num a, Num b)
{
    Rat r;
    if ((a.exact && a.r.n == 0) || (b.exact && b.r.n == 0)) return num_rat(rat_int(0));   /* exact 0 wins */
    if (a.exact && b.exact && rat_mul(a.r, b.r, &r)) { Num n = {true, a.dec || b.dec, r, 0}; return n; }
    return num_real(num_val(a) * num_val(b), a.dec || b.dec);
}

bool num_is(Num a, int64_t k) { return a.exact ? (a.r.d == 1 && a.r.n == k) : a.v == (double)k; }

Expr *num_expr(Num a)
{
    if (!a.exact) return ex_real(a.v);
    Expr *e = ex_num(a.r);
    if (a.dec && a.r.d != 1 && rat_terminates(a.r)) e->flags |= NF_DECIMAL;
    return e;
}

/* ---- functions ---- */

static bool rat_floor(Rat a, int64_t *out)
{
    int64_t q = a.n / a.d;
    if (a.n % a.d != 0 && a.n < 0) q--;
    *out = q;
    return true;
}

/* exact nCr / nPr; false on overflow */
static bool combi_exact(int f, int64_t n, int64_t r, int64_t *out)
{
    if (n < 0 || r < 0 || r > n) return false;
    int64_t v = 1;
    if (f == F_NCR) {
        if (r > n - r) r = n - r;
        /* v = C(n-r+i-1, i-1) times (n-r+i) is divisible by i; after taking
         * g = gcd(v, i) out, i/g is coprime to v/g so it divides (n-r+i). */
        for (int64_t i = 1; i <= r; i++) {
            int64_t g = i64_gcd(v, i);
            if (!i64_mul_ok(v / g, (n - r + i) / (i / g), &v)) return false;
        }
    } else {
        for (int64_t i = 0; i < r; i++)
            if (!i64_mul_ok(v, n - i, &v)) return false;
    }
    *out = v;
    return true;
}

static Expr *simp_func(Expr *f)
{
    Expr *a = f->nkids > 0 ? f->kids[0] : NULL;
    Expr *b = f->nkids > 1 ? f->kids[1] : NULL;
    switch (f->op) {
    case F_SQRT:
        if (f->nkids == 1) return simp_pow(a, ex_num(rat_make(1, 2)));
        break;
    case F_ROOT: {
        long long n;
        if (f->nkids == 2 && ex_is_int(a, &n) && n >= 1) return n == 1 ? b : simp_pow(b, ex_num(rat_make(1, n)));
        break;
    }
    case F_ABS:
        if (a && a->kind == E_NUM) return ex_num(rat_abs(a->r));
        if (a && a->kind == E_REAL) return ex_real(fabs(a->v));
        break;
    case F_FLOOR: case F_CEIL: case F_ROUND:
        if (a && a->kind == E_NUM) {
            Rat x = a->r;
            int64_t q;
            if (f->op == F_CEIL) { rat_floor(rat_neg(x), &q); return ex_int(-q); }
            if (f->op == F_FLOOR) { rat_floor(x, &q); return ex_int(q); }
            Rat h;                                  /* round half away from zero */
            if (rat_add(rat_abs(x), rat_make(1, 2), &h)) {
                rat_floor(h, &q);
                return ex_int(rat_is_neg(x) ? -q : q);
            }
        }
        if (a && a->kind == E_REAL)
            return ex_real(f->op == F_FLOOR ? floor(a->v) : (f->op == F_CEIL ? ceil(a->v) : round(a->v)));
        break;
    case F_NCR: case F_NPR: {
        long long n, r;
        int64_t v;
        if (ex_is_int(a, &n) && ex_is_int(b, &r) && r >= 0 && r <= n) {
            if (combi_exact(f->op, n, r, &v)) return ex_int(v);
            double d = 1;                           /* too big for exact: approximate */
            long long rr = (f->op == F_NCR && r > n - r) ? n - r : r;
            for (long long i = 0; i < rr && isfinite(d); i++)
                d = f->op == F_NCR ? d * (double)(n - i) / (double)(i + 1) : d * (double)(n - i);
            if (isfinite(d)) return ex_real(d);
        }
        break;
    }
    case F_LN:
        if (ex_is_one(a)) return ex_int(0);
        if (a && a->kind == E_CONST && a->op == C_E) return ex_int(1);
        if (a && a->kind == E_POW && a->kids[0]->kind == E_CONST && a->kids[0]->op == C_E) return a->kids[1];
        break;
    case F_LOG:
        if (f->nkids == 1 && ex_is_one(a)) return ex_int(0);
        if (f->nkids == 1 && a->kind == E_NUM && rat_eq(a->r, rat_int(10))) return ex_int(1);
        if (f->nkids == 2 && ex_is_one(b)) return ex_int(0);
        if (f->nkids == 2 && ex_equal(a, b)) return ex_int(1);
        break;
    case F_EXP:
        if (ex_is_zero(a)) return ex_int(1);
        break;
    default:
        break;
    }
    return f;
}

static Expr *simp_fact(Expr *a)
{
    long long n;
    if (ex_is_int(a, &n) && n >= 0) {
        if (n <= 20) {
            int64_t v = 1;
            for (int64_t i = 2; i <= n; i++) v *= i;
            return ex_int(v);
        }
        if (n <= 170) {
            double v = 1;
            for (int i = 2; i <= n; i++) v *= i;
            return ex_real(v);
        }
    }
    return ex_node(E_FACT, 1, &a);
}

/* ---- dispatcher ---- */

Expr *ex_simplify(const Expr *e)
{
    if (!e) return NULL;
    switch (e->kind) {
    case E_NUM: case E_REAL: case E_VAR: case E_CONST: case E_TEXT: {
        Expr *c = ex_clone(e);
        c->flags &= ~(unsigned)EF_PAREN;
        return c;
    }
    case E_RECUR: return ex_num(e->r);
    case E_MIXED: {
        if (e->nkids != 3 || e->kids[2]->kind != E_NUM || ex_is_zero(e->kids[2])) break;
        Expr *w = ex_simplify(e->kids[0]), *n = ex_simplify(e->kids[1]), *d = e->kids[2];
        Expr *k[2] = {n, simp_pow(ex_clone(d), ex_int(-1))};
        Expr *frac = simp_mul_list(k, 2);
        if (is_numexpr(w) && num_val(num_of(w)) < 0) frac = simp_mul_list((Expr *[]){ex_int(-1), frac}, 2);
        Expr *s[2] = {w, frac};
        return simp_add_list(s, 2);
    }
    default:
        break;
    }
    int n = e->nkids;
    Expr **k = amalloc(sizeof(Expr *) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) k[i] = ex_simplify(e->kids[i]);
    switch (e->kind) {
    case E_ADD: return simp_add_list(k, n);
    case E_MUL: return simp_mul_list(k, n);
    case E_NEG: return simp_mul_list((Expr *[]){ex_int(-1), k[0]}, 2);
    case E_DIV:
        if (ex_is_zero(k[1])) return ex_div2(k[0], k[1]);
        return simp_mul_list((Expr *[]){k[0], simp_pow(k[1], ex_int(-1))}, 2);
    case E_POW: return simp_pow(k[0], k[1]);
    case E_PCT: return simp_mul_list((Expr *[]){k[0], ex_num(rat_make(1, 100))}, 2);
    case E_FACT: return simp_fact(k[0]);
    case E_FUNC: {
        Expr *f = ex_node(E_FUNC, n, k);
        f->op = e->op;
        return simp_func(f);
    }
    default: {
        Expr *c = ex_node(e->kind, n, k);
        c->op = e->op;
        memcpy(c->ops, e->ops, sizeof c->ops);
        c->flags = e->flags & ~(unsigned)EF_PAREN;
        return c;
    }
    }
}

/* ---- substitution ---- */

Expr *ex_subst(const Expr *e, const char *name, const Expr *val)
{
    if (!e) return NULL;
    if (e->kind == E_VAR && name && strcmp(e->name, name) == 0) return ex_clone(val);
    Expr *c = ex_clone(e);
    for (int i = 0; i < c->nkids; i++) c->kids[i] = ex_subst(e->kids[i], name, val);
    return c;
}
