/* Changing the subject of a formula. When the subject appears once it is
 * "peeled": each step undoes the outermost operation round it with the inverse
 * operation on both sides. When it appears more than once, rearrange_lin.c
 * collects the subject terms and factorises it out. */
#include "solve_int.h"
#include "arena.h"
#include "rearrange_tidy.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    SvCtx *c;
    const char *v;
    Expr *F, *K;     /* F holds the subject, K does not */
    bool left;       /* F is written on the left */
} Rr;

int rr_count(const Expr *e, const char *v)
{
    if (!e) return 0;
    if (e->kind == E_VAR) return strcmp(e->name, v) == 0;
    int n = 0;
    for (int i = 0; i < e->nkids; i++) n += rr_count(e->kids[i], v);
    return n;
}

Expr *rr_par(const Expr *e)
{
    Expr *x = ex_clone(e);
    if (x->kind == E_ADD || x->kind == E_NEG) x->flags |= EF_PAREN;
    return x;
}

Expr *rr_neg(const Expr *e)
{
    if (e->kind == E_NUM) return ex_num(rat_neg(e->r));
    if (e->kind == E_NEG) return e->kids[0];
    if (e->kind == E_ADD) {
        Expr *k[32];
        int n = e->nkids > 32 ? 32 : e->nkids;
        for (int i = 0; i < n; i++) k[i] = rr_neg(e->kids[i]);
        return sv_nice(ex_node(E_ADD, n, k));
    }
    if (e->kind == E_MUL && e->kids[0]->kind == E_NUM) {
        Expr *m = ex_clone(e);
        m->kids[0] = ex_num(rat_neg(e->kids[0]->r));
        if (rat_is_one(m->kids[0]->r)) return m->nkids == 2 ? m->kids[1] : ex_node(E_MUL, m->nkids - 1, m->kids + 1);
        return m;
    }
    return ex_neg(ex_clone(e));
}

Expr *rr_div(Expr *K, const Expr *d)
{
    if (ex_is_one(d)) return K;
    if (K->kind == E_DIV) return ex_div2(K->kids[0], ex_mul2(rr_par(K->kids[1]), rr_par(d)));
    return ex_div2(rr_par(K), rr_par(d));
}

Expr *rr_mul(Expr *K, const Expr *m)
{
    if (K->kind == E_DIV && ex_equal(K->kids[1], m)) return K->kids[0];
    if (m->kind == E_NUM && K->kind == E_DIV) return ex_div2(ex_mul2(ex_clone(m), rr_par(K->kids[0])), K->kids[1]);
    if (m->kind == E_NUM) return ex_mul2(ex_clone(m), rr_par(K));
    if (K->kind == E_DIV && (m->kind == E_VAR || m->kind == E_CONST)) return ex_div2(ex_mul2(ex_clone(m), rr_par(K->kids[0])), K->kids[1]);
    return ex_mul2(rr_par(K), rr_par(m));
}

static void show(Rr *r, const char *title, const char *note)
{
    r->F->flags &= ~(unsigned)EF_PAREN;   /* a whole side needs no bracket */
    r->K->flags &= ~(unsigned)EF_PAREN;
    sv_step(r->c, title, note, ex_print(r->left ? ex_rel(R_EQ, r->F, r->K) : ex_rel(R_EQ, r->K, r->F)));
}

/* product of all kids except i (NULL if none) */
static Expr *others(const Expr *e, int i)
{
    Expr *k[32];
    int n = 0;
    for (int j = 0; j < e->nkids && n < 32; j++)
        if (j != i) k[n++] = e->kids[j];
    return n == 0 ? NULL : n == 1 ? k[0] : ex_node(e->kind, n, k);
}

static int var_kid(const Expr *e, const char *v)
{
    for (int i = 0; i < e->nkids; i++)
        if (rr_count(e->kids[i], v)) return i;
    return -1;
}

static bool has_add(const Expr *e)
{
    if (e->kind == E_ADD || e->kind == E_PM || e->kind == E_FUNC) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_add(e->kids[i])) return true;
    return false;
}

/* K^n; a fraction of products is raised top and bottom: (T/(2π))^2 -> T^2/(4π^2) */
static Expr *pow_side(Expr *K, const Expr *n)
{
    if (K->kind == E_DIV && !has_add(K) && ex_is_numeric(n)) {
        Expr *t = ex_tidy(ex_simplify(ex_pow2(ex_clone(K->kids[0]), ex_clone(n))));
        Expr *b = ex_tidy(ex_simplify(ex_pow2(ex_clone(K->kids[1]), ex_clone(n))));
        if (t->kind != E_DIV && b->kind != E_DIV) return ex_div2(t, b);
    }
    return ex_pow2(rr_par(K), ex_clone(n));
}

/* ±e (an even root has two signs) */
static Expr *pm_of(Expr *e) { return ex_node(E_PM, 1, &e); }

static bool is_pm1(const Expr *e) { return e->kind == E_PM && e->nkids == 1; }

static bool peel_add(Rr *r)
{
    Expr *F = r->F;
    int i = var_kid(F, r->v), j = i == 0 ? 1 : 0;
    Expr *t = F->kids[j];
    if (is_pm1(r->K)) {                    /* x+a=±s  ->  x=-a±s */
        Expr *k2[2] = { rr_neg(t), r->K->kids[0] };
        r->K = ex_node(E_PM, 2, k2);
        r->F = others(F, j);
        if (t->kind == E_NEG || (t->kind == E_NUM && rat_is_neg(t->r)))
            show(r, sv_fmt("Add %s to both sides", ex_print(rr_neg(t))), NULL);
        else
            show(r, sv_fmt("Subtract %s from both sides", ex_print(t)), NULL);
        return true;
    }
    if (t->kind == E_NEG) {
        r->K = ex_is_zero(r->K) ? ex_clone(t->kids[0]) : ex_add2(r->K, ex_clone(t->kids[0]));
        r->F = others(F, j);
        show(r, sv_fmt("Add %s to both sides", ex_print(t->kids[0])), NULL);
    } else if (t->kind == E_NUM && rat_is_neg(t->r)) {
        r->K = ex_is_zero(r->K) ? ex_num(rat_neg(t->r)) : ex_add2(r->K, ex_num(rat_neg(t->r)));
        r->F = others(F, j);
        show(r, sv_fmt("Add %s to both sides", sv_rat_str(rat_neg(t->r))), NULL);
    } else {
        r->K = ex_is_zero(r->K) ? rr_neg(t) : ex_add2(r->K, rr_neg(t));
        r->F = others(F, j);
        show(r, sv_fmt("Subtract %s from both sides", ex_print(t)), NULL);
    }
    return true;
}

static bool peel_mul(Rr *r)
{
    Expr *F = r->F;
    int i = var_kid(F, r->v);
    for (int j = 0; j < F->nkids; j++) {
        Expr *k = F->kids[j];
        if (j == i || !ex_is_numeric(k)) continue;
        k = ex_simplify(k);
        if (!k || k->kind != E_NUM || rat_is_int(k->r)) continue;
        Rat den = rat_int(k->r.d), num = rat_int(k->r.n);
        r->K = rr_mul(r->K, ex_num(den));
        Expr *nf = ex_clone(F);
        if (rat_is_one(num)) nf = others(F, j);
        else nf->kids[j] = ex_num(num);
        r->F = nf;
        show(r, sv_fmt("Multiply both sides by %s", sv_rat_str(den)), "This clears the fraction.");
        return true;
    }
    Expr *d = others(F, i);
    if (d->kind == E_NUM && rat_eq(d->r, rat_int(-1))) {
        r->K = rr_neg(r->K);
        r->F = F->kids[i];
        show(r, "Multiply both sides by -1", NULL);
        return true;
    }
    r->K = rr_div(r->K, d);
    r->F = F->kids[i];
    show(r, sv_fmt("Divide both sides by %s", ex_print(rr_par(d))), NULL);
    return true;
}

static bool peel_pow(Rr *r)
{
    Expr *b = r->F->kids[0], *n = r->F->kids[1];
    long long k;
    if (rr_count(b, r->v)) {
        r->F = b;
        if (ex_is_int(n, &k) && k == 2) {
            r->K = pm_of(ex_func1(F_SQRT, r->K));
            show(r, "Square root both sides", "A square root can be positive or negative, so write ±.");
        } else if (ex_is_int(n, &k) && k > 2) {
            r->K = ex_func2(F_ROOT, ex_int(k), r->K);
            if (k % 2 == 0) r->K = pm_of(r->K);
            show(r, sv_fmt("Take the %s root of both sides", k == 3 ? "cube" : sv_fmt("%lldth", k)),
                 k % 2 == 0 ? "An even root can be positive or negative, so write ±." : NULL);
        } else if (n->kind == E_NUM && rat_eq(n->r, rat_make(1, 2))) {
            r->K = pow_side(r->K, ex_int(2));
            show(r, "Square both sides", NULL);
        } else {
            r->K = ex_pow2(rr_par(r->K), ex_div2(ex_int(1), rr_par(n)));
            show(r, sv_fmt("Raise both sides to the power 1/%s", ex_print(rr_par(n))), NULL);
        }
        return true;
    }
    r->F = n;
    if (b->kind == E_CONST && b->op == C_E) {
        r->K = ex_func1(F_LN, r->K);
        show(r, "Take ln of both sides", NULL);
    } else {
        r->K = ex_func2(F_LOG, ex_clone(b), r->K);
        show(r, "Take logs of both sides", sv_fmt("log(%s, ...) undoes raising %s to a power.", ex_print(b), ex_print(b)));
    }
    return true;
}

static bool peel_func(Rr *r)
{
    Expr *F = r->F, *K = r->K;
    static const struct { int f, inv; const char *name; } T[] = {
        { F_SIN, F_ASIN, "sin^-1" }, { F_COS, F_ACOS, "cos^-1" }, { F_TAN, F_ATAN, "tan^-1" },
        { F_ASIN, F_SIN, "sin" }, { F_ACOS, F_COS, "cos" }, { F_ATAN, F_TAN, "tan" },
        { F_EXP, F_LN, "ln" },
    };
    if (F->op == F_SQRT && F->nkids == 1) {
        r->F = F->kids[0]; r->K = pow_side(K, ex_int(2));
        show(r, "Square both sides", NULL);
        return true;
    }
    if (F->op == F_ROOT && F->nkids == 2 && !rr_count(F->kids[0], r->v)) {
        r->F = F->kids[1]; r->K = pow_side(K, F->kids[0]);
        show(r, sv_fmt("Raise both sides to the power %s", ex_print(F->kids[0])), NULL);
        return true;
    }
    if (F->op == F_LN && F->nkids == 1) {
        r->F = F->kids[0]; r->K = ex_pow2(ex_const(C_E), rr_par(K));
        show(r, "Raise e to the power of both sides", NULL);
        return true;
    }
    if (F->op == F_LOG && (F->nkids == 1 || !rr_count(F->kids[0], r->v))) {
        Expr *b = F->nkids == 1 ? ex_int(10) : F->kids[0];
        r->F = F->kids[F->nkids - 1]; r->K = ex_pow2(rr_par(b), rr_par(K));
        show(r, sv_fmt("Raise %s to the power of both sides", ex_print(b)), NULL);
        return true;
    }
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        if (F->op != T[i].f || F->nkids != 1) continue;
        r->F = F->kids[0]; r->K = ex_func1(T[i].inv, K);
        show(r, sv_fmt("Take %s of both sides", T[i].name), NULL);
        return true;
    }
    return false;
}

static bool peel(Rr *r)
{
    for (int guard = 0; guard < 40; guard++) {
        Expr *F = r->F;
        if (F->kind == E_VAR) return true;
        switch (F->kind) {
        case E_ADD: if (!peel_add(r)) return false; break;
        case E_MUL: if (!peel_mul(r)) return false; break;
        case E_NEG:
            r->F = F->kids[0]; r->K = rr_neg(r->K);
            show(r, "Multiply both sides by -1", NULL);
            break;
        case E_DIV:
            if (rr_count(F->kids[0], r->v)) {
                r->F = F->kids[0]; r->K = rr_mul(r->K, F->kids[1]);
            } else {
                r->F = rr_mul(r->K, F->kids[1]); r->K = F->kids[0];
            }
            show(r, sv_fmt("Multiply both sides by %s", ex_print(rr_par(F->kids[1]))), NULL);
            break;
        case E_POW: if (!peel_pow(r)) return false; break;
        case E_FUNC: if (!peel_func(r)) return false; break;
        default: return false;
        }
    }
    return false;
}

/* π is not a polynomial letter, so for collecting it stands in as the letter
 * "#" (sorts first, so πr^2 keeps π in front) and is put back afterwards. */
#define PI_LETTER "#"

static Expr *pi_as_letter(const Expr *e)
{
    if (e->kind == E_CONST && e->op == C_PI) return ex_var(PI_LETTER);
    Expr *c = ex_clone(e);
    for (int i = 0; i < e->nkids; i++) c->kids[i] = pi_as_letter(e->kids[i]);
    return c;
}

static bool has_pi(const Expr *e)
{
    if (e->kind == E_CONST && e->op == C_PI) return true;
    for (int i = 0; i < e->nkids; i++)
        if (has_pi(e->kids[i])) return true;
    return false;
}

static const char *pi_back(const char *s)
{
    if (!s || !strchr(s, '#')) return s;
    char *o = amalloc(strlen(s) * 2 + 1), *w = o;
    for (; *s; s++) {
        if (*s == '#') { *w++ = (char)0xCF; *w++ = (char)0x80; }
        else *w++ = *s;
    }
    *w = 0;
    return o;
}

static bool collect_pi(SvCtx *c, const Expr *e, Expr **out)
{
    if (!has_pi(e)) return rr_collect(c, e->kids[0], e->kids[1], out);
    int n0 = c->st ? c->st->n : 0;
    bool ok = rr_collect(c, pi_as_letter(e->kids[0]), pi_as_letter(e->kids[1]), out);
    for (int i = n0; c->st && i < c->st->n; i++) {
        Step *s = &c->st->v[i];
        s->title = pi_back(s->title);
        s->note = pi_back(s->note);
        s->math = pi_back(s->math);
    }
    if (c->err) {
        const char *m = pi_back(c->err);
        if (m != c->err) snprintf(c->err, (size_t)c->errcap, "%s", m);
    }
    if (ok && *out) *out = ex_subst(*out, PI_LETTER, ex_const(C_PI));
    return ok;
}

bool alg_rearrange(const Expr *e, const char *var, Steps *st, Expr **out, char *err, int errcap)
{
    SvCtx c;
    memset(&c, 0, sizeof c);
    c.st = st; c.err = err; c.errcap = errcap;
    if (err && errcap > 0) err[0] = 0;
    if (!out) return sv_fail(&c, "Nothing to rearrange");
    *out = NULL;
    if (!e || e->kind != E_REL || e->op != R_EQ || e->nkids != 2) return sv_fail(&c, "Type a formula with =");
    if (!var || !var[0]) return sv_fail(&c, "Which letter should be the subject?");
    snprintf(c.var, sizeof c.var, "%s", var);
    int nl = rr_count(e->kids[0], var), nr = rr_count(e->kids[1], var);
    if (nl + nr == 0) return sv_fail(&c, sv_fmt("There is no %s in this formula", var));
    c.last = ex_print(e);
    if (nl + nr > 1) {
        if (!collect_pi(&c, e, out)) return false;
        Expr *t = *out ? rr_tidy((*out)->kids[1]) : NULL;
        if (t) {
            *out = ex_rel(R_EQ, ex_var(var), t);
            sv_step(&c, "Simplify the fraction", NULL, ex_print(*out));
        }
        return true;
    }
    Rr r = { &c, c.var, ex_clone(nl ? e->kids[0] : e->kids[1]), ex_clone(nl ? e->kids[1] : e->kids[0]), nl > 0 };
    if (!peel(&r))
        return sv_fail(&c, sv_fmt("Can't make %s the subject: there is no inverse operation to undo what is done to %s",
                                  var, var));
    Expr *tidy = rr_tidy(r.K);
    if (tidy) {
        r.K = tidy;
        show(&r, "Simplify the fraction", NULL);
    }
    if (!r.left) {
        r.left = true;
        show(&r, "Swap the sides", NULL);
    }
    *out = ex_rel(R_EQ, ex_var(var), r.K);
    return true;
}
