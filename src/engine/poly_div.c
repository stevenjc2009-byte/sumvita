/* Polynomials: content, monomial factors, exact division, univariate helpers. */
#include <string.h>

#include "arena.h"
#include "poly_int.h"

bool poly_content(const Poly *p, Rat *out)
{
    int64_t g = 0, l = 1;
    for (int i = 0; i < p->n; i++) {
        g = i64_gcd(g, p->t[i].c.n);
        if (!i64_lcm(l, p->t[i].c.d, &l)) return false;
    }
    *out = rat_make(g, l);
    return true;
}

Poly poly_monomial_gcd(const Poly *p)
{
    Poly m = poly_const(rat_int(1));
    if (p->n == 0) return m;
    m.nv = p->nv;
    memcpy(m.v, p->v, sizeof m.v);
    for (int v = 0; v < p->nv; v++) {
        int lo = p->t[0].e[v];
        for (int i = 1; i < p->n; i++)
            if (p->t[i].e[v] < lo) lo = p->t[i].e[v];
        m.t[0].e[v] = (unsigned short)lo;
    }
    poly_norm(&m);                      /* one term: cannot fail */
    return m;
}

bool poly_div_monomial(const Poly *p, const Poly *m, Poly *out)
{
    Poly x, y;
    if (m->n != 1 || !poly_unify(p, m, &x, &y)) return false;
    Poly r = x;
    r.t = poly_terms(x.n);
    for (int i = 0; i < x.n; i++) {
        r.t[i] = x.t[i];
        if (!rat_div(x.t[i].c, y.t[0].c, &r.t[i].c)) return false;
        for (int v = 0; v < x.nv; v++) {
            if (x.t[i].e[v] < y.t[0].e[v]) return false;
            r.t[i].e[v] = (unsigned short)(x.t[i].e[v] - y.t[0].e[v]);
        }
    }
    if (!poly_norm(&r)) return false;
    *out = r;
    return true;
}

static Poly lead(const Poly *p)
{
    Poly m = *p;
    m.n = 1;
    m.t = poly_terms(1);
    m.t[0] = p->t[0];
    poly_norm(&m);
    return m;
}

bool poly_div_exact(const Poly *a, const Poly *b, Poly *out)
{
    if (b->n == 0) return false;
    Poly r = *a, q = poly_zero(), lb = lead(b);
    for (int it = 0; r.n > 0; it++) {
        Poly lr = lead(&r), t, tb, nr, nq;
        if (it > 4 * POLY_MAXTERMS || !poly_div_monomial(&lr, &lb, &t)) return false;
        if (!poly_mul(&t, b, &tb) || !poly_sub(&r, &tb, &nr) || !poly_add(&q, &t, &nq)) return false;
        r = nr;
        q = nq;
    }
    *out = q;
    return true;
}

/* ---- univariate ---- */

bool poly_to_upoly(const Poly *p, const char *var, Rat *c, int maxdeg, int *deg)
{
    if (p->nv > 1 || (p->nv == 1 && (!var || strcmp(p->v[0], var) != 0))) return false;
    int d = p->n ? poly_degree(p, var) : 0;
    if (d > maxdeg) return false;
    for (int i = 0; i <= d; i++) c[i] = rat_int(0);
    for (int i = 0; i < p->n; i++) c[p->nv ? p->t[i].e[0] : 0] = p->t[i].c;
    *deg = d;
    return true;
}

Poly poly_from_upoly(const Rat *c, int deg, const char *var)
{
    Poly p = poly_var(var);
    p.n = deg + 1;
    p.t = poly_terms(p.n);
    for (int i = 0; i <= deg; i++) {
        p.t[i].c = c[i];
        p.t[i].e[0] = (unsigned short)i;
    }
    poly_norm(&p);                      /* distinct powers: nothing to add, cannot overflow */
    return p;
}

bool upoly_eval(const Rat *c, int deg, Rat x, Rat *out)
{
    Rat acc = c[deg];
    for (int i = deg - 1; i >= 0; i--)
        if (!rat_mul(acc, x, &acc) || !rat_add(acc, c[i], &acc)) return false;
    *out = acc;
    return true;
}

bool upoly_divmod(const Rat *a, int da, const Rat *b, int db, Rat *q, int *dq, Rat *r, int *dr)
{
    if (db < 0 || rat_is_zero(b[db])) return false;
    int n = (da > db ? da : db) + 1;
    Rat *t = amalloc(sizeof(Rat) * (size_t)n);
    for (int i = 0; i < n; i++) t[i] = i <= da ? a[i] : rat_int(0);
    q[0] = rat_int(0);
    *dq = 0;
    for (int k = da - db; k >= 0; k--) {
        Rat qk, s;
        if (!rat_div(t[db + k], b[db], &qk)) return false;
        q[k] = qk;
        for (int j = 0; j <= db; j++)
            if (!rat_mul(qk, b[j], &s) || !rat_sub(t[j + k], s, &t[j + k])) return false;
    }
    if (da >= db) *dq = da - db;
    r[0] = rat_int(0);
    *dr = 0;
    for (int i = 0; i < db; i++) {
        r[i] = t[i];
        if (!rat_is_zero(t[i])) *dr = i;
    }
    return true;
}

bool upoly_div_linear(const Rat *a, int da, Rat root, Rat *q, Rat *rem)
{
    if (da < 1) return false;
    q[da - 1] = a[da];
    Rat s;
    for (int i = da - 1; i >= 1; i--)
        if (!rat_mul(root, q[i], &s) || !rat_add(a[i], s, &q[i - 1])) return false;
    if (!rat_mul(root, q[0], &s) || !rat_add(a[0], s, rem)) return false;
    return true;
}
