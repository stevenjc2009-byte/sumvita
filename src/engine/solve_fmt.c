/* Decimal text for numeric answers (same rounding as calc answers), the
 * "a*T + d" decomposition used by the trig and surd solvers, trial and improvement. */
#include "solve_ext.h"
#include "calc_util.h"
#include <math.h>
#include <string.h>

const char *sv_numdec(SvCtx *c, double v)
{
    return cu_fmt_dec(v, c ? c->s : NULL, NULL);
}

Expr *sv_numexpr(SvCtx *c, double v)
{
    char err[64];
    int pos;
    Expr *e = ex_parse(sv_numdec(c, v), err, sizeof err, &pos);
    return e ? e : sv_real(v);
}

const char *sv_or_list_num(SvCtx *c, const SvRoots *R)
{
    const char *s = "";
    for (int i = 0; i < R->n; i++) {
        const char *one = sv_fmt("%s=%s", c->var, sv_numdec(c, sv_root_val(&R->r[i])));
        s = i == 0 ? one : sv_fmt("%s \"or\" %s", s, one);
    }
    return s;
}

bool sv_exact_num(const Expr *e, Rat *r)
{
    Expr *s = ex_simplify(e);
    if (!s || s->kind != E_NUM) return false;
    *r = s->r;
    return true;
}

static bool is_trig(const Expr *e, int fop)
{
    if (e->kind != E_FUNC || e->nkids != 1) return false;
    if (fop) return e->op == fop;
    return e->op == F_SIN || e->op == F_COS || e->op == F_TAN;
}

bool sv_lindec(SvCtx *c, const Expr *e, int fop, Rat *a, Rat *d, const Expr **T)
{
    if (!ex_has_var(e, c->var)) {
        *a = rat_int(0);
        return sv_exact_num(e, d);
    }
    Rat a1, d1;
    switch (e->kind) {
    case E_FUNC:
        if (!is_trig(e, fop)) return false;
        *a = rat_int(1); *d = rat_int(0); *T = e;
        return true;
    case E_NEG:
        if (!sv_lindec(c, e->kids[0], fop, &a1, &d1, T)) return false;
        *a = rat_neg(a1); *d = rat_neg(d1);
        return true;
    case E_ADD: {
        const Expr *t = NULL;
        Rat sa = rat_int(0), sd = rat_int(0);
        for (int i = 0; i < e->nkids; i++) {
            bool hv = ex_has_var(e->kids[i], c->var);
            if (hv && t) return false;
            const Expr *t1 = NULL;
            if (!sv_lindec(c, e->kids[i], fop, &a1, &d1, &t1)) return false;
            if (hv) { t = t1; sa = a1; }
            if (!rat_add(sd, d1, &sd)) return false;
        }
        *a = sa; *d = sd; *T = t;
        return t != NULL;
    }
    case E_MUL: {
        Rat k = rat_int(1);
        const Expr *t = NULL;
        Rat va = rat_int(0), vd = rat_int(0);
        for (int i = 0; i < e->nkids; i++) {
            if (ex_has_var(e->kids[i], c->var)) {
                if (t || !sv_lindec(c, e->kids[i], fop, &va, &vd, &t)) return false;
            } else {
                Rat n;
                if (!sv_exact_num(e->kids[i], &n) || !rat_mul(k, n, &k)) return false;
            }
        }
        if (!t || !rat_mul(k, va, a) || !rat_mul(k, vd, d)) return false;
        *T = t;
        return true;
    }
    case E_DIV: {
        Rat n;
        if (!sv_exact_num(e->kids[1], &n) || rat_is_zero(n)) return false;
        if (!sv_lindec(c, e->kids[0], fop, &a1, &d1, T)) return false;
        return rat_div(a1, n, a) && rat_div(d1, n, d);
    }
    default: return false;
    }
}

static double polyv(const double *a, int deg, double x)
{
    double y = 0;
    for (int i = deg; i >= 0; i--) y = y * x + a[i];
    return y;
}

/* one line per decimal place: the two values of x either side of the sign change */
void sv_trial_steps(SvCtx *c, const double *a, int deg, double root)
{
    double step = 1, lo = floor(root);
    static const char *const place[] = { "whole numbers", "1 decimal place", "2 decimal places", "3 decimal places" };
    for (int p = 0; p < 4; p++) {
        double hi = lo + step, flo = polyv(a, deg, lo), fhi = polyv(a, deg, hi);
        if (!((flo <= 0 && fhi >= 0) || (flo >= 0 && fhi <= 0))) return;
        const char *fmt = p == 0 ? "%.0f" : p == 1 ? "%.1f" : p == 2 ? "%.2f" : "%.3f";
        const char *xl = sv_fmt(fmt, lo), *xh = sv_fmt(fmt, hi);
        sv_step(c, sv_fmt("Trial and improvement: %s", place[p]),
                sv_fmt("With the equation written as (something) = 0: %s=%s gives %.4g and %s=%s gives %.4g. "
                       "The sign changes, so the answer is between %s and %s.",
                       c->var, xl, flo, c->var, xh, fhi, xl, xh),
                sv_fmt("%s<%s<%s", xl, c->var, xh));
        step /= 10;
        double best = lo;
        for (int i = 0; i < 10; i++) {
            double x0 = lo + i * step, f0 = polyv(a, deg, x0), f1 = polyv(a, deg, x0 + step);
            if ((f0 <= 0 && f1 >= 0) || (f0 >= 0 && f1 <= 0)) { best = x0; break; }
        }
        lo = floor(best / step + 0.5) * step;
    }
}
