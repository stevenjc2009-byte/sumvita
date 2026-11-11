/* Expand: alg_expand entry point. Splits the input into product terms (sums are expanded
 * term by term, keeping each term's sign), expands each with working (expand_term.c),
 * then collects like terms. Answer order: descending powers of the main letter, then
 * alphabetical (poly_to_expr). */
#include "alg_int.h"

#include <stdio.h>
#include <string.h>

static bool mul_in(ETerm *t, const Poly *q, const Expr *src)
{
    Rat c, co, nk;
    Poly m, nm;
    if (poly_is_const(q, &c)) {
        if (!rat_mul(t->k, c, &nk)) return false;
        t->k = nk;
        return true;
    }
    if (q->n == 1) {
        if (!alg_term(q, 0, &co, &m, NULL) || !rat_mul(t->k, co, &nk) || !poly_mul(&t->mono, &m, &nm)) return false;
        t->k = nk;
        t->mono = nm;
        return true;
    }
    if (t->nbr >= EXP_MAXBR) return false;
    t->br[t->nbr++] = (EBr){*q, src, 1};
    return true;
}

static bool add_br(ETerm *t, const Expr *b, int pw)
{
    Poly q, qp;
    if (!poly_from_expr(b, &q)) return false;
    if (q.n <= 1) return poly_pow(&q, pw, &qp) && mul_in(t, &qp, NULL);
    if (t->nbr >= EXP_MAXBR) return false;
    t->br[t->nbr++] = (EBr){q, b, pw};
    return true;
}

static bool decompose(const Expr *e, ETerm *t)
{
    long long n;
    Poly d, q;
    Rat c, nk;
    switch (e->kind) {
    case E_NEG:
        t->k = rat_neg(t->k);
        return decompose(e->kids[0], t);
    case E_MUL:
        for (int i = 0; i < e->nkids; i++)
            if (!decompose(e->kids[i], t)) return false;
        return true;
    case E_DIV:
        if (!poly_from_expr(e->kids[1], &d) || !poly_is_const(&d, &c) || rat_is_zero(c) || !rat_div(t->k, c, &nk))
            return false;
        t->k = nk;
        return decompose(e->kids[0], t);
    case E_POW:
        if (e->kids[0]->kind == E_ADD && ex_is_int(e->kids[1], &n) && n >= 1 && n <= POLY_MAXEXP)
            return add_br(t, e->kids[0], (int)n);
        break;
    case E_ADD:
        return add_br(t, e, 1);
    default:
        break;
    }
    return poly_from_expr(e, &q) && mul_in(t, &q, e);
}

static void term_init(ETerm *t)
{
    memset(t, 0, sizeof *t);
    t->k = rat_int(1);
    t->mono = poly_const(rat_int(1));
}

static int nterms(const Poly *p) { return p->n; }

/* math is ans, or a chain ending "=ans" */
static bool ends_with(const char *math, const char *ans)
{
    size_t m = math ? strlen(math) : 0, a = strlen(ans);
    if (m == a) return strcmp(math, ans) == 0;
    return m > a && math[m - a - 1] == '=' && strcmp(math + m - a, ans) == 0;
}

bool alg_expand(const Expr *e, Steps *st, Expr **out, char *err, int errcap)
{
    Poly P;
    if (!e || e->kind == E_REL || e->kind == E_LIST || !poly_from_expr(e, &P)) {
        snprintf(err, (size_t)errcap, "Can only expand polynomials");
        return false;
    }
    const Expr *one[1] = {e};
    const Expr *const *kids = e->kind == E_ADD ? (const Expr *const *)e->kids : one;
    int nk = e->kind == E_ADD ? e->nkids : 1;
    ETerm *t = amalloc(sizeof(ETerm) * (size_t)nk);
    Poly *r = amalloc(sizeof(Poly) * (size_t)nk);
    bool anybr = false;
    for (int i = 0; i < nk; i++) {
        term_init(&t[i]);
        if (!decompose(kids[i], &t[i])) {           /* odd shape: treat the term as already expanded */
            term_init(&t[i]);
            if (!poly_from_expr(kids[i], &t[i].mono)) { snprintf(err, (size_t)errcap, "Can only expand polynomials"); return false; }
        }
        if (t[i].nbr) anybr = true;
    }
    *out = alg_pexpr(&P);
    const char *ans = ex_print(*out);
    if (!anybr) {
        if (strcmp(ex_print(e), ans) != 0)
            steps_add(st, "Collect like terms", "Add the terms with the same letters and powers.", "%s", ans);
        else
            steps_add(st, "Nothing to expand", "There are no brackets to multiply out.", "%s", ans);
        return true;
    }
    Steps *sub = amalloc(sizeof(Steps) * (size_t)nk);
    Poly sum = poly_zero(), tmp;
    int shown = 0;
    for (int i = 0; i < nk; i++) {
        steps_init(&sub[i]);
        if (!exp_term(&t[i], nk == 1 ? st : &sub[i], &r[i]) || !poly_add(&sum, &r[i], &tmp)) {
            snprintf(err, (size_t)errcap, "Too big to expand");
            return false;
        }
        sum = tmp;
        shown += nterms(&r[i]);
    }
    if (!poly_equal(&sum, &P)) {
        snprintf(err, (size_t)errcap, "Expand check failed");
        return false;
    }
    if (nk > 1) {
        Expr **parts = amalloc(sizeof(Expr *) * (size_t)(shown + 1));
        int np = 0;
        for (int i = 0; i < nk; i++) {
            Poly *dt = amalloc(sizeof(Poly) * (size_t)(r[i].n + 1));
            int nd;
            if (!alg_display_terms(&r[i], dt, r[i].n + 1, &nd)) nd = 0;
            for (int j = 0; j < nd; j++) parts[np++] = alg_pexpr(&dt[j]);
        }
        steps_add(st, "Multiply out each bracket", "Expand each part separately, keeping its sign.", "%s",
                  np ? ex_print(np == 1 ? parts[0] : ex_node(E_ADD, np, parts)) : "0");
        steps_push(st);
        for (int i = 0; i < nk; i++) {
            if (!t[i].nbr || sub[i].n < 2) continue;
            steps_add(st, asprintf_a("Expand %s", ex_print(kids[i])), "", "%s", alg_pstr(&r[i]));
            steps_push(st);
            alg_steps_copy(st, &sub[i]);
            steps_pop(st);
        }
        steps_pop(st);
        if (shown != P.n)
            steps_add(st, "Collect like terms", "Add the terms with the same letters and powers.", "%s", ans);
    }
    if (st && st->n && !ends_with(st->v[st->n - 1].math, ans))
        steps_add(st, "Write the answer in order", "Highest power first.", "%s", ans);
    return true;
}
