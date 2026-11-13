/* Solve hooks around alg_solve (called from calc.c): friendly errors for inputs that
 * are not one equation, a product of letter factors, decimal answers for decimal input.
 * The tidying of symbolic answers and of working lines is in calc_solve_tidy.c. */
#include <stdio.h>
#include <string.h>

#include "algebra.h"
#include "arena.h"
#include "calc_util.h"
#include "poly.h"

/* ---- friendly errors: say what to do ---- */

static bool all_eq(const Expr *e)
{
    for (int i = 0; i < e->nkids - 1 && i < EXPR_MAX_REL; i++)
        if (e->ops[i] != R_EQ) return false;
    return true;
}

/* "2x+3y=12 x-y=1" reads as the chain 2x+3y=12x-y=1. Split the typed middle part at its
 * first space to show the two equations the student meant. NULL if it has no space. */
static const char *split_example(const char *raw)
{
    const char *e1 = raw ? strchr(raw, '=') : NULL, *e2 = e1 ? strchr(e1 + 1, '=') : NULL;
    if (!e2) return NULL;
    const char *a = e1 + 1, *b = e2;
    while (a < b && *a == ' ') a++;
    while (b > a && b[-1] == ' ') b--;
    const char *sp = memchr(a, ' ', (size_t)(b - a));
    if (!sp) return NULL;
    const char *r = sp, *end = raw + strlen(raw);
    while (r < b && *r == ' ') r++;
    while (end > e2 && end[-1] == ' ') end--;
    while (*raw == ' ') raw++;
    return asprintf_a("%.*s, %.*s", (int)(sp - raw), raw, (int)(end - r), r);
}

bool cs_precheck(const Expr *t, const char *raw, CalcResult *out)
{
    if (t->kind == E_LIST) {
        const Expr *eq = NULL, *other = NULL;
        for (int i = 0; i < t->nkids; i++) {
            const Expr *k = t->kids[i];
            if (k->kind != E_REL) continue;
            if (k->nkids == 2 && k->op == R_EQ) { if (!eq) eq = k; }
            else if (!other) other = k;
        }
        if (!other) return false;
        if (!eq) cu_fail(out, "Solve one inequality at a time, or join them as one line like 3<x<7");
        else cu_fail(out, "Solve one at a time: type %s on its own, then check its answers against %s",
                     ex_print(eq), ex_print(other));
        return true;
    }
    char names[8][12];
    if (t->kind == E_REL && t->nkids > 2 && all_eq(t) && ex_vars(t, names, 8) > 1) {
        const char *ex = t->nkids == 3 ? split_example(raw) : NULL;
        if (ex) cu_fail(out, "Type two equations separated by a comma: %s", ex);
        else cu_fail(out, "Type the equations separated by commas, like x+y=5, x-y=1");
        return true;
    }
    return false;
}

/* ---- (x+a)(x+b)=0: solve each linear factor ---- */

static Expr *no_paren(const Expr *e)
{
    Expr *c = ex_clone(e);
    c->flags &= ~(unsigned)EF_PAREN;
    return c;
}

bool cs_factor_product(const Expr *t, const CalcSettings *s, Steps *st, Expr **res)
{
    char names[8][12], var[12] = "";
    if (t->kind != E_REL || t->nkids != 2 || t->op != R_EQ || ex_vars(t, names, 8) < 2) return false;
    const Expr *L = t->kids[0], *R = t->kids[1];
    if (ex_is_zero(L)) { const Expr *x = L; L = R; R = x; }
    if (!ex_is_zero(R) || L->kind != E_MUL) return false;
    if (s && s->target_var[0]) snprintf(var, sizeof var, "%s", s->target_var);
    else if (ex_has_var(t, "x")) strcpy(var, "x");
    else return false;
    Expr *eqs[8], *ans[8];
    int n = 0;
    for (int i = 0; i < L->nkids; i++) {
        Poly p;
        const Expr *f = L->kids[i];
        if (!ex_has_var(f, var) || !poly_from_expr(f, &p) || poly_degree(&p, var) != 1 || n >= 8) return false;
        Expr *eq = ex_rel(R_EQ, no_paren(f), ex_int(0)), *sol = NULL;
        char err[80] = "";
        if (!alg_rearrange(eq, var, NULL, &sol, err, sizeof err) || !sol || sol->kind != E_REL) return false;
        bool dup = false;
        for (int j = 0; j < n; j++) dup |= ex_equal(ans[j]->kids[1], sol->kids[1]);
        if (dup) continue;
        eqs[n] = eq;
        ans[n++] = sol;
    }
    if (n == 1) { *res = ans[0]; return true; }
    const char *a = "", *b = "";
    for (int i = 0; i < n; i++) {
        a = i ? asprintf_a("%s \"or\" %s", a, ex_print(eqs[i])) : ex_print(eqs[i]);
        b = i ? asprintf_a("%s \"or\" %s", b, ex_print(ans[i])) : ex_print(ans[i]);
    }
    steps_add(st, "Set each factor equal to 0", "If two things multiply to make 0, one of them must be 0.", "%s", a);
    steps_add(st, "Solve each one", "", "%s", b);
    *res = ex_node(E_LIST, n, ans);
    return true;
}

/* ---- decimals in, decimals out ---- */

static Expr *dec_tree(const Expr *e, bool *ch)
{
    Expr *c = ex_clone(e);
    const Expr *num = e->kind == E_DIV && e->kids[0]->kind == E_NUM && e->kids[1]->kind == E_NUM ? e : NULL;
    Rat r;
    if (num && !rat_is_zero(num->kids[1]->r) && rat_div(num->kids[0]->r, num->kids[1]->r, &r) && cu_exact_dec(r)) {
        *ch = true;
        Expr *d = ex_num(r);
        d->flags |= NF_DECIMAL;
        return d;
    }
    if (e->kind == E_NUM && e->r.d != 1 && cu_exact_dec(e->r)) {
        *ch = true;
        c->flags |= NF_DECIMAL;
        return c;
    }
    for (int i = 0; i < c->nkids; i++) c->kids[i] = dec_tree(e->kids[i], ch);
    return c;
}

/* The answer with every exact fraction that terminates written as a decimal; NULL if none. */
Expr *cs_decimal_answer(const Expr *res)
{
    bool ch = false;
    Expr *d = dec_tree(res, &ch);
    return ch ? d : NULL;
}
