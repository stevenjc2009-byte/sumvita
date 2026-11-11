/* One linear and one non-linear equation (line + curve, line + circle):
 * make a letter the subject of the linear one, substitute into the other,
 * solve the resulting equation, then substitute each answer back. */
#include "solve_int.h"
#include <stdio.h>
#include <string.h>

static bool coef(const Poly *P, const char *v, Rat *k)
{
    Poly q;
    if (!poly_coeff(P, v, 1, &q)) return false;
    if (poly_is_zero(&q)) { *k = rat_int(0); return true; }
    return poly_is_const(&q, k);
}

/* value of the subject for one root: alpha*root + beta */
static SvRoot back(const SvRoot *r, Rat alpha, Rat beta)
{
    SvRoot o = *r;
    o.pm_next = false;
    o.v = rat_to_double(alpha) * sv_root_val(r) + rat_to_double(beta);
    if (r->exact) {
        Rat t;
        if (!rat_mul(alpha, r->A, &t) || !rat_add(t, beta, &o.A) || !rat_mul(alpha, r->B, &o.B)) o.exact = false;
    }
    return o;
}

bool sv_system_linquad(SvCtx *c, Expr **eqs, char vars[][12], Expr **out)
{
    Poly P[2], L, R;
    int lin = -1;
    for (int i = 0; i < 2; i++) {
        if (!poly_from_expr(eqs[i]->kids[0], &L) || !poly_from_expr(eqs[i]->kids[1], &R) || !poly_sub(&L, &R, &P[i]))
            return sv_fail(c, "I can only solve simultaneous equations made of polynomials");
        if (poly_total_degree(&P[i]) <= 1 && lin < 0) lin = i;
    }
    if (lin < 0) return sv_fail(c, "One of the equations must be linear");
    int oth = 1 - lin;
    Expr *num[2] = { eqs[0], eqs[1] };
    sv_step(c, "Number the equations", "Call them (1) and (2).", ex_print(ex_node(E_LIST, 2, num)));
    /* choose the subject: a letter with coefficient ±1 if possible, y before x */
    Rat k[2];
    if (!coef(&P[lin], vars[0], &k[0]) || !coef(&P[lin], vars[1], &k[1])) return sv_fail(c, "The numbers are too large");
    int s = -1;
    for (int i = 1; i >= 0 && s < 0; i--)
        if (rat_eq(rat_abs(k[i]), rat_int(1))) s = i;
    for (int i = 1; i >= 0 && s < 0; i--)
        if (!rat_is_zero(k[i])) s = i;
    if (s < 0) return sv_fail(c, "The linear equation has no letters");
    const char *sv = vars[s], *ov = vars[1 - s];
    Poly term = poly_var(sv), rest, sub;
    Rat inv;
    if (!poly_scale(&term, k[s], &term) || !poly_sub(&P[lin], &term, &rest) || !rat_div(rat_int(-1), k[s], &inv) ||
        !poly_scale(&rest, inv, &sub))
        return sv_fail(c, "The numbers are too large");
    Rat alpha, beta;
    if (!coef(&sub, ov, &alpha)) return sv_fail(c, "The numbers are too large");
    Poly cst;
    if (!poly_coeff(&sub, ov, 0, &cst) || !poly_is_const(&cst, &beta)) beta = rat_int(0);
    if (poly_is_zero(&cst)) beta = rat_int(0);
    Expr *se = sv_nice(sv_pexpr(&sub, ov));
    Expr *subj = ex_rel(R_EQ, ex_var(sv), se);
    const char *subj_s = ex_print(subj);
    if (strcmp(subj_s, ex_print(eqs[lin])) != 0)
        sv_step(c, sv_fmt("Rearrange (%d) to make %s the subject", lin + 1, sv), NULL, subj_s);
    Expr *wrap = ex_clone(se);
    if (wrap->kind == E_ADD || wrap->kind == E_NEG) wrap->flags |= EF_PAREN;
    Expr *subd = ex_subst(eqs[oth], sv, wrap);
    for (int i = 0; i < 2; i++) subd->kids[i]->flags &= ~(unsigned)EF_PAREN;   /* a whole side needs no bracket */
    sv_step(c, sv_fmt("Substitute into (%d)", oth + 1), sv_fmt("Replace %s with %s.", sv, ex_print(se)), ex_print(subd));
    char save[12];
    memcpy(save, c->var, sizeof save);
    snprintf(c->var, sizeof c->var, "%s", ov);
    c->last = ex_print(subd);
    Poly PL, PR;
    SvRoots Rt;
    memset(&Rt, 0, sizeof Rt);
    bool ok = poly_from_expr(subd->kids[0], &PL) && poly_from_expr(subd->kids[1], &PR) &&
              sv_prepare(c, R_EQ, subd->kids[0], subd->kids[1], &PL, &PR) && sv_poly_solve(c, &PL, &PR, &Rt);
    memcpy(c->var, save, sizeof save);
    if (!ok) {
        if (c->err && !c->err[0]) sv_fail(c, "The numbers are too large");
        return false;
    }
    if (Rt.all) {
        sv_step(c, "Every value works", "The line lies along the curve, so there are infinitely many solutions.", NULL);
        *out = ex_text("infinitely many solutions");
        return true;
    }
    if (Rt.n == 0) {
        sv_step(c, "The graphs don't meet", "The line does not cross the curve, so there are no real solutions.", NULL);
        *out = ex_text("no real solutions");
        return true;
    }
    Expr *pairs[SV_MAXROOTS];
    for (int i = 0; i < Rt.n; i++) {
        SvRoot r = Rt.r[i];
        r.pm_next = false;
        SvRoot b = back(&r, alpha, beta);
        Expr *re = sv_root_expr(&r), *be = sv_root_expr(&b);
        Expr *chain[3] = { ex_var(sv), NULL, be };
        int ops[2] = { R_EQ, R_EQ };
        bool simple = r.exact && rat_is_zero(r.B);
        Expr *line;
        if (simple) {
            Expr *rv = ex_clone(re);
            if (rat_is_neg(r.A)) rv->flags |= EF_PAREN;
            chain[1] = ex_subst(se, ov, rv);
            line = strcmp(ex_print(chain[1]), ex_print(be)) == 0 ? ex_rel(R_EQ, chain[0], be) : sv_chain(3, chain, ops);
        } else line = ex_rel(R_EQ, chain[0], be);
        sv_step(c, sv_fmt("Substitute %s into %s", ex_print(ex_rel(R_EQ, ex_var(ov), re)), subj_s), NULL, ex_print(line));
        Expr *kv[2];
        kv[s] = ex_rel(R_EQ, ex_var(sv), be);
        kv[1 - s] = ex_rel(R_EQ, ex_var(ov), re);
        pairs[i] = ex_node(E_LIST, 2, kv);
    }
    *out = Rt.n == 1 ? pairs[0] : ex_node(E_LIST, Rt.n, pairs);
    sv_step(c, "Write the solutions in pairs", NULL, ex_print(*out));
    return true;
}
