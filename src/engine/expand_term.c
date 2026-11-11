/* Expanding one product term with working:
 *   k(a+b)            -> multiply each term in the bracket
 *   (a+b)(c+d)        -> every term times every term (the four products), then collect
 *   3+ brackets       -> expand the first two, then multiply by the next, each stage shown
 *   (a+b)^n, n <= 4   -> written as repeated brackets; n > 4 -> binomial expansion */
#include "alg_int.h"

#define MAXPROD 256

static Expr *br_expr(const EBr *b)
{
    Expr *e = b->disp ? ex_clone(b->disp) : alg_pexpr(&b->p);
    return b->pw > 1 ? ex_pow2(e, ex_int(b->pw)) : e;
}

/* k·mono · cur · br[from..] for display */
static Expr *term_display(const ETerm *t, const Poly *cur, int from)
{
    Expr *kids[EXP_MAXBR + 2];
    int n = 0;
    Poly km;
    if (cur) kids[n++] = alg_pexpr(cur);
    for (int j = from; j < t->nbr; j++) kids[n++] = br_expr(&t->br[j]);
    if (!poly_scale(&t->mono, t->k, &km)) return ex_int(0);
    if (n == 0) return alg_pexpr(&km);
    Expr *b = alg_prod(kids, n);
    Rat c;
    if (poly_is_const(&km, &c) && rat_is_one(c)) return b;
    if (poly_is_const(&km, &c) && rat_eq(c, rat_int(-1))) return ex_neg(b);
    return alg_prod((Expr *[]){alg_pexpr(&km), b}, 2);
}

/* The bracket's terms in the order they were typed. */
static bool br_terms(const EBr *b, Poly *t, int max, int *n)
{
    const Expr *d = b->disp;
    if (d && d->kind == E_ADD && d->nkids <= max) {
        Poly sum = poly_zero(), ns;
        bool ok = true;
        for (int i = 0; i < d->nkids && ok; i++)
            ok = poly_from_expr(d->kids[i], &t[i]) && t[i].n == 1 && poly_add(&sum, &t[i], &ns) && ((sum = ns), true);
        if (ok && sum.n == d->nkids && poly_equal(&sum, &b->p)) { *n = d->nkids; return true; }
    }
    return alg_display_terms(&b->p, t, max, n);
}

static bool power_steps(ETerm *t, Steps *st)
{
    EBr nb[EXP_MAXBR];
    int n = 0;
    bool rep = false;
    for (int j = 0; j < t->nbr; j++) {
        EBr b = t->br[j];
        if (b.pw >= 2 && b.pw <= 4) {
            if (n + b.pw > EXP_MAXBR) return false;
            for (int r = 0; r < b.pw; r++) nb[n++] = (EBr){b.p, b.disp, 1};
            rep = true;
        } else if (b.pw > 4) {
            Poly P;
            if (n >= EXP_MAXBR || !poly_pow(&b.p, b.pw, &P)) return false;
            steps_add(st, b.p.n == 2 ? "Use the binomial expansion" : "Multiply out the power",
                      b.p.n == 2 ? "The coefficients come from Pascal's triangle (nCr)." : "Multiply the bracket by itself.",
                      "%s=%s", ex_print(br_expr(&b)), alg_pstr(&P));
            nb[n++] = (EBr){P, NULL, 1};
        } else {
            if (n >= EXP_MAXBR) return false;
            nb[n++] = b;
        }
    }
    for (int j = 0; j < n; j++) t->br[j] = nb[j];
    t->nbr = n;
    if (rep)
        steps_add(st, "Write the power as repeated brackets", "A power of a bracket means the bracket multiplied by itself.",
                  "%s", ex_print(term_display(t, NULL, 0)));
    return true;
}

bool exp_term(ETerm *t, Steps *st, Poly *out)
{
    Poly km, cur, res;
    if (!poly_scale(&t->mono, t->k, &km)) return false;
    if (t->nbr == 0) { *out = km; return true; }
    if (!power_steps(t, st)) return false;
    cur = t->br[0].p;
    for (int j = 1; j < t->nbr; j++) {
        Poly *at = amalloc(sizeof(Poly) * MAXPROD), *bt = amalloc(sizeof(Poly) * MAXPROD);
        Poly *pr = amalloc(sizeof(Poly) * MAXPROD), prod = poly_zero(), tmp;
        int na, nb, np = 0;
        if (!(j == 1 ? br_terms(&t->br[0], at, MAXPROD, &na) : alg_display_terms(&cur, at, MAXPROD, &na)) ||
            !br_terms(&t->br[j], bt, MAXPROD, &nb))
            return false;
        bool show = na * nb <= MAXPROD;
        for (int a = 0; a < na; a++)
            for (int b = 0; b < nb; b++) {
                Poly p;
                if (!poly_mul(&at[a], &bt[b], &p) || !poly_add(&prod, &p, &tmp)) return false;
                prod = tmp;
                if (show) pr[np++] = p;
            }
        const char *title = t->nbr == 2 ? "Multiply out the brackets"
                          : j == 1 ? "Expand the first two brackets"
                          : asprintf_a("Multiply by (%s)", alg_pstr(&t->br[j].p));
        const char *note = j == 1 ? "Multiply each term in the first bracket by each term in the second."
                                  : "Multiply each term of the answer so far by each term in the next bracket.";
        if (show) steps_add(st, title, note, "%s", ex_print(alg_sum_of(pr, np)));
        else steps_add(st, title, note, "%s", alg_pstr(&prod));
        cur = prod;
        if (show && prod.n < np)
            steps_add(st, "Collect like terms", "Add the terms with the same letters and powers.", "%s",
                      ex_print(term_display(t, &cur, j + 1)));
        else if (j + 1 < t->nbr)
            steps_add(st, "So far", "", "%s", ex_print(term_display(t, &cur, j + 1)));
    }
    Rat c;
    if (poly_is_const(&km, &c) && rat_is_one(c)) { *out = cur; return true; }
    if (!poly_mul(&km, &cur, &res)) return false;
    const char *ks = alg_pstr(&km);
    steps_add(st, t->nbr == 1 ? asprintf_a("Multiply each term in the bracket by %s", ks)
                              : asprintf_a("Multiply each term by %s", ks),
              "Every term inside the bracket gets multiplied.", "%s", alg_pstr(&res));
    *out = res;
    return true;
}
