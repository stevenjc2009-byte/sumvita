/* Poly / display helpers shared by expand, factor and algebraic fractions. */
#include "alg_int.h"

#include <string.h>

Rat alg_rat1(void) { return rat_int(1); }

bool alg_poly_of(const Expr *e, Poly *out) { return e && poly_from_expr(e, out); }

const char *alg_mainvar(const Poly *p)
{
    int best = -1, bd = 0;
    for (int j = 0; j < p->nv; j++) {
        int d = poly_degree(p, p->v[j]);
        if (d > bd) { bd = d; best = j; }
    }
    return best < 0 ? NULL : p->v[best];
}

Expr *alg_pexpr(const Poly *p) { return ex_tidy(poly_to_expr(p, alg_mainvar(p))); }

Expr *alg_pexpr_asc(const Poly *p)
{
    Expr *e = poly_to_expr(p, alg_mainvar(p));
    if (e->kind == E_ADD) {
        Expr **k = amalloc(sizeof(Expr *) * (size_t)e->nkids);
        for (int i = 0; i < e->nkids; i++) k[i] = e->kids[e->nkids - 1 - i];
        e = ex_node(E_ADD, e->nkids, k);
    }
    return ex_tidy(e);
}

const char *alg_pstr(const Poly *p) { return ex_print(alg_pexpr(p)); }

bool alg_term(const Poly *p, int i, Rat *coef, Poly *mono, Poly *whole)
{
    Poly m = poly_const(rat_int(1));
    for (int j = 0; j < p->nv; j++) {
        if (!p->t[i].e[j]) continue;
        Poly v = poly_var(p->v[j]), pw, nm;
        if (!poly_pow(&v, p->t[i].e[j], &pw) || !poly_mul(&m, &pw, &nm)) return false;
        m = nm;
    }
    if (coef) *coef = p->t[i].c;
    if (mono) *mono = m;
    if (whole && !poly_scale(&m, p->t[i].c, whole)) return false;
    return true;
}

bool alg_display_terms(const Poly *p, Poly *terms, int max, int *n)
{
    *n = 0;
    if (poly_is_zero(p)) return true;
    Expr *e = poly_to_expr(p, alg_mainvar(p));
    if (e->kind != E_ADD) {
        if (max < 1) return false;
        terms[0] = *p;
        *n = 1;
        return true;
    }
    if (e->nkids > max) return false;
    for (int i = 0; i < e->nkids; i++)
        if (!poly_from_expr(e->kids[i], &terms[i])) return false;
    *n = e->nkids;
    return true;
}

int alg_lead_index(const Poly *p)
{
    Poly t[1];
    int n;
    Expr *e = poly_to_expr(p, alg_mainvar(p));
    if (e->kind == E_ADD) e = e->kids[0];
    if (!poly_from_expr(e, &t[0]) || t[0].n != 1) return 0;
    (void)n;
    for (int i = 0; i < p->n; i++) {
        Rat c;
        Poly m, w;
        if (alg_term(p, i, &c, &m, &w) && poly_equal(&w, &t[0])) return i;
    }
    return 0;
}

Rat alg_lead_coef(const Poly *p)
{
    if (p->n == 0) return rat_int(0);
    return p->t[alg_lead_index(p)].c;
}

bool alg_hcf(const Poly *p, Rat *content, Poly *mono)
{
    if (!poly_content(p, content)) return false;
    *mono = poly_monomial_gcd(p);
    return true;
}

bool alg_mono_root(const Poly *mono, int k, Poly *out)
{
    if (mono->n != 1) return false;
    Poly m = poly_const(rat_int(1));
    for (int j = 0; j < mono->nv; j++) {
        int e = mono->t[0].e[j];
        if (e % k) return false;
        if (!e) continue;
        Poly v = poly_var(mono->v[j]), pw, nm;
        if (!poly_pow(&v, e / k, &pw) || !poly_mul(&m, &pw, &nm)) return false;
        m = nm;
    }
    *out = m;
    return true;
}

int alg_mono_degree(const Poly *mono) { return poly_total_degree(mono); }

Expr *alg_km_expr(Rat k, const Poly *mono)
{
    Poly s;
    if (!poly_scale(mono, k, &s)) return ex_num(k);
    return alg_pexpr(&s);
}

Expr *alg_prod(Expr **kids, int n)
{
    int tot = 0;
    for (int i = 0; i < n; i++) tot += (kids[i]->kind == E_MUL && !(kids[i]->flags & EF_PAREN)) ? kids[i]->nkids : 1;
    if (tot == 0) return ex_int(1);
    Expr **k = amalloc(sizeof(Expr *) * (size_t)tot);
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (kids[i]->kind == E_MUL && !(kids[i]->flags & EF_PAREN))
            for (int j = 0; j < kids[i]->nkids; j++) k[m++] = kids[i]->kids[j];
        else
            k[m++] = kids[i];
    }
    return m == 1 ? k[0] : ex_node(E_MUL, m, k);
}

static Expr *render_exprs(Rat k, const Poly *mono, Expr **brs, int n)
{
    Expr **kids = amalloc(sizeof(Expr *) * (size_t)(n + 2));
    int m = 0;
    bool neg = rat_is_neg(k);
    Rat ak = rat_abs(k);
    bool hasmono = mono && !poly_is_const(mono, NULL);
    if (!rat_is_one(ak) || (!hasmono && n == 0)) kids[m++] = ex_num(ak);
    if (hasmono) kids[m++] = alg_km_expr(rat_int(1), mono);
    for (int i = 0; i < n; i++) kids[m++] = brs[i];
    Expr *e = alg_prod(kids, m);
    if (!neg) return e;
    /* -3xy(...) keeps the sign on the number; -(x-2)(x-3) uses a leading minus */
    if (e->kind == E_MUL && e->kids[0]->kind == E_NUM) {
        e->kids[0] = ex_num(k);
        return e;
    }
    if (e->kind == E_NUM) return ex_num(k);
    return ex_neg(e);
}

Expr *alg_render(Rat k, const Poly *mono, const Poly *br, const int *mult, const unsigned char *asc, int n)
{
    Expr **brs = amalloc(sizeof(Expr *) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) {
        Expr *b = (asc && asc[i]) ? alg_pexpr_asc(&br[i]) : alg_pexpr(&br[i]);
        if (mult && mult[i] > 1) b = ex_pow2(b, ex_int(mult[i]));
        brs[i] = b;
    }
    return render_exprs(k, mono, brs, n);
}

Expr *fac_render(const FCtx *c, int over, Expr *with)
{
    Expr **brs = amalloc(sizeof(Expr *) * (size_t)(c->nbr + 1));
    int mult[FAC_MAXBR];
    int src[FAC_MAXBR];
    int n = 0;
    for (int i = 0; i < c->nbr; i++) {
        if (i != over) {
            int j;
            for (j = 0; j < n; j++)
                if (src[j] >= 0 && poly_equal(&c->br[src[j]].p, &c->br[i].p)) break;
            if (j < n) { mult[j]++; continue; }
        }
        src[n] = i == over ? -1 : i;
        mult[n] = 1;
        brs[n] = i == over ? with : (c->br[i].asc ? alg_pexpr_asc(&c->br[i].p) : alg_pexpr(&c->br[i].p));
        n++;
    }
    for (int j = 0; j < n; j++)
        if (mult[j] > 1) brs[j] = ex_pow2(brs[j], ex_int(mult[j]));
    return render_exprs(c->num, &c->mono, brs, n);
}

const char *alg_rstr(Rat r)
{
    char buf[48];
    rat_to_str(r, buf, sizeof buf);
    return astrdup(buf);
}

const char *alg_rpar(Rat r)
{
    const char *s = alg_rstr(r);
    return rat_is_neg(r) ? asprintf_a("(%s)", s) : s;
}

void alg_steps_copy(Steps *dst, const Steps *src)
{
    if (!dst || !src) return;
    int save = dst->depth;
    for (int i = 0; i < src->n; i++) {
        dst->depth = save + src->v[i].depth;
        steps_add(dst, src->v[i].title, src->v[i].note, "%s", src->v[i].math);
    }
    dst->depth = save;
}

Expr *alg_sum_of(const Poly *terms, int n)
{
    if (n == 0) return ex_int(0);
    if (n == 1) return alg_pexpr(&terms[0]);
    Expr **k = amalloc(sizeof(Expr *) * (size_t)n);
    for (int i = 0; i < n; i++) k[i] = alg_pexpr(&terms[i]);
    return ex_node(E_ADD, n, k);
}
