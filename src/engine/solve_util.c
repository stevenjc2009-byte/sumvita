/* Small helpers shared by the solve files: steps, display, surds, root sets. */
#include "solve_int.h"
#include "arena.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sv_step(SvCtx *c, const char *title, const char *note, const char *math)
{
    if (!c) return;
    if (math && *math) c->last = math;
    if (!c->st) return;
    steps_add(c->st, title, note ? note : "", "%s", math ? math : "");
}

void sv_step_rel(SvCtx *c, const char *title, const char *note, int op, const Expr *l, const Expr *r)
{
    if (!c) return;
    sv_step(c, title, note, ex_print(ex_rel(op, (Expr *)l, (Expr *)r)));
}

void sv_step_new(SvCtx *c, const char *title, const char *note, const char *math)
{
    if (c && c->last && math && strcmp(c->last, math) == 0) return;
    sv_step(c, title, note, math);
}

bool sv_fail(SvCtx *c, const char *msg)
{
    if (c && c->err && c->errcap > 0) snprintf(c->err, (size_t)c->errcap, "%s", msg);
    return false;
}

const char *sv_fmt(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return astrdup(buf);
}

Expr *sv_pexpr(const Poly *p, const char *var)
{
    return ex_tidy(poly_to_expr(p, var));
}

static bool neg_term(const Expr *e)
{
    if (e->kind == E_NEG) return true;
    if (e->kind == E_NUM) return rat_is_neg(e->r);
    if (e->kind == E_REAL) return e->v < 0;
    if ((e->kind == E_MUL || e->kind == E_DIV) && e->nkids > 0) return neg_term(e->kids[0]);
    return false;
}

Expr *sv_nice(Expr *e)
{
    if (!e || e->kind != E_ADD || !neg_term(e->kids[0])) return e;
    for (int i = 1; i < e->nkids; i++) {
        if (neg_term(e->kids[i])) continue;
        Expr **k = amalloc(sizeof(Expr *) * (size_t)e->nkids);
        k[0] = e->kids[i];
        for (int j = 0, o = 1; j < e->nkids; j++)
            if (j != i) k[o++] = e->kids[j];
        Expr *n = ex_node(E_ADD, e->nkids, k);
        n->flags = e->flags;
        return n;
    }
    return e;
}

Poly sv_lin_poly(Rat a, Rat b, const char *var)
{
    Rat c[2] = { b, a };
    return poly_from_upoly(c, 1, var);
}

Expr *sv_lin_expr(Rat a, Rat b, const char *var)
{
    Poly p = sv_lin_poly(a, b, var);
    return sv_pexpr(&p, var);
}

bool sv_upoly(const Poly *p, const char *var, Rat *c, int *deg)
{
    return poly_to_upoly(p, var, c, SV_MAXDEG, deg);
}

int sv_flip(int op)
{
    switch (op) {
    case R_LT: return R_GT;
    case R_GT: return R_LT;
    case R_LE: return R_GE;
    case R_GE: return R_LE;
    default: return op;
    }
}

Expr *sv_chain(int n, Expr **kids, const int *ops)
{
    Expr *e = ex_node(E_REL, n, kids);
    e->op = ops[0];
    for (int i = 0; i < n - 1 && i < EXPR_MAX_REL; i++) e->ops[i] = ops[i];
    return e;
}

bool sv_sqrt_split(Rat d, Rat *s, int64_t *m)
{
    int64_t n, k = 1, r;
    if (rat_is_neg(d)) return false;
    if (rat_is_zero(d)) { *s = rat_int(0); *m = 1; return true; }
    if (!i64_mul_ok(d.n, d.d, &n)) return false;
    for (int64_t f = 2; f <= 200000 && f * f <= n; f++)
        while (n % (f * f) == 0) { n /= f * f; k *= f; }
    if (n > 1 && i64_isqrt_exact(n, &r)) { k *= r; n = 1; }
    *s = rat_make(k, d.d);
    *m = n;
    return true;
}

static Expr *pm1(Expr *a)
{
    return ex_node(E_PM, 1, &a);
}

Expr *sv_surd_expr(Rat A, Rat B, int64_t m, bool pm)
{
    if (rat_is_zero(B)) return ex_num(A);
    if (m == 1) {
        Rat s;
        if (pm) {
            Expr *b = ex_num(rat_abs(B));
            if (rat_is_zero(A)) return pm1(b);
            Expr *k[2] = { ex_num(A), b };
            return ex_node(E_PM, 2, k);
        }
        if (rat_add(A, B, &s)) return ex_num(s);
        return sv_real(rat_to_double(A) + rat_to_double(B));
    }
    int64_t d, a, b;
    if (!i64_lcm(A.d, B.d, &d) || !i64_mul_ok(A.n, d / A.d, &a) || !i64_mul_ok(B.n, d / B.d, &b))
        return sv_real(rat_to_double(A) + rat_to_double(B) * sqrt((double)m));
    Expr *sq = ex_func1(F_SQRT, ex_int(m));
    int64_t ab = b < 0 ? -b : b;
    Expr *bt = ab == 1 ? sq : ex_mul2(ex_int(ab), sq);
    Expr *num;
    if (pm) {
        if (a == 0) num = pm1(bt);
        else { Expr *k[2] = { ex_int(a), bt }; num = ex_node(E_PM, 2, k); }
    } else {
        Expr *t = b < 0 ? ex_neg(bt) : bt;
        num = a == 0 ? t : ex_add2(ex_int(a), t);
    }
    return d == 1 ? num : ex_div2(num, ex_int(d));
}

double sv_root_val(const SvRoot *r)
{
    if (!r->exact) return r->v;
    return rat_to_double(r->A) + rat_to_double(r->B) * sqrt((double)r->m);
}

Expr *sv_root_expr(const SvRoot *r)
{
    if (!r->exact) return sv_real(r->v);
    return sv_surd_expr(r->A, r->B, r->m, false);
}

void sv_add_rat(SvRoots *R, Rat v)
{
    for (int i = 0; i < R->n; i++)
        if (R->r[i].exact && rat_is_zero(R->r[i].B) && rat_eq(R->r[i].A, v)) return;
    if (R->n >= SV_MAXROOTS) return;
    SvRoot *r = &R->r[R->n++];
    memset(r, 0, sizeof *r);
    r->exact = true; r->A = v; r->B = rat_int(0); r->m = 1;
    r->v = rat_to_double(v);
}

void sv_add_real(SvRoots *R, double v)
{
    for (int i = 0; i < R->n; i++)
        if (fabs(sv_root_val(&R->r[i]) - v) < 1e-9 * (1 + fabs(v))) return;
    if (R->n >= SV_MAXROOTS) return;
    SvRoot *r = &R->r[R->n++];
    memset(r, 0, sizeof *r);
    r->exact = false; r->v = v;
    R->numeric = true;
}

Expr *sv_real(double v)
{
    char buf[40];
    snprintf(buf, sizeof buf, "%.12g", v);
    v = strtod(buf, NULL);
    if (fabs(v) < 1e-12) v = 0;
    return ex_real(v);
}

const char *sv_dec(double v, int dp)
{
    char buf[64];
    if (dp > 12) dp = 12;
    snprintf(buf, sizeof buf, "%.*f", dp, v);
    const char *p = buf;
    if (buf[0] == '-' && strspn(buf + 1, "0.") == strlen(buf + 1)) p = buf + 1;
    return astrdup(p);
}

int sv_dp(const SvCtx *c)
{
    return (c && c->s && c->s->dp >= 0) ? c->s->dp : 2;
}

Expr *sv_answer(SvCtx *c, const SvRoots *R)
{
    if (R->all) return ex_text(sv_fmt("true for all values of %s", c->var));
    if (R->n == 0) return ex_text("no real solutions");
    Expr *items[SV_MAXROOTS];
    double key[SV_MAXROOTS];
    int n = 0;
    for (int i = 0; i < R->n; i++) {
        const SvRoot *r = &R->r[i];
        Expr *val;
        bool rational = r->exact && (rat_is_zero(r->B) || r->m == 1);
        if (r->pm_next && i + 1 < R->n && !(rational && R->n > 2)) {
            val = sv_surd_expr(r->A, r->B, r->m, true);
            key[n] = rat_to_double(r->A);
            i++;
        } else {
            val = sv_root_expr(r);
            key[n] = sv_root_val(r);
        }
        /* insertion sort: answers in increasing order */
        int j = n++;
        Expr *it = ex_rel(R_EQ, ex_var(c->var), val);
        double k = key[j];
        while (j > 0 && key[j - 1] > k) { items[j] = items[j - 1]; key[j] = key[j - 1]; j--; }
        items[j] = it;
        key[j] = k;
    }
    return n == 1 ? items[0] : ex_node(E_LIST, n, items);
}

const char *sv_or_list(SvCtx *c, const SvRoots *R, bool decimals)
{
    const char *s = "";
    for (int i = 0; i < R->n; i++) {
        const SvRoot *r = &R->r[i];
        const char *one;
        if (decimals) one = sv_fmt("%s=%s", c->var, sv_dec(sv_root_val(r), sv_dp(c)));
        else if (r->pm_next && i + 1 < R->n) {
            one = ex_print(ex_rel(R_EQ, ex_var(c->var), sv_surd_expr(r->A, r->B, r->m, true)));
            i++;
        } else one = ex_print(ex_rel(R_EQ, ex_var(c->var), sv_root_expr(r)));
        s = i == 0 || !*s ? one : sv_fmt("%s \"or\" %s", s, one);
    }
    return s;
}

bool sv_lcm_dens(const Poly *p, int64_t *m)
{
    int64_t l = 1;
    for (int i = 0; i < p->n; i++)
        if (!i64_lcm(l, p->t[i].c.d, &l)) return false;
    *m = l;
    return true;
}

bool sv_has_decimal(const Expr *e)
{
    if (!e) return false;
    if (e->kind == E_NUM && (e->flags & NF_DECIMAL)) return true;
    for (int i = 0; i < e->nkids; i++)
        if (sv_has_decimal(e->kids[i])) return true;
    return false;
}

const char *sv_rat_str(Rat r)
{
    return ex_print(ex_num(r));
}
