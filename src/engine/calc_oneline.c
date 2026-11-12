/* One line of working for a single operation the BIDMAS walk drops as "not worth
 * a step" when it is exactly where students go wrong:
 *   (-3)^2 = (-3)×(-3) = 9        -3^2 = -(3^2) = -9        -2×-3 = 6 (signs)
 *   0.5+1/2 = 0.5+0.5 = 1         80×1.15 = 92              250/1.25 = 200 */
#include <string.h>

#include "arena.h"
#include "calc_more.h"

#define TIMES "\xC3\x97"

static const Expr *bare(const Expr *e)
{
    Expr *c = amalloc(sizeof(Expr));
    *c = *e;
    c->flags &= ~(unsigned)EF_PAREN;
    return c;
}

/* a whole number written with a minus: -3 or (-3) */
static bool neg_int(const Expr *e, Rat *r)
{
    if (e->kind == E_NUM && rat_is_neg(e->r) && e->r.d == 1) { *r = e->r; return true; }
    if (e->kind == E_NEG && e->nkids == 1 && e->kids[0]->kind == E_NUM && e->kids[0]->r.d == 1) {
        *r = rat_neg(e->kids[0]->r);
        return true;
    }
    return false;
}

static bool is_negative(const Expr *e)
{
    return e->kind == E_NEG || (e->kind == E_NUM && rat_is_neg(e->r));
}

/* the value as a GCSE student writes it: a decimal if it stops, else a fraction */
static const char *val_text(Rat r)
{
    const char *d = cu_exact_dec(r);
    return d ? d : cu_rat_str(r);
}

static bool neg_power(const Expr *t, Rat v, Steps *st)
{
    Rat b;
    long long k;
    if (!neg_int(t->kids[0], &b) || !(t->kids[0]->flags & EF_PAREN) || !ex_is_int(t->kids[1], &k) || k < 2) return false;
    const char *bt = asprintf_a("(%s)", ex_print(bare(t->kids[0]))), *rep = bt;
    for (long long i = 1; i < k && k <= 4; i++) rep = asprintf_a("%s" TIMES "%s", rep, bt);
    const char *note = k % 2 ? "An odd power of a negative number is negative"
                             : "An even power of a negative number is positive";
    if (k <= 4) steps_add(st, "Indices", note, "%s=%s=%s", ex_print(t), rep, cu_rat_str(v));
    else steps_add(st, "Indices", note, "%s=%s", ex_print(t), cu_rat_str(v));
    return true;
}

/* -3^2: the power is done first, then the minus */
static bool minus_power(const Expr *t, Rat v, Steps *st)
{
    const Expr *p = t->kids[0];
    if (p->kind != E_POW || (p->flags & EF_PAREN) || p->kids[0]->kind != E_NUM || rat_is_neg(p->kids[0]->r)) return false;
    steps_add(st, "Indices first, then the minus", asprintf_a("The power only applies to the %s", ex_print(p->kids[0])),
              "%s=-(%s)=%s", ex_print(t), ex_print(p), cu_rat_str(v));
    return true;
}

static bool signs(const Expr *t, Rat v, Steps *st)
{
    bool n0 = is_negative(t->kids[0]), n1 = is_negative(t->kids[1]);
    if (!n0 && !n1) return false;
    bool mul = t->kind == E_MUL;
    const char *note = n0 && n1 ? (mul ? "Negative " TIMES " negative = positive" : "Negative \xC3\xB7 negative = positive")
                                : (mul ? "Negative " TIMES " positive = negative" : "Negative \xC3\xB7 positive = negative");
    const Expr *r = t->kids[1];
    const char *rt = n1 && !(r->flags & EF_PAREN) ? asprintf_a("(%s)", ex_print(r)) : ex_print(r);
    steps_add(st, mul ? "Multiply" : "Divide", note, "%s%s%s=%s", ex_print(t->kids[0]), mul ? TIMES : "/", rt,
              val_text(v));
    return true;
}

/* 0.5+1/2: the fraction becomes a decimal first */
static bool dec_and_frac(const Expr *t, Rat v, Steps *st)
{
    int di = -1, fi = -1;
    for (int i = 0; i < 2; i++) {
        const Expr *k = t->kids[i];
        if (k->kind == E_NUM && (k->flags & NF_DECIMAL)) di = i;
        else if (k->kind == E_DIV && k->kids[0]->kind == E_NUM && k->kids[1]->kind == E_NUM) fi = i;
    }
    Rat f;
    if (di < 0 || fi < 0 || !cu_rational(t->kids[fi], &f) || !cu_exact_dec(f) || !cu_exact_dec(v)) return false;
    const char *a = ex_print(t->kids[0]), *b = ex_print(t->kids[1]);
    const char *fd = cu_exact_dec(f);
    steps_add(st, "Change the fraction to a decimal", asprintf_a("%s = %s", ex_print(t->kids[fi]), fd), "%s=%s+%s=%s",
              ex_print(t), di == 0 ? a : fd, di == 0 ? fd : b, cu_exact_dec(v));
    return true;
}

bool cu_one_line(const Expr *t, bool degrees, Steps *st)
{
    Rat v;
    CVal cv;
    if (!cu_value(t, degrees, &cv) || !cv.exact || !cu_rational(cv.e, &v)) return false;
    switch (t->kind) {
    case E_POW: return t->nkids == 2 && neg_power(t, v, st);
    case E_NEG: return t->nkids == 1 && minus_power(t, v, st);
    case E_MUL: case E_DIV:
        if (t->nkids != 2) return false;
        if (signs(t, v, st)) return true;
        if (!cu_has_decimal(t)) return false;
        steps_add(st, t->kind == E_MUL ? "Multiply" : "Divide", "", "%s=%s", ex_print(t), val_text(v));
        return true;
    case E_ADD: return t->nkids == 2 && dec_and_frac(t, v, st);
    default: return false;
    }
}
