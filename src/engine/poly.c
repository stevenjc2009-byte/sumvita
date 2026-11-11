/* Polynomials: construction, normal form, conversion from and to Expr. */
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "poly_int.h"

PolyTerm *poly_terms(int n)
{
    size_t sz = sizeof(PolyTerm) * (size_t)(n > 0 ? n : 1);
    PolyTerm *t = amalloc(sz);
    memset(t, 0, sz);
    return t;
}

Poly poly_zero(void)
{
    Poly p;
    memset(&p, 0, sizeof p);
    p.t = poly_terms(1);
    return p;
}

Poly poly_const(Rat c)
{
    Poly p = poly_zero();
    if (!rat_is_zero(c)) {
        p.n = 1;
        p.t[0].c = c;
    }
    return p;
}

Poly poly_var(const char *name)
{
    Poly p = poly_const(rat_int(1));
    p.nv = 1;
    strncpy(p.v[0], name, sizeof p.v[0] - 1);
    p.v[0][sizeof p.v[0] - 1] = 0;
    p.t[0].e[0] = 1;
    return p;
}

int poly_term_cmp(const PolyTerm *a, const PolyTerm *b)
{
    for (int i = 0; i < POLY_MAXV; i++)
        if (a->e[i] != b->e[i]) return a->e[i] > b->e[i] ? 1 : -1;
    return 0;
}

static int cmp_desc(const void *a, const void *b) { return -poly_term_cmp(a, b); }

bool poly_norm(Poly *p)
{
    PolyTerm *t = p->t;
    if (p->n > 1) qsort(t, (size_t)p->n, sizeof *t, cmp_desc);
    int w = 0;
    for (int i = 0; i < p->n; i++) {
        if (w > 0 && poly_term_cmp(&t[w - 1], &t[i]) == 0) {
            if (!rat_add(t[w - 1].c, t[i].c, &t[w - 1].c)) return false;
        } else {
            t[w++] = t[i];
        }
    }
    int z = 0;
    for (int i = 0; i < w; i++)
        if (!rat_is_zero(t[i].c)) t[z++] = t[i];
    p->n = z;
    if (z > POLY_MAXTERMS) return false;
    int nv = 0;
    for (int i = 0; i < p->nv; i++) {
        bool used = false;
        for (int j = 0; j < z && !used; j++) used = t[j].e[i] != 0;
        if (!used) continue;
        if (nv != i) {
            memcpy(p->v[nv], p->v[i], sizeof p->v[nv]);
            for (int j = 0; j < z; j++) t[j].e[nv] = t[j].e[i];
        }
        nv++;
    }
    for (int j = 0; j < z; j++)
        for (int i = nv; i < POLY_MAXV; i++) t[j].e[i] = 0;
    p->nv = nv;
    return true;
}

int poly_var_index(const Poly *p, const char *name)
{
    for (int i = 0; name && i < p->nv; i++)
        if (strcmp(p->v[i], name) == 0) return i;
    return -1;
}

static void remap(const Poly *p, char names[][12], int n, Poly *out)
{
    int map[POLY_MAXV] = {0};
    for (int i = 0; i < p->nv; i++)
        for (int k = 0; k < n; k++)
            if (strcmp(p->v[i], names[k]) == 0) map[i] = k;
    out->nv = n;
    for (int k = 0; k < n; k++) memcpy(out->v[k], names[k], sizeof out->v[k]);
    out->n = p->n;
    out->t = poly_terms(p->n);
    for (int j = 0; j < p->n; j++) {
        out->t[j].c = p->t[j].c;
        for (int i = 0; i < p->nv; i++) out->t[j].e[map[i]] = p->t[j].e[i];
    }
}

bool poly_unify(const Poly *a, const Poly *b, Poly *a2, Poly *b2)
{
    char names[2 * POLY_MAXV][12];
    int i = 0, j = 0, n = 0;
    while (i < a->nv || j < b->nv) {
        int c = i >= a->nv ? 1 : j >= b->nv ? -1 : strcmp(a->v[i], b->v[j]);
        const char *s = c <= 0 ? a->v[i] : b->v[j];
        if (c <= 0) i++;
        if (c >= 0) j++;
        memcpy(names[n++], s, 12);
    }
    if (n > POLY_MAXV) return false;
    remap(a, names, n, a2);
    remap(b, names, n, b2);
    return true;
}

/* ---- from Expr ---- */

static bool from(const Expr *e, Poly *out, int depth)
{
    Poly a, b;
    long long k;
    Rat c;
    if (!e || depth > 200) return false;
    switch (e->kind) {
    case E_NUM: case E_RECUR:
        *out = poly_const(e->r);
        return true;
    case E_MIXED: {
        if (e->nkids != 3) return false;
        const Expr *w = e->kids[0], *n = e->kids[1], *d = e->kids[2];
        Rat f, v;
        if (w->kind != E_NUM || n->kind != E_NUM || d->kind != E_NUM) return false;
        if (!rat_div(n->r, d->r, &f) || !rat_add(rat_abs(w->r), f, &v)) return false;
        *out = poly_const(rat_is_neg(w->r) ? rat_neg(v) : v);
        return true;
    }
    case E_VAR:
        *out = poly_var(e->name);
        return true;
    case E_ADD: case E_MUL:
        *out = e->kind == E_ADD ? poly_zero() : poly_const(rat_int(1));
        for (int i = 0; i < e->nkids; i++) {
            if (!from(e->kids[i], &a, depth + 1)) return false;
            if (!(e->kind == E_ADD ? poly_add(out, &a, &b) : poly_mul(out, &a, &b))) return false;
            *out = b;
        }
        return true;
    case E_NEG:
        return from(e->kids[0], &a, depth + 1) && poly_neg(&a, out);
    case E_PCT:
        return from(e->kids[0], &a, depth + 1) && poly_scale(&a, rat_make(1, 100), out);
    case E_DIV:
        if (!from(e->kids[0], &a, depth + 1) || !from(e->kids[1], &b, depth + 1)) return false;
        if (!poly_is_const(&b, &c) || rat_is_zero(c) || !rat_inv(c, &c)) return false;
        return poly_scale(&a, c, out);
    case E_POW:
        if (!ex_is_int(e->kids[1], &k) || k < 0 || k > POLY_MAXEXP) return false;
        return from(e->kids[0], &a, depth + 1) && poly_pow(&a, (int)k, out);
    default:
        return false;
    }
}

bool poly_from_expr(const Expr *e, Poly *out) { return from(e, out, 0); }

/* ---- to Expr ---- */

static Expr *term_expr(const Poly *p, const PolyTerm *t)
{
    Expr *k[POLY_MAXV + 1];
    int n = 0;
    if (!rat_is_one(t->c)) k[n++] = ex_num(t->c);
    for (int i = 0; i < p->nv; i++) {
        if (!t->e[i]) continue;
        Expr *v = ex_var(p->v[i]);
        k[n++] = t->e[i] == 1 ? v : ex_pow2(v, ex_int(t->e[i]));
    }
    if (n == 0) return ex_num(t->c);
    return n == 1 ? k[0] : ex_node(E_MUL, n, k);
}

Expr *poly_to_expr(const Poly *p, const char *mainvar)
{
    if (p->n == 0) return ex_int(0);
    int mv = poly_var_index(p, mainvar), n = p->n;
    const PolyTerm **ord = amalloc(sizeof(PolyTerm *) * (size_t)n);
    for (int i = 0; i < n; i++) ord[i] = &p->t[i];
    if (mv >= 0) {                     /* stable insertion sort by power of mainvar, highest first */
        for (int i = 1; i < n; i++) {
            const PolyTerm *x = ord[i];
            int j = i;
            while (j > 0 && ord[j - 1]->e[mv] < x->e[mv]) { ord[j] = ord[j - 1]; j--; }
            ord[j] = x;
        }
    }
    if (n == 1) return term_expr(p, ord[0]);
    Expr **k = amalloc(sizeof(Expr *) * (size_t)n);
    for (int i = 0; i < n; i++) k[i] = term_expr(p, ord[i]);
    return ex_node(E_ADD, n, k);
}
