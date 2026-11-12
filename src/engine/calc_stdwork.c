/* Standard-form arithmetic working, the GCSE way:
 *   (6×10^8)÷(3×10^3): divide the numbers 6÷3 = 2, subtract the powers 10^(8-3) = 10^5;
 *   (2×10^5)+(3×10^4): write both with the same power, 2×10^5 + 0.3×10^5 = 2.3×10^5.
 * Used by the Standard form action (× ÷) and by "=" (all four, then the ordinary number). */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_more.h"

#define TIMES "\xC3\x97"
#define DIVS "\xC3\xB7"

/* a number as a GCSE student writes it: 7.2 rather than 36/5 */
static const char *mtext(Rat r)
{
    const char *d = cu_exact_dec(r);
    return d ? d : ex_print(cu_rat_disp(r));
}

static const char *sci(Rat a, int n) { return asprintf_a("%s" TIMES "10^%d", mtext(a), n); }

/* a standard-form number, possibly with a minus in front */
static bool signed_parts(const Expr *e, Rat *a, int *n)
{
    bool neg = false;
    while (e->kind == E_NEG && e->nkids == 1) { neg = !neg; e = e->kids[0]; }
    if (!cu_std_parts(e, a, n)) return false;
    if (neg) *a = rat_neg(*a);
    return true;
}

bool cu_std_muldiv_steps(const Expr *t, Steps *st, Rat *m, int *p)
{
    Rat a, a2;
    int n, n2;
    if ((t->kind != E_MUL && t->kind != E_DIV) || t->nkids != 2 || !cu_std_parts(t->kids[0], &a, &n) ||
        !cu_std_parts(t->kids[1], &a2, &n2))
        return false;
    bool mul = t->kind == E_MUL;
    if (!(mul ? rat_mul(a, a2, m) : rat_div(a, a2, m))) return false;
    *p = mul ? n + n2 : n - n2;
    steps_add(st, mul ? "Multiply the numbers" : "Divide the numbers", "", "%s%s%s=%s", mtext(a), mul ? TIMES : DIVS,
              mtext(a2), mtext(*m));
    steps_add(st, mul ? "Add the powers of 10" : "Subtract the powers of 10",
              mul ? "10^a" TIMES "10^b = 10^(a+b)" : "10^a" DIVS "10^b = 10^(a-b)", "10^%d%s10^%d=10^%d", n,
              mul ? TIMES : DIVS, n2, *p);
    return true;
}

/* (a×10^n) ± (b×10^m): same power first, then add or subtract the numbers */
static bool addsub_steps(const Expr *t, Steps *st, Rat *m, int *p)
{
    Rat a[2];
    int n[2];
    if (t->kind != E_ADD || t->nkids != 2 || !signed_parts(t->kids[0], &a[0], &n[0]) ||
        !signed_parts(t->kids[1], &a[1], &n[1]))
        return false;
    int P = n[0] > n[1] ? n[0] : n[1];
    Rat b[2];
    for (int i = 0; i < 2; i++) {
        Rat scale;
        if (P - n[i] > 18 || !rat_pow_int(rat_int(10), P - n[i], &scale) || !rat_div(a[i], scale, &b[i])) return false;
    }
    bool sub = rat_is_neg(a[1]);
    const char *op = sub ? "-" : "+";
    const char *same = asprintf_a("%s%s%s", sci(b[0], P), op, sci(rat_abs(b[1]), P));
    if (n[0] != n[1])
        steps_add(st, "Write both with the same power of 10", "Change the smaller power to match the bigger one",
                  "%s%s%s=%s", sci(a[0], n[0]), op, sci(rat_abs(a[1]), n[1]), same);
    if (!rat_add(b[0], b[1], m)) return false;
    *p = P;
    steps_add(st, sub ? "Subtract the numbers" : "Add the numbers",
              asprintf_a("The powers of 10 are the same, so %s the first numbers", sub ? "subtract" : "add"), "%s=%s",
              same, sci(*m, P));
    return true;
}

static bool has_mantissa(const Expr *e)
{
    while (e->kind == E_NEG && e->nkids == 1) e = e->kids[0];
    return e->kind == E_MUL;
}

bool cu_std_working(const Expr *t, Steps *st)
{
    int before = st->n, p;
    Rat m;
    if (t->nkids != 2 || !(has_mantissa(t->kids[0]) || has_mantissa(t->kids[1]))) return false;
    if (!cu_std_muldiv_steps(t, st, &m, &p) && !addsub_steps(t, st, &m, &p)) { st->n = before; return false; }
    const char *md = cu_exact_dec(m), *plain = md ? cu_shift_point(md, p) : NULL;
    if (!plain) { st->n = before; return false; }
    if (rat_is_zero(m)) return true;
    steps_add(st, "Write as an ordinary number",
              asprintf_a("Move the decimal point %d place%s %s", p < 0 ? -p : p, p == 1 || p == -1 ? "" : "s",
                         p >= 0 ? "right" : "left"),
              "%s=%s", sci(m, p), plain);
    return true;
}
