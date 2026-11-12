/* Surd brackets as they were written, and expanding them:
 *   (1+√3)^2 = (1+√3)(1+√3) = 1+√3+√3+3 = 4+2√3
 *   (3+√2)(1+√2) = 3+3√2+√2+2 = 5+4√2
 * Every term is multiplied by every term (first bracket, then second) in the
 * order written, then like terms are collected. */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_more.h"

const char *cu_term_text(SurdTerm t)
{
    SurdSum s;
    s.n = 1;
    s.t[0] = t;
    return ex_print(ss_to_expr(&s));
}

const char *cu_terms_text(const SurdTerm *w, int n)
{
    const char *s = "";
    for (int i = 0; i < n; i++) {
        SurdTerm t = w[i];
        bool neg = rat_is_neg(t.c);
        if (i) t.c = rat_abs(t.c);
        s = asprintf_a("%s%s%s", s, i && neg ? "-" : (i ? "+" : ""), cu_term_text(t));
    }
    return s;
}

static bool one_term(const Expr *e, SurdTerm *t)
{
    SurdSum s;
    if (!ss_from_expr(e, &s) || s.n != 1) return false;
    *t = s.t[0];
    return true;
}

int cu_surd_terms(const Expr *e, SurdTerm *w, int max)
{
    if (e->kind != E_ADD) return max >= 1 && one_term(e, &w[0]) ? 1 : 0;
    if (e->nkids > max) return 0;
    for (int i = 0; i < e->nkids; i++)
        if (!one_term(e->kids[i], &w[i])) return 0;
    return e->nkids;
}

static bool term_mul(SurdTerm a, SurdTerm b, SurdTerm *out)
{
    int64_t bb, k, m;
    Rat c;
    if (!i64_mul_ok(a.b, b.b, &bb) || !rat_mul(a.c, b.c, &c) || !cu_sqrt_split(bb, &k, &m) || !rat_mul(c, rat_int(k), &c))
        return false;
    out->c = c;
    out->b = m;
    return true;
}

/* a bracket of two terms that are not both whole numbers */
static bool surd_bracket(const SurdTerm *w, int n) { return n == 2 && (w[0].b != 1 || w[1].b != 1); }

static const char *bracket_text(const SurdTerm *w, int n)
{
    if (n == 1) {
        const char *s = cu_term_text(w[0]);
        return rat_is_neg(w[0].c) ? asprintf_a("(%s)", s) : s;
    }
    return asprintf_a("(%s)", cu_terms_text(w, n));
}

bool cu_surd_expand(const Expr *t, const SurdSum *whole, Steps *st)
{
    const SurdTerm zero = {{0, 1}, 1};
    SurdTerm x[2] = {zero, zero}, y[2] = {zero, zero}, prod[4] = {zero, zero, zero, zero};
    int nx, ny;
    bool square = false;
    if (t->kind == E_POW && t->nkids == 2) {
        long long k;
        if (!ex_is_int(t->kids[1], &k) || k != 2) return false;
        nx = ny = cu_surd_terms(t->kids[0], x, 2);
        memcpy(y, x, sizeof x);
        square = true;
    } else if (t->kind == E_MUL && t->nkids == 2) {
        nx = cu_surd_terms(t->kids[0], x, 2);
        ny = cu_surd_terms(t->kids[1], y, 2);
    } else {
        return false;
    }
    if (!nx || !ny || !(surd_bracket(x, nx) || surd_bracket(y, ny))) return false;
    int np = 0;
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
            if (!term_mul(x[i], y[j], &prod[np++])) return false;
    const char *br = asprintf_a("%s%s", bracket_text(x, nx), bracket_text(y, ny));
    if (square) steps_add(st, "Write the square as two brackets", "", "%s=%s", ex_print(t), br);
    const char *terms = cu_terms_text(prod, np), *ans = ex_print(ss_to_expr(whole));
    steps_add(st, "Multiply out the brackets",
              nx == 2 && ny == 2 ? "Multiply each term in the first bracket by each term in the second"
                                 : "Multiply each term in the bracket by the number outside",
              "%s=%s", br, terms);
    if (strcmp(terms, ans) != 0)
        steps_add(st, "Collect like terms", "Add the whole numbers, then add the surds", "%s=%s", terms, ans);
    return true;
}
