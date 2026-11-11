/* Simplifier: sums (collect like terms, canonical term order). */
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "expr.h"
#include "simplify_int.h"

Num term_split(Expr *t, Expr **rest)
{
    if (is_numexpr(t)) { *rest = NULL; return num_of(t); }
    if (t->kind == E_MUL && t->nkids >= 2 && is_numexpr(t->kids[0])) {
        *rest = t->nkids == 2 ? t->kids[1] : ex_node(E_MUL, t->nkids - 1, t->kids + 1);
        return num_of(t->kids[0]);
    }
    *rest = t;
    return num_rat(rat_int(1));
}

static int count_flat(Expr **k, int n)
{
    int c = 0;
    for (int i = 0; i < n; i++) c += k[i]->kind == E_ADD ? count_flat(k[i]->kids, k[i]->nkids) : 1;
    return c;
}

static void push_flat(Expr **k, int n, Expr **out, int *m)
{
    for (int i = 0; i < n; i++) {
        if (k[i]->kind == E_ADD) push_flat(k[i]->kids, k[i]->nkids, out, m);
        else out[(*m)++] = k[i];
    }
}

/* ---- term order ---- */

typedef struct { const char *name; double e; } VarPow;

/* Variable powers of a coefficient-free term, in factor order (alphabetical). */
static int term_vars(const Expr *t, VarPow *vp, int max)
{
    int n = 0, nk = t->kind == E_MUL ? t->nkids : 1;
    for (int i = 0; i < nk && n < max; i++) {
        const Expr *f = t->kind == E_MUL ? t->kids[i] : t;
        const Expr *b = f->kind == E_POW ? f->kids[0] : f;
        if (b->kind != E_VAR) continue;
        double e = 1;
        if (f->kind == E_POW) {
            if (!is_numexpr(f->kids[1])) continue;
            e = num_val(num_of(f->kids[1]));
        }
        vp[n].name = b->name;
        vp[n].e = e;
        n++;
    }
    return n;
}

static int term_cmp(const void *pa, const void *pb)
{
    const Expr *a = *(Expr *const *)pa, *b = *(Expr *const *)pb;
    Expr *ra, *rb;
    term_split((Expr *)a, &ra);
    term_split((Expr *)b, &rb);
    VarPow va[16], vb[16];
    int na = term_vars(ra, va, 16), nb = term_vars(rb, vb, 16);
    double da = 0, db = 0;
    for (int i = 0; i < na; i++) da += va[i].e;
    for (int i = 0; i < nb; i++) db += vb[i].e;
    if (da != db) return da > db ? -1 : 1;            /* highest degree first */
    for (int i = 0; i < na && i < nb; i++) {
        int c = strcmp(va[i].name, vb[i].name);
        if (c) return c < 0 ? -1 : 1;                 /* x^2 before xy before y^2 */
        if (va[i].e != vb[i].e) return va[i].e > vb[i].e ? -1 : 1;
    }
    if (na != nb) return na > nb ? -1 : 1;
    int c = ex_compare(ra, rb);
    return c ? c : ex_compare(a, b);
}

/* ---- sums ---- */

static Expr *with_coef(Num c, Expr *rest)
{
    if (num_is(c, 1)) return rest;
    Expr *ce = num_expr(c);
    if (rest->kind != E_MUL) {
        Expr *k[2] = {ce, rest};
        return ex_node(E_MUL, 2, k);
    }
    Expr **k = amalloc(sizeof(Expr *) * (size_t)(rest->nkids + 1));
    k[0] = ce;
    memcpy(k + 1, rest->kids, sizeof(Expr *) * (size_t)rest->nkids);
    return ex_node(E_MUL, rest->nkids + 1, k);
}

Expr *simp_add_list(Expr **kids, int n)
{
    if (n == 0) return ex_int(0);
    int cap = count_flat(kids, n), m = 0;
    Expr **f = amalloc(sizeof(Expr *) * (size_t)(cap + 1));
    push_flat(kids, n, f, &m);

    Num cst = num_rat(rat_int(0));
    Expr **rest = amalloc(sizeof(Expr *) * (size_t)(m + 1));
    Num *coef = amalloc(sizeof(Num) * (size_t)(m + 1));
    int ng = 0;
    for (int i = 0; i < m; i++) {
        Expr *r;
        Num c = term_split(f[i], &r);
        if (!r) { cst = num_add(cst, c); continue; }
        int j = 0;
        while (j < ng && !ex_equal(rest[j], r)) j++;
        if (j == ng) { rest[ng] = r; coef[ng] = num_rat(rat_int(0)); ng++; }
        coef[j] = num_add(coef[j], c);
    }

    Expr **out = amalloc(sizeof(Expr *) * (size_t)(ng + 2));
    int no = 0;
    for (int j = 0; j < ng; j++)
        if (!num_is(coef[j], 0)) out[no++] = with_coef(coef[j], rest[j]);
    qsort(out, (size_t)no, sizeof(Expr *), term_cmp);
    if (!num_is(cst, 0) || no == 0) out[no++] = num_expr(cst);
    if (no == 1) return out[0];
    return ex_node(E_ADD, no, out);
}
