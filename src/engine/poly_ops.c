/* Polynomials: arithmetic, inspection, coefficients, substitution. */
#include <string.h>

#include "arena.h"
#include "poly_int.h"

static bool combine(const Poly *a, const Poly *b, bool sub, Poly *out)
{
    Poly x, y;
    if (!poly_unify(a, b, &x, &y)) return false;
    if (x.n + y.n > 2 * POLY_MAXTERMS) return false;
    Poly r = x;
    r.n = x.n + y.n;
    r.t = poly_terms(r.n);
    memcpy(r.t, x.t, sizeof(PolyTerm) * (size_t)x.n);
    for (int i = 0; i < y.n; i++) {
        r.t[x.n + i] = y.t[i];
        if (sub) {
            if (y.t[i].c.n == INT64_MIN) return false;
            r.t[x.n + i].c = rat_neg(y.t[i].c);
        }
    }
    if (!poly_norm(&r)) return false;
    *out = r;
    return true;
}

bool poly_add(const Poly *a, const Poly *b, Poly *out) { return combine(a, b, false, out); }
bool poly_sub(const Poly *a, const Poly *b, Poly *out) { return combine(a, b, true, out); }

bool poly_mul(const Poly *a, const Poly *b, Poly *out)
{
    Poly x, y;
    if (!poly_unify(a, b, &x, &y)) return false;
    long long cnt = (long long)x.n * y.n;
    if (cnt > 16LL * POLY_MAXTERMS) return false;
    Poly r = x;
    r.n = (int)cnt;
    r.t = poly_terms(r.n);
    int k = 0;
    for (int i = 0; i < x.n; i++)
        for (int j = 0; j < y.n; j++, k++) {
            if (!rat_mul(x.t[i].c, y.t[j].c, &r.t[k].c)) return false;
            for (int v = 0; v < POLY_MAXV; v++) {
                int e = x.t[i].e[v] + y.t[j].e[v];
                if (e > POLY_MAXEXP) return false;
                r.t[k].e[v] = (unsigned short)e;
            }
        }
    if (!poly_norm(&r)) return false;
    *out = r;
    return true;
}

bool poly_pow(const Poly *a, int k, Poly *out)
{
    if (k < 0 || k > POLY_MAXEXP) return false;
    Poly r = poly_const(rat_int(1)), b = *a, t;
    while (k > 0) {
        if (k & 1) {
            if (!poly_mul(&r, &b, &t)) return false;
            r = t;
        }
        k >>= 1;
        if (k > 0) {
            if (!poly_mul(&b, &b, &t)) return false;
            b = t;
        }
    }
    *out = r;
    return true;
}

bool poly_scale(const Poly *a, Rat k, Poly *out)
{
    Poly r = *a;
    r.t = poly_terms(a->n);
    for (int i = 0; i < a->n; i++) {
        r.t[i] = a->t[i];
        if (!rat_mul(a->t[i].c, k, &r.t[i].c)) return false;
    }
    if (!poly_norm(&r)) return false;
    *out = r;
    return true;
}

bool poly_neg(const Poly *a, Poly *out) { return poly_scale(a, rat_int(-1), out); }

bool poly_equal(const Poly *a, const Poly *b)
{
    if (a->n != b->n || a->nv != b->nv) return false;
    for (int i = 0; i < a->nv; i++)
        if (strcmp(a->v[i], b->v[i]) != 0) return false;
    for (int i = 0; i < a->n; i++)
        if (!rat_eq(a->t[i].c, b->t[i].c) || poly_term_cmp(&a->t[i], &b->t[i]) != 0) return false;
    return true;
}

bool poly_is_zero(const Poly *p) { return p->n == 0; }

bool poly_is_const(const Poly *p, Rat *c)
{
    if (p->nv != 0 || p->n > 1) return false;
    if (c) *c = p->n ? p->t[0].c : rat_int(0);
    return true;
}

int poly_total_degree(const Poly *p)
{
    int best = -1;
    for (int i = 0; i < p->n; i++) {
        int d = 0;
        for (int v = 0; v < p->nv; v++) d += p->t[i].e[v];
        if (d > best) best = d;
    }
    return best;
}

int poly_degree(const Poly *p, const char *var)
{
    if (p->n == 0) return -1;
    int v = poly_var_index(p, var), best = 0;
    for (int i = 0; v >= 0 && i < p->n; i++)
        if (p->t[i].e[v] > best) best = p->t[i].e[v];
    return best;
}

bool poly_coeff(const Poly *p, const char *var, int k, Poly *out)
{
    int v = poly_var_index(p, var);
    Poly r = *p;
    r.t = poly_terms(p->n);
    r.n = 0;
    for (int i = 0; i < p->n; i++) {
        int e = v >= 0 ? p->t[i].e[v] : 0;
        if (e != k) continue;
        r.t[r.n] = p->t[i];
        if (v >= 0) r.t[r.n].e[v] = 0;
        r.n++;
    }
    if (!poly_norm(&r)) return false;
    *out = r;
    return true;
}

bool poly_eval(const Poly *p, const char *var, Rat x, Poly *out)
{
    int v = poly_var_index(p, var);
    Poly r = *p;
    r.t = poly_terms(p->n);
    for (int i = 0; i < p->n; i++) {
        r.t[i] = p->t[i];
        if (v < 0 || p->t[i].e[v] == 0) continue;
        Rat pw;
        if (!rat_pow_int(x, p->t[i].e[v], &pw) || !rat_mul(r.t[i].c, pw, &r.t[i].c)) return false;
        r.t[i].e[v] = 0;
    }
    if (!poly_norm(&r)) return false;
    *out = r;
    return true;
}
