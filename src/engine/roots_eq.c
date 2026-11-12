/* "x=-2 and x=-3" (or "x=-2, x=-3", "x=3±√5"): the solutions of one letter ->
 * the equation they solve, with GCSE working: each solution gives a bracket,
 * multiply the brackets, expand, collect like terms.
 *   rational p/q   -> bracket (qx-p), the fraction cleared straight away
 *   surd a+c√b     -> must come with its partner a-c√b; the pair's brackets
 *                     multiply to (x-a)^2-c²b (difference of two squares)
 * calc.c calls calc_roots_eq() for Auto, Solve and Expand before the solver.
 * The form: every item is <letter>=<number>, all the same letter (x and X count
 * as one letter here only), and either two or more of them or one with a ±.
 * "x=2" alone and anything with a letter on the right stay with the solver. */
#include <stdio.h>
#include <string.h>

#include "algebra.h"
#include "arena.h"
#include "calc_util.h"
#include "poly.h"

#define RQ_MAX 8

typedef struct { Rat a, c; int64_t b; bool surd, used; } Root;   /* a + c√b */
typedef struct { char var[12]; Root r[RQ_MAX]; int n; } RootSet;

static bool ascii_letter(const char *s)
{
    return s[1] == 0 && ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z'));
}

static bool same_letter(const char *x, const char *y)
{
    if (strcmp(x, y) == 0) return true;
    return ascii_letter(x) && ascii_letter(y) && (x[0] | 0x20) == (y[0] | 0x20);
}

static bool has_kind(const Expr *e, ExprKind k)
{
    if (e->kind == k) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_kind(e->kids[i], k)) return true;
    return false;
}

static int count_pm(const Expr *e)
{
    int n = e->kind == E_PM;
    for (int i = 0; i < e->nkids; i++) n += count_pm(e->kids[i]);
    return n;
}

/* copy of e with its ± read as + (minus false) or - (minus true) */
static Expr *pick_pm(const Expr *e, bool minus)
{
    if (e->kind == E_PM) {
        Expr *b = pick_pm(e->kids[e->nkids - 1], minus);
        if (minus) b = ex_neg(b);
        return e->nkids == 1 ? b : ex_add2(pick_pm(e->kids[0], minus), b);
    }
    if (!e->nkids) return (Expr *)e;
    Expr *c = amalloc(sizeof(Expr));
    *c = *e;
    c->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = pick_pm(e->kids[i], minus);
    return c;
}

/* <letter>=<number> either way round */
static bool rel_parts(const Expr *r, const Expr **var, const Expr **val)
{
    if (r->kind != E_REL || r->nkids != 2 || r->op != R_EQ) return false;
    for (int s = 0; s < 2; s++) {
        const Expr *v = r->kids[s], *n = r->kids[1 - s];
        if (v->kind == E_VAR && !ex_has_var(n, NULL) && !has_kind(n, E_TEXT)) {
            *var = v;
            *val = n;
            return true;
        }
    }
    return false;
}

/* 1 ok, 0 not a number we can use, -1 more than one surd */
static int to_root(const Expr *v, Root *out)
{
    SurdSum s;
    if (!ss_from_expr(v, &s)) return 0;
    memset(out, 0, sizeof *out);
    out->a = out->c = rat_int(0);
    out->b = 1;
    for (int i = 0; i < s.n; i++) {
        if (s.t[i].b == 1) { out->a = s.t[i].c; continue; }
        if (out->surd) return -1;
        out->surd = true;
        out->c = s.t[i].c;
        out->b = s.t[i].b;
    }
    return 1;
}

/* 0: not this form (the solver has it); 1: collected; -1: out->error set */
static int collect(const Expr *t, RootSet *rs, CalcResult *out)
{
    const Expr *items[2 * RQ_MAX], *vars[2 * RQ_MAX];
    int ni = 0;
    if (t->kind == E_REL) {
        items[ni++] = t;
    } else if (t->kind == E_LIST) {
        for (int i = 0; i < t->nkids; i++) {
            if (t->kids[i]->kind == E_TEXT) continue;
            if (ni == 2 * RQ_MAX) return 0;
            items[ni++] = t->kids[i];
        }
    }
    if (ni == 0) return 0;
    const Expr *vals[2 * RQ_MAX];
    for (int i = 0; i < ni; i++) {
        if (!rel_parts(items[i], &vars[i], &vals[i])) return 0;
        if (!same_letter(vars[i]->name, vars[0]->name)) return 0;
    }
    if (ni == 1 && count_pm(vals[0]) == 0) return 0;        /* "x=2" alone: the solver's */
    snprintf(rs->var, sizeof rs->var, "%s", vars[0]->name);
    for (int i = 0; i < ni; i++)                             /* X and x: write x */
        if (ascii_letter(vars[i]->name) && vars[i]->name[0] >= 'a') snprintf(rs->var, sizeof rs->var, "%s", vars[i]->name);
    rs->n = 0;
    for (int i = 0; i < ni; i++) {
        int pm = count_pm(vals[i]);
        if (pm > 1) { cu_fail(out, "Use \xC2\xB1 only once in each solution"); return -1; }
        for (int k = 0; k <= pm; k++) {
            if (rs->n == RQ_MAX) { cu_fail(out, "Too many solutions: %d at most", RQ_MAX); return -1; }
            const Expr *v = pm ? pick_pm(vals[i], k == 1) : vals[i];
            int ok = to_root(v, &rs->r[rs->n]);
            if (ok == 0) { cu_fail(out, "Each solution must be a number, a fraction or a surd"); return -1; }
            if (ok < 0) { cu_fail(out, "Each solution can only have one surd in it"); return -1; }
            rs->n++;
        }
    }
    return 1;
}

/* ---- text pieces ---- */

static const char *join(const char *head, const char *term)
{
    if (!*head) return term;
    return asprintf_a(term[0] == '-' ? "%s%s" : "%s+%s", head, term);
}

static const char *surd_str(Rat c, int64_t b)
{
    SurdSum s = {.n = 1};
    s.t[0].c = c;
    s.t[0].b = b;
    return ex_print(ss_to_expr(&s));
}

/* a+c√b as text, "3+√5" */
static const char *root_str(const Root *r)
{
    const char *h = r->surd && rat_is_zero(r->a) ? "" : cu_rat_str(r->a);
    return r->surd ? join(h, surd_str(r->c, r->b)) : h;
}

/* x minus the root: "x+2", "x-1/2", "x-3-√5"; just "x" for 0 */
static const char *minus_root(const char *x, const Root *r)
{
    const char *s = rat_is_zero(r->a) ? x : join(x, cu_rat_str(rat_neg(r->a)));
    return r->surd ? join(s, surd_str(rat_neg(r->c), r->b)) : s;
}

static const char *paren(const char *s, bool bare)
{
    return bare ? s : asprintf_a("(%s)", s);
}

/* ---- polynomials as coefficient arrays c[0..deg] ---- */

static bool up_mul(Rat *a, int *da, const Rat *b, int db)
{
    Rat r[2 * RQ_MAX + 1];
    for (int k = 0; k <= *da + db; k++) r[k] = rat_int(0);
    for (int i = 0; i <= *da; i++)
        for (int j = 0; j <= db; j++) {
            Rat t;
            if (!rat_mul(a[i], b[j], &t) || !rat_add(r[i + j], t, &r[i + j])) return false;
        }
    *da += db;
    memcpy(a, r, sizeof(Rat) * (size_t)(*da + 1));
    return true;
}

static const char *up_eq(const Rat *c, int deg, const char *var)
{
    Poly p = poly_from_upoly(c, deg, var);
    return ex_print(ex_rel(R_EQ, ex_tidy(poly_to_expr(&p, var)), ex_int(0)));
}

/* alg_expand's working on `prod`; its main lines get "=0" after them, its
 * nested side workings (expanding one part, e.g. (x-3)^2) stay as they are */
static bool expand_steps(const char *prod, Steps *st)
{
    char err[160];
    int pos;
    if (!strchr(prod, '(')) return true;      /* x^2-12: no brackets left to expand */
    Expr *e = ex_parse(prod, err, sizeof err, &pos), *res = NULL;
    Steps tmp;
    steps_init(&tmp);
    if (!e || !alg_expand(e, &tmp, &res, err, sizeof err) || !res || tmp.n == 0) return false;
    int base = st->depth;
    for (int i = 0; i < tmp.n; i++) {
        const Step *s = &tmp.v[i];
        st->depth = base + s->depth;
        steps_add(st, s->title, s->note, *s->math ? (s->depth ? "%s" : "%s=0") : NULL, s->math);
    }
    st->depth = base;
    return true;
}

static const char *list_add(const char *l, const char *item)
{
    return *l ? asprintf_a("%s, %s", l, item) : item;
}

/* x, x^2, ... for the zero solutions */
static const char *x_pow(const char *x, int k)
{
    return k == 1 ? x : asprintf_a("%s^%d", x, k);
}

static void roots_build(RootSet *rs, CalcResult *out)
{
    const char *x = rs->var, *sol = "";
    const char *rb[RQ_MAX], *pb[RQ_MAX][2];
    Root pr[RQ_MAX];
    int nr = 0, np = 0, zeros = 0;
    Rat poly[2 * RQ_MAX + 1] = {rat_int(1)};
    int deg = 0;
    Steps *st = &out->steps;
    Steps clears;
    steps_init(&clears);
    for (int i = 0; i < rs->n; i++) {
        Root *r = &rs->r[i];
        Rat f[3];
        int fd = 1;
        if (r->surd) {
            if (r->used) continue;
            int j = i + 1;
            while (j < rs->n && !(rs->r[j].surd && !rs->r[j].used && rs->r[j].b == r->b &&
                                  rat_eq(rs->r[j].a, r->a) && rat_eq(rs->r[j].c, rat_neg(r->c))))
                j++;
            if (j == rs->n) {
                Root q = *r;
                q.c = rat_neg(r->c);
                cu_fail(out, "%s=%s needs its partner %s=%s: surd solutions come in pairs", x, root_str(r), x,
                        root_str(&q));
                return;
            }
            r->used = rs->r[j].used = true;
            pr[np] = *r;
            pb[np][0] = minus_root(x, r);
            pb[np][1] = minus_root(x, &rs->r[j]);
            sol = list_add(list_add(sol, asprintf_a("%s=0", pb[np][0])), asprintf_a("%s=0", pb[np][1]));
            np++;
            Rat a2, c2, k;                    /* (x-a)^2-c²b = x^2-2ax+(a²-c²b) */
            if (!rat_mul(r->a, r->a, &a2) || !rat_mul(r->c, r->c, &c2) || !rat_mul(c2, rat_int(r->b), &c2) ||
                !rat_sub(a2, c2, &k) || !rat_mul(r->a, rat_int(-2), &f[1])) {
                cu_fail(out, "Numbers too big");
                return;
            }
            f[0] = k;
            f[2] = rat_int(1);
            fd = 2;
        } else {
            sol = list_add(sol, asprintf_a("%s=0", minus_root(x, r)));
            if (rat_is_zero(r->a)) {
                zeros++;
                f[0] = rat_int(0);
                f[1] = rat_int(1);
            } else {
                int64_t q = r->a.d, p = r->a.n;
                const char *br = minus_root(x, r);
                if (q != 1) {
                    br = join(asprintf_a("%lld%s", (long long)q, x), asprintf_a("%lld", (long long)-p));
                    steps_add(&clears, asprintf_a("Multiply by %lld so there are no fractions", (long long)q), "",
                              "%s=0", br);
                }
                rb[nr++] = br;
                f[0] = rat_int(-p);
                f[1] = rat_int(q);
            }
        }
        if (!up_mul(poly, &deg, f, fd)) { cu_fail(out, "Numbers too big"); return; }
    }
    /* the brackets: zero solutions first (x(x-5)), then the rest as typed */
    const char *lead = zeros ? x_pow(x, zeros) : "", *prod = lead, *alt = lead, *dots = lead, *sq = lead;
    for (int i = 0; i < nr; i++) {
        prod = asprintf_a("%s(%s)", prod, rb[i]);
        int seen = 0, cnt = 0;
        for (int j = 0; j < nr; j++) {
            if (strcmp(rb[j], rb[i]) != 0) continue;
            if (j < i) seen = 1;
            cnt++;
        }
        if (!seen) alt = cnt > 1 ? asprintf_a("%s(%s)^%d", alt, rb[i], cnt) : asprintf_a("%s(%s)", alt, rb[i]);
    }
    dots = sq = prod;
    bool alone = np == 1 && !*prod;           /* just one pair: no outer brackets */
    for (int i = 0; i < np; i++) {
        prod = asprintf_a("%s(%s)(%s)", prod, pb[i][0], pb[i][1]);
        alt = asprintf_a("%s(%s)(%s)", alt, pb[i][0], pb[i][1]);
        Root *r = &pr[i];
        const char *xa = rat_is_zero(r->a) ? x : asprintf_a("(%s)", join(x, cu_rat_str(rat_neg(r->a))));
        Rat c2;
        (void)rat_mul(r->c, r->c, &c2);
        (void)rat_mul(c2, rat_int(r->b), &c2); /* overflow checked above */
        dots = asprintf_a("%s%s", dots, paren(asprintf_a("%s^2-(%s)^2", xa, surd_str(rat_abs(r->c), r->b)), alone));
        sq = asprintf_a("%s%s", sq, paren(asprintf_a("%s^2-%s", xa, cu_rat_str(c2)), alone));
    }
    steps_add(st, "Each solution gives a bracket", "Take each number to the other side so it equals 0", "%s", sol);
    for (int i = 0; i < clears.n; i++) steps_add(st, clears.v[i].title, "", "%s", clears.v[i].math);
    steps_add(st, "Multiply the brackets", "The brackets multiply to make 0", "%s=0", prod);
    if (np) {
        steps_add(st, "Use the difference of two squares", "(A-B)(A+B)=A\xC2\xB2-B\xC2\xB2", "%s=0", dots);
        steps_add(st, "Square the surd", "", "%s=0", sq);
    }
    if (!expand_steps(np ? sq : prod, st))
        steps_add(st, "Expand the brackets", "", "%s", up_eq(poly, deg, x));
    int64_t L = 1;
    for (int i = 0; i <= deg; i++)
        if (!i64_lcm(L, poly[i].d, &L)) { cu_fail(out, "Numbers too big"); return; }
    for (int i = 0; L > 1 && i <= deg; i++)
        if (!rat_mul(poly[i], rat_int(L), &poly[i])) { cu_fail(out, "Numbers too big"); return; }
    const char *ans = up_eq(poly, deg, x);
    if (L > 1) steps_add(st, asprintf_a("Multiply by %lld so there are no fractions", (long long)L), "", "%s", ans);
    steps_add(st, "So the equation is", "", "%s", ans);
    out->answer = ans;
    out->answer_alt = asprintf_a("%s=0", alt);
}

bool calc_roots_eq(const Expr *t, CalcResult *out)
{
    RootSet rs;
    int k = collect(t, &rs, out);
    if (k == 0) return false;
    if (k > 0) roots_build(&rs, out);
    return true;
}
