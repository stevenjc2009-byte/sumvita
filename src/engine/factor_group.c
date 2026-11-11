/* Factorising by grouping: split the terms into equal groups, take each group's common
 * factor, and look for the same bracket in every group. Terms are grouped in the order
 * the user typed them first, so 2x^2y-8y+3x^2-12 pairs as (2x^2y-8y)+(3x^2-12). */
#include "alg_int.h"

#include <string.h>

#define GMAX 8

static bool group_terms(FCtx *c, const Poly *q, Poly *t, int *n)
{
    if (c->ntyped == q->n && q->n <= GMAX) {
        Poly sum = poly_zero(), ns;
        bool ok = true;
        for (int j = 0; j < c->ntyped && ok; j++) {
            ok = poly_div_exact(&c->typed[j], &c->tg, &t[j]) && poly_add(&sum, &t[j], &ns);
            if (ok) sum = ns;
        }
        if (ok && poly_equal(&sum, q)) { *n = q->n; return true; }
    }
    return alg_display_terms(q, t, GMAX, n);
}

typedef struct {
    FCtx *c;
    int i;
    FSplit *s;
    const Poly *t;
    int n, k, size;
    int asg[GMAX];
} GState;

static Expr *gexpr(const Poly *g)
{
    Rat v;
    if (poly_is_const(g, &v)) return ex_num(v);
    return alg_pexpr(g);
}

static bool try_partition(GState *g)
{
    Poly G[GMAX], gf[GMAX], r[GMAX], other = poly_zero(), tmp;
    Poly gt[GMAX][GMAX];
    int gn[GMAX] = {0};
    for (int j = 0; j < g->n; j++) gt[g->asg[j]][gn[g->asg[j]]++] = g->t[j];
    for (int j = 0; j < g->k; j++) {
        Rat K;
        Poly M, neg;
        G[j] = poly_zero();
        for (int m = 0; m < gn[j]; m++) {
            if (!poly_add(&G[j], &gt[j][m], &tmp)) return false;
            G[j] = tmp;
        }
        if (!alg_hcf(&G[j], &K, &M)) return false;
        if (rat_is_neg(gt[j][0].t[0].c)) K = rat_neg(K);   /* a group starting with - takes out a negative */
        if (!poly_scale(&M, K, &gf[j]) || !poly_div_exact(&G[j], &gf[j], &r[j])) return false;
        if (j == 0) {
            if (r[0].n < 2) return false;
            continue;
        }
        if (!poly_equal(&r[j], &r[0])) {
            if (!poly_neg(&r[j], &neg) || !poly_equal(&neg, &r[0])) return false;
            if (!poly_neg(&gf[j], &tmp)) return false;
            gf[j] = tmp;
            r[j] = neg;
        }
    }
    for (int j = 0; j < g->k; j++) {
        if (!poly_add(&other, &gf[j], &tmp)) return false;
        other = tmp;
    }
    if (other.n < 2) return false;
    FSplit *s = g->s;
    s->pc[0] = r[0];
    s->pc[1] = other;
    s->n = 2;
    Expr **ge = amalloc(sizeof(Expr *) * (size_t)g->k), **fe = amalloc(sizeof(Expr *) * (size_t)g->k);
    Expr *re = alg_pexpr(&r[0]);
    for (int j = 0; j < g->k; j++) {
        ge[j] = alg_sum_of(gt[j], gn[j]);
        ge[j]->flags |= EF_PAREN;
        fe[j] = alg_prod((Expr *[]){gexpr(&gf[j]), re}, 2);
    }
    steps_add(&s->pre, "Group the terms", "Put terms that share a common factor together.", "%s",
              ex_print(fac_render(g->c, g->i, ex_node(E_ADD, g->k, ge))));
    steps_add(&s->pre, "Factorise each group", "Take out the common factor of each group.", "%s",
              ex_print(fac_render(g->c, g->i, ex_node(E_ADD, g->k, fe))));
    s->title = "Take out the common bracket";
    s->note = asprintf_a("(%s) is in every group.", ex_print(re));
    return true;
}

/* Assign terms to groups: each new group starts with the first unassigned term. */
static bool enumerate(GState *g, int grp, int filled, int from)
{
    if (grp == g->k) return try_partition(g);
    if (filled == 0) {
        for (int j = 0; j < g->n; j++)
            if (g->asg[j] < 0) {
                g->asg[j] = grp;
                bool ok = g->size == 1 ? enumerate(g, grp + 1, 0, 0) : enumerate(g, grp, 1, j + 1);
                g->asg[j] = -1;
                return ok;
            }
        return false;
    }
    for (int j = from; j < g->n; j++) {
        if (g->asg[j] >= 0) continue;
        g->asg[j] = grp;
        bool ok = filled + 1 == g->size ? enumerate(g, grp + 1, 0, 0) : enumerate(g, grp, filled + 1, j + 1);
        g->asg[j] = -1;
        if (ok) return true;
    }
    return false;
}

bool fac_group(FCtx *c, int i, FSplit *s)
{
    const Poly *q = &c->br[i].p;
    Poly t[GMAX];
    int n;
    if (q->n < 4 || q->n > GMAX || !group_terms(c, q, t, &n)) return false;
    GState g;
    memset(&g, 0, sizeof g);
    g.c = c;
    g.i = i;
    g.s = s;
    g.t = t;
    g.n = n;
    for (int k = 2; k <= n / 2; k++) {
        if (n % k) continue;
        g.k = k;
        g.size = n / k;
        for (int j = 0; j < n; j++) g.asg[j] = -1;
        if (enumerate(&g, 0, 0, 0)) return true;
    }
    return false;
}
