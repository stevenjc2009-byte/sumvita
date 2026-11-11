/* ex_tidy: canonical internals back to display form. Cosmetic only, never expands:
 *   MUL by -1 -> NEG, negative powers -> denominator (DIV),
 *   b^(1/2) -> √b, b^(1/q) -> root(q, b). */
#include <string.h>

#include "arena.h"
#include "expr.h"

static Expr *mk_list(Expr **k, int n)
{
    if (n == 0) return ex_int(1);
    if (n == 1) return k[0];
    return ex_node(E_MUL, n, k);
}

static bool neg_exp(const Expr *f)
{
    return f->kind == E_POW && f->kids[1]->kind == E_NUM && rat_is_neg(f->kids[1]->r);
}

/* b^p with p > 0 already known; roots become √ / root() */
static Expr *tidy_pow_pos(const Expr *b, Rat p)
{
    Expr *tb = ex_tidy(b);
    if (rat_is_one(p)) return tb;
    if (p.n == 1 && p.d == 2) return ex_func1(F_SQRT, tb);
    if (p.n == 1 && p.d > 2) return ex_func2(F_ROOT, ex_int(p.d), tb);
    return ex_pow2(tb, ex_num(p));
}

static bool pos_exp(const Expr *f)
{
    return f->kind == E_POW && f->kids[1]->kind == E_NUM && !rat_is_neg(f->kids[1]->r) &&
           !(f->kids[1]->flags & NF_DECIMAL);
}

static Expr *tidy_mul(const Expr *e)
{
    int n = e->nkids, i0 = 0, nn = 0, nd = 0;
    Expr **num = amalloc(sizeof(Expr *) * (size_t)(n + 1));
    Expr **den = amalloc(sizeof(Expr *) * (size_t)(n + 1));
    const Expr *coef = NULL;
    if (e->kids[0]->kind == E_NUM || e->kids[0]->kind == E_REAL) { coef = e->kids[0]; i0 = 1; }
    for (int i = i0; i < n; i++) {
        const Expr *f = e->kids[i];
        if (neg_exp(f)) den[nd++] = tidy_pow_pos(f->kids[0], rat_neg(f->kids[1]->r));
        else num[nn++] = ex_tidy(f);
    }
    bool neg = coef && (coef->kind == E_NUM ? rat_is_neg(coef->r) : coef->v < 0);
    if (nd == 0) {
        if (coef && coef->kind == E_NUM && coef->r.n == -1 && coef->r.d == 1) return ex_neg(mk_list(num, nn));
        if (!coef) return mk_list(num, nn);
        Expr **all = amalloc(sizeof(Expr *) * (size_t)(nn + 1));
        all[0] = ex_clone(coef);
        memcpy(all + 1, num, sizeof(Expr *) * (size_t)nn);
        return mk_list(all, nn + 1);
    }
    /* coefficient p/q: |p| on top, q underneath (a typed decimal stays whole on top) */
    Expr **top = amalloc(sizeof(Expr *) * (size_t)(nn + 1));
    Expr **bot = amalloc(sizeof(Expr *) * (size_t)(nd + 1));
    int nt = 0, nb = 0;
    if (coef && coef->kind == E_NUM && !(coef->flags & NF_DECIMAL)) {
        Rat a = rat_abs(coef->r);
        if (a.n != 1) top[nt++] = ex_int(a.n);
        if (a.d != 1) bot[nb++] = ex_int(a.d);
    } else if (coef) {
        Expr *c = ex_clone(coef);
        if (c->kind == E_NUM) c->r = rat_abs(c->r);
        else c->v = c->v < 0 ? -c->v : c->v;
        if (!ex_is_one(c)) top[nt++] = c;
    }
    memcpy(top + nt, num, sizeof(Expr *) * (size_t)nn);
    memcpy(bot + nb, den, sizeof(Expr *) * (size_t)nd);
    Expr *d = ex_div2(mk_list(top, nt + nn), mk_list(bot, nb + nd));
    return neg ? ex_neg(d) : d;
}

Expr *ex_tidy(const Expr *e)
{
    if (!e) return NULL;
    if (e->nkids == 0) return ex_clone(e);
    if (e->kind == E_MUL) return tidy_mul(e);
    if (e->kind == E_POW && neg_exp(e))
        return ex_div2(ex_int(1), tidy_pow_pos(e->kids[0], rat_neg(e->kids[1]->r)));
    if (pos_exp(e)) return tidy_pow_pos(e->kids[0], e->kids[1]->r);
    Expr *c = ex_clone(e);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = ex_tidy(e->kids[i]);
    return c;
}
