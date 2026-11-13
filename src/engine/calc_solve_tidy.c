/* Clean-up of Solve output (called from calc.c after alg_solve):
 *  - symbolic answers: x=±√(a^2) -> x=±a, x=-2a/2 -> x=-a, x=(-b)/a -> x=-b/a
 *  - working lines for fractions: "Multiply both sides by 3" shows 2x+1=15, not (2x+1)=15,
 *    and an "Expand the brackets" step that changes nothing is dropped.
 * Each line is parsed, rewritten and printed again only when a rule changed it. */
#include <string.h>

#include "arena.h"
#include "calc_util.h"

static int64_t gcd64(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    while (b) { int64_t t = a % b; a = b; b = t; }
    return a;
}

/* numerator forms n, k*rest, -(n), -(k*rest): the integer k, or 0 if it is not one of them */
static int64_t lead_int(const Expr *e, bool *neg, const Expr **rest)
{
    *rest = NULL;
    *neg = false;
    if (e->kind == E_NEG) { *neg = true; e = e->kids[0]; }
    if (e->kind == E_NUM) return e->r.d == 1 ? e->r.n : 0;
    if (e->kind == E_MUL && e->kids[0]->kind == E_NUM && e->kids[0]->r.d == 1) {
        *rest = e;
        return e->kids[0]->r.n;
    }
    return 0;
}

static Expr *cancel_div(Expr *e)
{
    if (e->kind != E_DIV || e->kids[1]->kind != E_NUM || e->kids[1]->r.d != 1 || e->kids[1]->r.n < 2) return e;
    bool neg;
    const Expr *mul;
    int64_t k = lead_int(e->kids[0], &neg, &mul), d = e->kids[1]->r.n, g;
    if (!k || (g = gcd64(k, d)) < 2) return e;
    k /= g;
    d /= g;
    if (neg) k = -k;
    Expr *top;
    if (!mul) {
        top = ex_int(k);
    } else {
        Expr *rest = mul->nkids == 2 ? mul->kids[1] : ex_node(E_MUL, mul->nkids - 1, mul->kids + 1);
        if (k == 1) top = rest;
        else if (k == -1) top = ex_neg(rest);
        else {
            Expr **kk = amalloc(sizeof(Expr *) * (size_t)mul->nkids);
            kk[0] = ex_int(k);
            for (int i = 1; i < mul->nkids; i++) kk[i] = mul->kids[i];
            top = ex_node(E_MUL, mul->nkids, kk);
        }
    }
    return d == 1 ? top : ex_div2(top, ex_int(d));
}

static Expr *tidy(Expr *e, bool *ch)
{
    for (int i = 0; i < e->nkids; i++) e->kids[i] = tidy(e->kids[i], ch);
    if (e->kind == E_PM && e->nkids == 1) {
        Expr *s = e->kids[0], *b;
        long long k;
        if (s->kind == E_FUNC && s->op == F_SQRT && s->nkids == 1 && s->kids[0]->kind == E_POW &&
            ex_is_int(s->kids[0]->kids[1], &k) && k >= 2 && k % 2 == 0) {
            b = s->kids[0]->kids[0];
            if (b->kind != E_ADD) b->flags &= ~(unsigned)EF_PAREN;
            *ch = true;
            e->kids[0] = k == 2 ? b : ex_pow2(b, ex_int(k / 2));
        }
    }
    if (e->kind == E_DIV && e->kids[0]->kind == E_NEG && (e->kids[0]->flags & EF_PAREN)) {
        e->kids[0]->flags &= ~(unsigned)EF_PAREN;
        *ch = true;
    }
    Expr *c = cancel_div(e);
    if (c != e) *ch = true;
    return c;
}

/* line rewritten, or the same string when no rule applies (or it does not parse) */
static const char *tidy_line(const char *math)
{
    char err[80];
    Expr *e = ex_parse(math, err, sizeof err, NULL);
    bool ch = false;
    if (!e) return math;
    e = tidy(e, &ch);
    return ch ? ex_print(e) : math;
}

Expr *cs_tidy_answer(Expr *res)
{
    bool ch = false;
    return tidy(res, &ch);
}

/* a whole side written (2x+1) is a bracket nobody needs */
static const char *unbracket_sides(const char *math)
{
    char err[80];
    Expr *e = ex_parse(math, err, sizeof err, NULL);
    if (!e || e->kind != E_REL) return math;
    bool ch = false;
    for (int i = 0; i < e->nkids; i++)
        if (e->kids[i]->kind == E_ADD && (e->kids[i]->flags & EF_PAREN)) {
            e->kids[i]->flags &= ~(unsigned)EF_PAREN;
            ch = true;
        }
    return ch ? ex_print(e) : math;
}

void cs_tidy_steps(Steps *st, bool symbolic)
{
    if (!st) return;
    int m = 0;
    for (int i = 0; i < st->n; i++) {
        Step *s = &st->v[i];
        if (s->math && *s->math) {
            if (symbolic) s->math = tidy_line(s->math);
            if (strncmp(s->title, "Multiply both sides by", 22) == 0 &&
                (strstr(s->note, "lowest common multiple") || strstr(s->note, "clears the decimals")))
                s->math = unbracket_sides(s->math);
        }
        if (m > 0 && strcmp(s->title, "Expand the brackets") == 0 && s->math && strcmp(s->math, st->v[m - 1].math) == 0 &&
            s->depth == st->v[m - 1].depth)
            continue;
        st->v[m++] = *s;
    }
    st->n = m;
}
