/* The Surds action: simplify each root (√72 = √(36×2) = 6√2), collect like
 * surds, and rationalise the denominator - by √b for one term, by the
 * conjugate (difference of two squares) for two terms. */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc_more.h"

#define TIMES "\xC3\x97"
#define MAX_ROOT_STEPS 8

static const char *P(const Expr *e) { return ex_print(e); }

static const char *sum_text(const SurdSum *s) { return P(ss_to_expr(s)); }

static const char *term_text(SurdTerm t)
{
    SurdSum s;
    s.n = 1;
    s.t[0] = t;
    return sum_text(&s);
}

/* text in brackets unless it is a plain number or a lone surd */
static const char *wrap(const SurdSum *s)
{
    if (s->n == 1 && !rat_is_neg(s->t[0].c) && s->t[0].c.d == 1 && (s->t[0].b == 1 || s->t[0].c.n == 1))
        return sum_text(s);
    return asprintf_a("(%s)", sum_text(s));
}

/* "2^2", "(√3)^2", "(-2)^2" */
static const char *square_text(SurdTerm t)
{
    const char *x = term_text(t);
    bool plain = t.b == 1 && t.c.d == 1 && !rat_is_neg(t.c);
    return asprintf_a(plain ? "%s^2" : "(%s)^2", x);
}

/* copy of e with every √(whole number) written in its simplest form,
 * writing a "Simplify the surd" step for each one that changes */
static Expr *simplify_roots(const Expr *e, Steps *st, int *nsteps, int depth)
{
    if (depth > 200) return (Expr *)e;
    long long n;
    if (e->kind == E_FUNC && e->op == F_SQRT && e->nkids == 1 && ex_is_int(e->kids[0], &n) && n > 0) {
        SurdSum s;
        if (!ss_from_expr(e, &s)) return (Expr *)e;
        if (*nsteps < MAX_ROOT_STEPS && cw_surd_step((int64_t)n, st)) (*nsteps)++;
        Expr *r = ss_to_expr(&s);
        r->flags |= e->flags & (unsigned)EF_PAREN;
        return r;
    }
    if (e->nkids == 0) return (Expr *)e;
    Expr *c = amalloc(sizeof(Expr));
    *c = *e;
    c->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = simplify_roots(e->kids[i], st, nsteps, depth + 1);
    return c;
}

static void finish(const SurdSum *r, CalcResult *out)
{
    Expr *e = ss_to_expr(r);
    Rat q;
    out->answer = P(e);
    out->value = ss_value(r);
    out->has_value = true;
    out->answer_alt = ss_rational(r, &q) && q.d == 1 ? "" : cu_fmt_dec(out->value, NULL, NULL);
}

/* one-term bottom c√b: multiply the top and bottom by √b */
static bool by_root(const SurdSum *A, const SurdSum *B, const char *lhs, Steps *st)
{
    SurdSum rb, top, bot;
    rb.n = 1;
    rb.t[0].c = rat_int(1);
    rb.t[0].b = B->t[0].b;
    if (!ss_mul(A, &rb, &top) || !ss_mul(B, &rb, &bot)) return false;
    const char *root = term_text(rb.t[0]);
    steps_add(st, asprintf_a("Multiply the top and bottom by %s", root), "This keeps the value the same",
              "%s=(%s" TIMES "%s)/(%s" TIMES "%s)", lhs, sum_text(A), root, sum_text(B), root);
    steps_add(st, "Work out the top and bottom", asprintf_a("%s" TIMES "%s = %lld", root, root, (long long)rb.t[0].b),
              "(%s" TIMES "%s)/(%s" TIMES "%s)=%s/%s", sum_text(A), root, sum_text(B), root, wrap(&top), wrap(&bot));
    return true;
}

/* two-term bottom as written, a+b: multiply by the conjugate a-b ("change the
 * sign in the middle"), so √3-1 uses √3+1 and the bottom comes out positive */
static bool by_conjugate(const SurdSum *A, const SurdSum *B, const Expr *bot, const char *lhs, Steps *st,
                         CalcResult *out)
{
    SurdTerm w[2], cw[2];
    if (cu_surd_terms(bot, w, 2) != 2 || w[0].b == w[1].b) { w[0] = B->t[0]; w[1] = B->t[1]; }
    cw[0] = w[0];
    cw[1] = w[1];
    cw[1].c = rat_neg(cw[1].c);
    SurdSum c0, c1, conj, top;
    c0.n = c1.n = 1;
    c0.t[0] = cw[0];
    c1.t[0] = cw[1];
    Rat s0, s1, D;
    if (!ss_add(&c0, &c1, &conj) || !rat_mul(w[0].c, w[0].c, &s0) || !rat_mul(s0, rat_int(w[0].b), &s0) ||
        !rat_mul(w[1].c, w[1].c, &s1) || !rat_mul(s1, rat_int(w[1].b), &s1) || !rat_sub(s0, s1, &D))
        return false;
    if (rat_is_zero(D)) { cu_fail(out, "Maths error: can't divide by 0"); return true; }
    if (!ss_mul(A, &conj, &top)) return false;
    const char *bt = cu_terms_text(w, 2), *ct = cu_terms_text(cw, 2), *at = wrap(A);
    Rat one = rat_int(1), aq;
    bool a_is_one = ss_rational(A, &aq) && rat_eq(aq, one);
    steps_add(st, "Multiply the top and bottom by the conjugate", asprintf_a("Change the sign in the middle: %s", ct),
              "%s=%s(%s)/((%s)(%s))", lhs, at, ct, bt, ct);
    SurdTerm a = w[0], b = w[1];
    a.c = rat_abs(a.c);
    b.c = rat_abs(b.c);
    steps_add(st, "Difference of two squares on the bottom", "(a+b)(a-b) = a^2-b^2", "(%s)(%s)=%s-%s=%s-%s=%s", bt,
              ct, square_text(a), square_text(b), cu_rat_str(s0), cu_rat_str(s1), cu_rat_str(D));
    if (!a_is_one) steps_add(st, "Expand the top", "", "%s(%s)=%s", at, ct, sum_text(&top));
    if (!rat_eq(D, one)) {
        SurdSum d, r;
        ss_rat(D, &d);
        if (ss_div(&top, &d, &r)) {
            const char *over = asprintf_a("%s/%s", wrap(&top), P(cu_rat_disp(D))), *res = sum_text(&r);
            if (strcmp(over, res) != 0)
                steps_add(st, asprintf_a("Divide by %s", cu_rat_str(D)), "", "%s=%s", over, res);
            else
                steps_add(st, "Put the top over the new bottom", "", "%s=%s", lhs, res);
        }
    }
    return true;
}

int cu_surd_working(const Expr *t, Steps *st, CalcResult *out)
{
    SurdSum whole, A, B;
    if (!ss_from_expr(t, &whole)) return 0;
    bool div = t->kind == E_DIV && t->nkids == 2 && ss_from_expr(t->kids[0], &A) && ss_from_expr(t->kids[1], &B) &&
               ss_has_surd(&B) && B.n <= 2;
    int before = st->n, nroot = 0;
    Expr *shown = simplify_roots(t, st, &nroot, 0);
    if (div) {
        const char *lhs = P(shown);
        bool ok = B.n == 1 ? by_root(&A, &B, lhs, st) : by_conjugate(&A, &B, shown->kids[1], lhs, st, out);
        if (!out->ok) return 1;
        if (!ok) { st->n = before; return -1; }
        const char *ans = sum_text(&whole);
        Step *last = st->n ? &st->v[st->n - 1] : NULL;
        if (last && last->math && !strstr(last->math, asprintf_a("=%s", ans)))
            steps_add(st, "Simplify", "", "%s", ans);
        return 1;
    }
    if (cu_surd_expand(shown, &whole, st)) return 1;
    st->n = before;
    return 0;
}

void calc_surd(const Expr *t, const CalcSettings *s, CalcResult *out)
{
    Steps *st = &out->steps;
    if (!cu_domain_ok(t, s->degrees, out)) return;
    SurdSum whole, A0, B0;
    if (t->kind == E_DIV && t->nkids == 2 && ss_from_expr(t->kids[0], &A0) && ss_from_expr(t->kids[1], &B0) &&
        ss_has_surd(&B0) && B0.n > 2) {
        cu_fail(out, "Can't rationalise a bottom with 3 or more terms");
        return;
    }
    if (!ss_from_expr(t, &whole)) {
        if (ex_has_var(t, NULL)) calc_simplify(t, s, out);
        else calc_eval(t, s, out);
        return;
    }
    int w = cu_surd_working(t, st, out);
    if (!out->ok) return;
    if (w < 0) { cu_fail(out, "Number too big"); return; }
    if (w > 0) { finish(&whole, out); return; }
    int nroot = 0;
    Expr *shown = simplify_roots(t, st, &nroot, 0);
    const char *lhs = P(shown);
    if (!ss_has_surd(&whole)) {
        st->n = 0;
        calc_eval(t, s, out);
        return;
    }
    const char *ans = sum_text(&whole);
    if (strcmp(lhs, ans) != 0)
        steps_add(st, t->kind == E_ADD ? "Collect like surds" : "Simplify",
                  t->kind == E_ADD ? "Add the numbers in front of the same root" : "", "%s=%s", lhs, ans);
    if (st->n == 0) steps_add(st, "Already as simple as it can be", "", NULL);
    finish(&whole, out);
}
