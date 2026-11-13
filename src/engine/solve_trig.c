/* Trig equations a*trig(kx+b) + d = c, solved for 0 <= x < 360 (degrees) or 0 <= x < 2pi
 * (radians): the principal value from the inverse function, the other solution from the
 * graph's symmetry, then every angle in range. Exact angles when there are some. */
#include "solve_ext.h"
#include <math.h>
#include <string.h>

#define MAXSOL 96
#define MAXOUT SV_MAXROOTS

typedef struct {
    bool deg;
    double full, half;
    Rat rfull, rhalf;        /* exact: degrees, or multiples of pi */
} Ang;

typedef struct {
    double u, x;
    bool uex, xex;
    Rat ur, xr;
} Sol;

bool sv_has_trig(const Expr *e, const char *var)
{
    if (e->kind == E_FUNC && (e->op == F_SIN || e->op == F_COS || e->op == F_TAN) && ex_has_var(e, var)) return true;
    for (int i = 0; i < e->nkids; i++)
        if (sv_has_trig(e->kids[i], var)) return true;
    return false;
}

static Expr *piexpr(Rat q)
{
    if (rat_is_zero(q)) return ex_int(0);
    Rat m = rat_abs(q);
    Expr *e = m.n == 1 ? ex_const(C_PI) : ex_mul2(ex_int(m.n), ex_const(C_PI));
    if (m.d != 1) e = ex_div2(e, ex_int(m.d));
    return rat_is_neg(q) ? ex_neg(e) : e;
}

static Expr *wexpr(SvCtx *c, const Ang *g, double d, bool ex, Rat r, bool frac_ok)
{
    if (ex && g->deg && (r.d == 1 || frac_ok)) return ex_num(r);
    if (ex && !g->deg) return piexpr(r);
    return sv_numexpr(c, d);
}

static bool alpha_exact(const Ang *g, double al, Rat *r)
{
    if (g->deg) {
        double ri = round(al);
        if (fabs(al - ri) > 1e-9) return false;
        *r = rat_int((int64_t)ri);
        return true;
    }
    static const int dens[] = { 1, 2, 3, 4, 5, 6, 8, 10, 12 };
    for (unsigned i = 0; i < sizeof dens / sizeof *dens; i++) {
        double t = al / M_PI * dens[i], ti = round(t);
        if (fabs(t - ti) < 1e-9) { *r = rat_make((int64_t)ti, dens[i]); return true; }
    }
    return false;
}

static int add_sol(Sol *s, int n, const Sol *t)
{
    for (int i = 0; i < n; i++)
        if (fabs(s[i].x - t->x) < 1e-9) return n;
    int j = n++;
    while (j > 0 && s[j - 1].x > t->x) { s[j] = s[j - 1]; j--; }
    s[j] = *t;
    return n;
}

/* every x in [0, full) with kx + b = base + n*full for a base angle */
static int enumerate(const Ang *g, const double bd[2], const bool bex[2], const Rat br[2], Rat k, Rat b, Sol *out)
{
    double kd = rat_to_double(k), bb = rat_to_double(b);
    double lo = fmin(bb, bb + kd * g->full), hi = fmax(bb, bb + kd * g->full);
    int n = 0;
    for (int j = 0; j < 2; j++) {
        int n0 = (int)floor((lo - bd[j]) / g->full) - 1, n1 = (int)ceil((hi - bd[j]) / g->full) + 1;
        for (int m = n0; m <= n1; m++) {
            Sol t;
            memset(&t, 0, sizeof t);
            t.u = bd[j] + m * g->full;
            t.x = (t.u - bb) / kd;
            if (t.x < -1e-9 || t.x > g->full - 1e-9) continue;
            if (fabs(t.x) < 1e-9) t.x = 0;
            Rat mr, u, x;
            t.uex = bex[j] && rat_mul(rat_int(m), g->rfull, &mr) && rat_add(br[j], mr, &u);
            if (t.uex) {
                t.ur = u;
                if (g->deg ? (rat_sub(u, b, &x) && rat_div(x, k, &x)) : (rat_is_zero(b) && rat_div(u, k, &x))) {
                    t.xex = true;
                    t.xr = x;
                }
            }
            n = add_sol(out, n, &t);
            if (n >= MAXSOL) return n;
        }
    }
    return n;
}

static const char *or_join(const char *s, const char *one)
{
    return *s ? sv_fmt("%s \"or\" %s", s, one) : one;
}

static const char *rel_line(const Expr *lhs, Expr *val)
{
    return ex_print(ex_rel(R_EQ, ex_clone(lhs), val));
}

static const char *num_str(double d)
{
    return sv_fmt("%.10g", fabs(d) < 1e-9 ? 0.0 : d);
}

/* the value the trig function must equal: (K - d) / a */
static Expr *numx(Rat t, bool dec)
{
    Expr *e = ex_num(t);
    if (dec && rat_terminates(t) && t.d != 1) e->flags |= NF_DECIMAL;
    return e;
}

static bool value_of(const Expr *K, Rat a, Rat d, bool deg, double *v, Expr **vx, bool *vrat)
{
    Rat kr, t;
    if ((*vrat = sv_exact_num(K, &kr))) {
        if (!rat_sub(kr, d, &t) || !rat_div(t, a, &t)) return false;
        *v = rat_to_double(t);
        *vx = numx(t, sv_has_decimal(K));
        return true;
    }
    if (!rat_is_one(a) || !rat_is_zero(d) || !ex_is_numeric(K)) return false;
    EvalEnv env;
    memset(&env, 0, sizeof env);
    env.degrees = deg;
    if (!ex_eval(K, &env, v)) return false;
    *vx = ex_tidy(ex_simplify(K));
    return true;
}

static void isolate_steps(SvCtx *c, const Expr *T, Rat a, Rat d, Rat kr, bool vrat, Expr *vx, bool dec)
{
    if (!vrat) return;
    Rat t;
    rat_sub(kr, d, &t);
    Expr *lhs = rat_is_one(a) ? ex_clone(T) : rat_eq(a, rat_int(-1)) ? ex_neg(ex_clone(T)) : ex_mul2(ex_num(a), ex_clone(T));
    if (!rat_is_zero(d))
        sv_step(c, rat_is_neg(d) ? "Add to both sides" : "Subtract from both sides", NULL,
                ex_print(ex_rel(R_EQ, lhs, numx(t, dec))));
    if (!rat_is_one(a)) sv_step(c, "Divide both sides", NULL, ex_print(ex_rel(R_EQ, ex_clone(T), vx)));
}

static void range_step(SvCtx *c, const Ang *g, const Sol *sol, int n, Rat k, Rat b, const char *argstr, const Expr *arg)
{
    double kd = rat_to_double(k), bb = rat_to_double(b);
    double lo = fmin(bb, bb + kd * g->full), hi = fmax(bb, bb + kd * g->full);
    const char *rng = NULL, *whole = g->deg ? "360" : "2\xCF\x80";
    if (g->deg) rng = sv_fmt("%s and %s", num_str(lo), num_str(hi));
    else if (rat_is_zero(b)) {
        Rat e;
        if (rat_mul(k, g->rfull, &e))
            rng = sv_fmt("%s and %s", ex_print(piexpr(rat_is_neg(e) ? e : rat_int(0))), ex_print(piexpr(rat_is_neg(e) ? rat_int(0) : e)));
    }
    const char *note = rng ? sv_fmt("x must be between %s, so %s is between %s. Add or subtract %s to find every angle in that range.",
                                    g->deg ? "0 and 360" : "0 and 2\xCF\x80", argstr, rng, whole)
                           : sv_fmt("Add or subtract %s to find every angle in range.", whole);
    const Sol *byu[MAXSOL];
    for (int i = 0; i < n; i++) byu[i] = &sol[i];
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && byu[j - 1]->u > byu[j]->u; j--) { const Sol *t = byu[j]; byu[j] = byu[j - 1]; byu[j - 1] = t; }
    const char *s = "";
    for (int i = 0; i < n; i++) s = or_join(s, rel_line(arg, wexpr(c, g, byu[i]->u, byu[i]->uex, byu[i]->ur, true)));
    sv_step(c, "List the angles in range", note, s);
}

bool sv_trig_eq(SvCtx *c, const Expr *L, const Expr *R, Expr **out)
{
    bool lv = ex_has_var(L, c->var), rv = ex_has_var(R, c->var);
    if (lv == rv) return false;
    const Expr *F = lv ? L : R, *K = lv ? R : L, *T = NULL;
    Rat a, d, hc[SV_MAXDEG + 1], kr = rat_int(0);
    Poly hp;
    int hd;
    if (!sv_lindec(c, F, 0, &a, &d, &T) || rat_is_zero(a) || !T) return false;
    if (!poly_from_expr(T->kids[0], &hp) || !sv_upoly(&hp, c->var, hc, &hd) || hd != 1) return false;
    Rat k = hc[1], b = hc[0];
    if (fabs(rat_to_double(k)) > 50) return false;
    double v;
    Expr *vx;
    bool vrat;
    if (!value_of(K, a, d, c->s ? c->s->degrees : true, &v, &vx, &vrat)) return false;
    if (vrat) sv_exact_num(K, &kr);

    Ang g;
    g.deg = c->s ? c->s->degrees : true;
    g.full = g.deg ? 360 : 2 * M_PI;
    g.half = g.full / 2;
    g.rfull = rat_int(g.deg ? 360 : 2);
    g.rhalf = rat_int(g.deg ? 180 : 1);
    int fop = T->op;
    const char *fname = fop == F_SIN ? "sin" : fop == F_COS ? "cos" : "tan";
    const char *argstr = ex_print(T->kids[0]);
    const char *fstr = T->kids[0]->kind == E_VAR ? sv_fmt("%s %s", fname, argstr) : sv_fmt("%s(%s)", fname, argstr);

    if (fop != F_TAN && fabs(v) > 1 + 1e-12) {
        isolate_steps(c, T, a, d, kr, vrat, vx, sv_has_decimal(K));
        const char *msg = sv_fmt("No solutions: %s is always between \xE2\x88\x92" "1 and 1", fstr);
        sv_step(c, "No solutions", msg, NULL);
        *out = ex_text(msg);
        return true;
    }
    if (fop != F_TAN) v = fmax(-1, fmin(1, v));
    double al = fop == F_SIN ? asin(v) : fop == F_COS ? acos(v) : atan(v);
    if (g.deg) al = al * 180 / M_PI;
    Rat ar = rat_int(0);
    bool aex = alpha_exact(&g, al, &ar);
    double bd[2] = { al, fop == F_SIN ? g.half - al : fop == F_COS ? g.full - al : al + g.half };
    bool bex[2] = { aex, aex };
    Rat br[2] = { ar, rat_int(0) };
    if (aex) {
        Rat t = rat_int(0);
        bool ok = fop == F_SIN ? rat_sub(g.rhalf, ar, &t) : fop == F_COS ? rat_sub(g.rfull, ar, &t) : rat_add(ar, g.rhalf, &t);
        if (!ok) bex[0] = bex[1] = false;
        br[1] = t;
    }
    Sol sol[MAXSOL];
    int n = enumerate(&g, bd, bex, br, k, b, sol);
    if (n >= MAXSOL || n > MAXOUT) return false;

    /* ---- working ---- */
    isolate_steps(c, T, a, d, kr, vrat, vx, sv_has_decimal(K));
    Expr *arg = T->kids[0];
    int fi = fop == F_SIN ? F_ASIN : fop == F_COS ? F_ACOS : F_ATAN;
    Expr *k3[3] = { ex_clone(arg), ex_func1(fi, ex_clone(vx)), wexpr(c, &g, bd[0], bex[0], br[0], true) };
    int ops[2] = { R_EQ, R_EQ };
    sv_step(c, "Find the first solution", sv_fmt("Use the inverse %s button on the calculator.", fname),
            ex_print(sv_chain(3, k3, ops)));
    bool two = fabs(bd[1] - bd[0]) > 1e-9;
    if (two) {
        Expr *a0 = wexpr(c, &g, bd[0], bex[0], br[0], true);
        if (bd[0] < 0 && fop != F_TAN) a0->flags |= EF_PAREN;
        Expr *half = g.deg ? ex_int(180) : ex_const(C_PI), *full = g.deg ? ex_int(360) : ex_mul2(ex_int(2), ex_const(C_PI));
        Expr *form = fop == F_SIN ? ex_sub2(half, a0) : fop == F_COS ? ex_sub2(full, a0) : ex_add2(a0, half);
        Expr *k4[3] = { ex_clone(arg), form, wexpr(c, &g, bd[1], bex[1], br[1], true) };
        const char *turn = g.deg ? "180" : "\xCF\x80", *whole = g.deg ? "360" : "2\xCF\x80";
        const char *note = fop == F_SIN ? sv_fmt("The sine graph is symmetrical, so the second solution is %s minus the first.", turn)
                         : fop == F_COS ? sv_fmt("The cosine graph is symmetrical, so the second solution is %s minus the first.", whole)
                                        : sv_fmt("The tangent graph repeats every %s, so add %s.", turn, turn);
        sv_step(c, "Find the second solution", note, ex_print(sv_chain(3, k4, ops)));
    }
    bool plain = rat_is_one(k) && rat_is_zero(b);
    int hit = 0;
    for (int i = 0; i < n; i++)
        if (fabs(sol[i].u - bd[0]) < 1e-9 || fabs(sol[i].u - bd[1]) < 1e-9) hit++;
    if (!plain || hit != n || n != (two ? 2 : 1)) range_step(c, &g, sol, n, k, b, argstr, arg);
    if (!plain) {
        const char *t = !rat_is_zero(b) && !rat_is_one(k) ? "Subtract, then divide" : !rat_is_one(k) ? "Divide both sides" : "Subtract from both sides";
        const char *s = "";
        Expr *xv = ex_var(c->var);
        for (int i = 0; i < n; i++) s = or_join(s, rel_line(xv, wexpr(c, &g, sol[i].x, sol[i].xex, sol[i].xr, false)));
        sv_step(c, t, NULL, s);
    }
    if (n == 0) {
        sv_step(c, "No solutions", "There are no solutions in this range.", NULL);
        *out = ex_text("no solutions in range");
        return true;
    }
    Expr *items[MAXSOL];
    const char *s = "";
    for (int i = 0; i < n; i++) {
        items[i] = ex_rel(R_EQ, ex_var(c->var), wexpr(c, &g, sol[i].x, sol[i].xex, sol[i].xr, false));
        s = or_join(s, ex_print(items[i]));
    }
    sv_step_new(c, g.deg ? "Solutions between 0 and 360" : "Solutions between 0 and 2\xCF\x80", NULL, s);
    *out = n == 1 ? items[0] : ex_node(E_LIST, n, items);
    return true;
}
