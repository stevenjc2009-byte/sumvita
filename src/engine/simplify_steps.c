/* simp_working: the Simplify action with GCSE working, for the cases a teacher
 * would show line by line:
 *   √(4a^2)            -> "Square root each part"            2a  (taking a ≥ 0)
 *   (x+1)/2+(x-1)/3    -> "Write over a common denominator 6", "Add the tops",
 *                         "Expand the brackets", "Collect like terms"   (5x+1)/6
 *   3(x+1)-3x          -> "Expand the brackets", "Collect like terms"   3
 * Anything else returns false and the caller keeps its own (ex_simplify) path. */
#include <string.h>
#include "alg_int.h"
#include "simplify_int.h"

#define SW_MAXT 24      /* top-level terms handled */
#define SW_MAXD 64      /* display terms after expanding */

typedef struct { int sign; const Expr *top; int64_t den; } SwTerm;

static bool has_sum_inside(const Expr *e, bool under)
{
    if (e->kind == E_ADD && under) return true;
    bool u = under || e->kind == E_MUL || e->kind == E_POW || e->kind == E_NEG || e->kind == E_DIV;
    for (int i = 0; i < e->nkids; i++)
        if (has_sum_inside(e->kids[i], u)) return true;
    return false;
}

/* top-level terms with their sign; a term n/d with whole d > 1 keeps d apart */
static void split_terms(const Expr *e, int sign, SwTerm *t, int *n)
{
    if (e->kind == E_ADD) {
        for (int i = 0; i < e->nkids; i++) split_terms(e->kids[i], sign, t, n);
        return;
    }
    if (e->kind == E_NEG) { split_terms(e->kids[0], -sign, t, n); return; }
    if (*n >= SW_MAXT) { *n = SW_MAXT + 1; return; }
    SwTerm *s = &t[(*n)++];
    s->sign = sign;
    s->top = e;
    s->den = 1;
    const Expr *d = e->kind == E_DIV ? e->kids[1] : NULL;
    if (d && d->kind == E_NUM && d->r.d == 1 && d->r.n > 1) {
        s->top = e->kids[0];
        s->den = d->r.n;
    }
}

static Expr *bare(const Expr *e)
{
    Expr *c = ex_clone(e);
    c->flags &= ~(unsigned)EF_PAREN;
    return c;
}

/* the terms of p appended, uncollected, to list d */
static bool push_terms(const Poly *p, Poly *d, int *nd)
{
    Poly tmp[SW_MAXD];
    int k;
    if (!alg_display_terms(p, tmp, SW_MAXD, &k) || *nd + k > SW_MAXD) return false;
    for (int i = 0; i < k; i++) d[(*nd)++] = tmp[i];
    return true;
}

/* √(k·m) with k a square number and every power in m even: √(4a^2) -> 2a */
static Expr *root_parts(const Expr *e, bool *did)
{
    if (e->kind == E_FUNC && e->op == F_SQRT && e->nkids == 1) {
        Poly p, mono, rm;
        Rat k;
        int64_t rn, rd;
        if (poly_from_expr(e->kids[0], &p) && p.n == 1 && alg_term(&p, 0, &k, &mono, NULL) &&
            poly_total_degree(&mono) > 0 && !rat_is_neg(k) && i64_isqrt_exact(k.n, &rn) &&
            i64_isqrt_exact(k.d, &rd) && alg_mono_root(&mono, 2, &rm)) {
            *did = true;
            return alg_km_expr(rat_make(rn, rd), &rm);
        }
    }
    Expr *c = ex_clone(e);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = root_parts(e->kids[i], did);
    return c;
}

/* letters under the root, for the note: "a" or "x and y" */
static const char *root_letters(const Expr *e)
{
    char names[8][12];
    int n = ex_vars(e, names, 8);
    if (n <= 0) return "the letters";
    if (n == 1) return astrdup(names[0]);
    const char *s = astrdup(names[0]);
    for (int i = 1; i < n; i++) s = asprintf_a("%s%s%s", s, i == n - 1 ? " and " : ", ", names[i]);
    return s;
}

static bool fraction_path(const Expr *t, SwTerm *tm, int n, Steps *st, Expr **out)
{
    int64_t L = 1;
    int nfrac = 0;
    for (int i = 0; i < n; i++) {
        if (tm[i].den > 1) nfrac++;
        if (!i64_lcm(L, tm[i].den, &L) || L > 1000000) return false;
    }
    if (nfrac < 2) return false;
    Expr **over = amalloc(sizeof(Expr *) * (size_t)n), **tops = amalloc(sizeof(Expr *) * (size_t)n);
    Poly num = poly_zero(), d[SW_MAXD];
    int nd = 0;
    const char *dens = "";
    for (int i = 0; i < n; i++) {
        int64_t k = L / tm[i].den;
        Expr *top = k == 1 ? bare(tm[i].top) : ex_mul2(ex_int(k), bare(tm[i].top));
        Expr *signed_top = tm[i].sign < 0 ? ex_neg(top) : top;
        over[i] = ex_div2(signed_top, ex_int(L));
        tops[i] = signed_top;
        Poly p, kp;
        if (!poly_from_expr(tm[i].top, &p) || !poly_scale(&p, rat_int(k * tm[i].sign), &kp) ||
            !poly_add(&num, &kp, &num) || !push_terms(&kp, d, &nd))
            return false;
        if (tm[i].den > 1) dens = *dens ? asprintf_a("%s and %lld", dens, (long long)tm[i].den)
                                        : asprintf_a("%lld", (long long)tm[i].den);
    }
    const char *echo = ex_print(t);
    steps_add(st, asprintf_a("Write over a common denominator %lld", (long long)L),
              asprintf_a("%lld is the lowest common multiple of %s. Multiply the top and bottom of each "
                         "fraction so every bottom is %lld.", (long long)L, dens, (long long)L),
              "%s=%s", echo, ex_print(ex_node(E_ADD, n, over)));
    Expr *L_e = ex_int(L);
    Expr *one = ex_div2(ex_node(E_ADD, n, tops), L_e);
    steps_add(st, "Add the tops", "The bottoms are the same, so add the tops and keep the bottom.", "%s", ex_print(one));
    Expr *expd = ex_div2(alg_sum_of(d, nd), L_e);
    if (has_sum_inside(ex_node(E_ADD, n, tops), false))
        steps_add(st, "Expand the brackets", "Multiply each term inside the bracket by the number outside.", "%s",
                  ex_print(expd));
    Expr *coll = ex_div2(alg_pexpr(&num), L_e);
    if (num.n < nd)
        steps_add(st, "Collect like terms", "Add the terms with the same letters and powers.", "%s", ex_print(coll));
    Rat c;
    int64_t g = L;
    if (poly_content(&num, &c) && c.d == 1) g = i64_gcd(c.n, L);
    else g = 1;
    if (poly_is_zero(&num)) {
        *out = ex_int(0);
    } else if (g > 1) {
        Poly r;
        if (!poly_scale(&num, rat_make(1, g), &r)) return false;
        *out = L / g == 1 ? alg_pexpr(&r) : ex_div2(alg_pexpr(&r), ex_int(L / g));
        steps_add(st, asprintf_a("Cancel the common factor %lld", (long long)g),
                  asprintf_a("Every term on the top and the bottom divides by %lld.", (long long)g), "%s=%s",
                  ex_print(coll), ex_print(*out));
    } else {
        *out = coll;
    }
    return true;
}

static bool bracket_path(const Expr *t, SwTerm *tm, int n, Steps *st, Expr **out)
{
    Poly all = poly_zero(), d[SW_MAXD];
    int nd = 0;
    for (int i = 0; i < n; i++) {
        Poly p, sp;
        const Expr *e = tm[i].den > 1 ? ex_div2(bare(tm[i].top), ex_int(tm[i].den)) : tm[i].top;
        if (!poly_from_expr(e, &p) || !poly_scale(&p, rat_int(tm[i].sign), &sp) || !poly_add(&all, &sp, &all) ||
            !push_terms(&sp, d, &nd))
            return false;
    }
    if (all.n >= nd) return false;              /* expanding collects nothing: not a simplification */
    bool sq = false;
    for (int i = 0; i < n; i++) {
        const Expr *e = tm[i].top;
        if (e->kind == E_POW && e->kids[0]->kind == E_ADD) sq = true;
    }
    steps_add(st, "Expand the brackets",
              sq ? "Multiply every term inside each bracket by what is outside it. A squared bracket "
                   "(a+b)^2 means (a+b)(a+b)."
                 : "Multiply every term inside each bracket by what is outside it.",
              "%s=%s", ex_print(t), ex_print(alg_sum_of(d, nd)));
    *out = alg_pexpr(&all);
    steps_add(st, "Collect like terms", "Add the terms with the same letters and powers.", "%s", ex_print(*out));
    return true;
}

bool simp_working(const Expr *t, Steps *st, Expr **out)
{
    if (!t || !out || t->kind == E_REL || t->kind == E_LIST) return false;
    *out = NULL;
    bool rooted = false;
    Expr *e = root_parts(t, &rooted);
    if (rooted) {
        steps_add(st, "Square root each part",
                  asprintf_a("√(a^2)=a and √4=2. This takes %s ≥ 0, as GCSE questions do.", root_letters(t)),
                  "%s=%s", ex_print(t), ex_print(e));
        Expr *ans = ex_tidy(ex_simplify(e)), *more = NULL;
        if (simp_working(e, st, &more) && more) {
            ans = more;
        } else if (strcmp(ex_print(ans), ex_print(e)) != 0) {
            steps_add(st, "Collect like terms", "Add the terms with the same letters and powers.", "%s",
                      ex_print(ans));
        }
        *out = ans;
        return true;
    }
    SwTerm tm[SW_MAXT];
    int n = 0;
    split_terms(t, 1, tm, &n);
    if (n > SW_MAXT || n < 2) return false;
    for (int i = 0; i < n; i++) {
        Poly p;
        if (!poly_from_expr(tm[i].top, &p)) return false;
    }
    if (fraction_path(t, tm, n, st, out)) return true;
    if (!has_sum_inside(t, false)) return false;
    return bracket_path(t, tm, n, st, out);
}
