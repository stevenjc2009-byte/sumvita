/* Numeric root finding on doubles: polynomials (roots bracketed between the
 * turning points, found recursively from the derivative) and general one-variable
 * expressions (scan + bisection, plus touching roots via a minimum search). */
#include "solve_ext.h"
#include <math.h>

static double peval(const double *a, int d, double x)
{
    double y = 0;
    for (int i = d; i >= 0; i--) y = y * x + a[i];
    return y;
}

static double pscale(const double *a, int d, double x)
{
    double s = 0, p = 1, ax = fabs(x);
    for (int i = 0; i <= d; i++) { s += fabs(a[i]) * p; p *= ax; }
    return s;
}

static double bisect_poly(const double *a, int d, double lo, double hi)
{
    double flo = peval(a, d, lo);
    for (int i = 0; i < 200; i++) {
        double mid = 0.5 * (lo + hi);
        if (mid <= lo || mid >= hi) break;
        double fm = peval(a, d, mid);
        if (fm == 0) return mid;
        if ((fm < 0) == (flo < 0)) { lo = mid; flo = fm; }
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

static int add_root(double *out, int n, int max, double x)
{
    for (int i = 0; i < n; i++)
        if (fabs(out[i] - x) <= 1e-9 * (1 + fabs(x))) return n;
    if (n < max) out[n++] = x;
    return n;
}

static void sort_d(double *v, int n)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && v[j - 1] > v[j]; j--) { double t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
}

int sv_upoly_real_roots(const double *a, int deg, double *out, int max)
{
    while (deg > 0 && a[deg] == 0) deg--;
    if (deg <= 0 || max <= 0) return 0;
    if (deg == 1) { out[0] = -a[0] / a[1]; return 1; }
    double da[SV_MAXDEG + 1], crit[SV_MAXDEG + 1], pts[SV_MAXDEG + 3];
    for (int i = 1; i <= deg; i++) da[i - 1] = i * a[i];
    int nc = sv_upoly_real_roots(da, deg - 1, crit, SV_MAXDEG);
    sort_d(crit, nc);
    double B = 0;
    for (int i = 0; i < deg; i++) B = fmax(B, fabs(a[i] / a[deg]));
    B += 1;
    int np = 0;
    pts[np++] = -B;
    for (int i = 0; i < nc; i++)
        if (crit[i] > -B && crit[i] < B) pts[np++] = crit[i];
    pts[np++] = B;
    int n = 0;
    for (int i = 0; i < np; i++) {
        double f = peval(a, deg, pts[i]);
        if (fabs(f) <= 1e-12 * pscale(a, deg, pts[i])) n = add_root(out, n, max, pts[i]);
        if (i + 1 < np) {
            double g = peval(a, deg, pts[i + 1]);
            if ((f < 0 && g > 0) || (f > 0 && g < 0)) n = add_root(out, n, max, bisect_poly(a, deg, pts[i], pts[i + 1]));
        }
    }
    sort_d(out, n);
    return n;
}

/* ---- general expressions ---- */

typedef struct { const Expr *f; EvalEnv env; } Fn;

static bool fe(Fn *F, double x, double *y)
{
    F->env.var_values[0] = x;
    return ex_eval(F->f, &F->env, y) && isfinite(*y);
}

#define N1 1500
#define N2 8000
#define N3 1500

static double grid(int i)
{
    if (i < N1) return -1e6 * pow(1e-4, (double)i / N1);
    if (i <= N1 + N2) return -100 + 200.0 * (i - N1) / N2;
    return 100 * pow(1e4, (double)(i - N1 - N2) / N3);
}

static bool bisect_fn(Fn *F, double lo, double hi, double flo, double *root)
{
    for (int i = 0; i < 200; i++) {
        double mid = 0.5 * (lo + hi), fm;
        if (mid <= lo || mid >= hi) break;
        if (!fe(F, mid, &fm)) return false;
        if (fm == 0) { lo = hi = mid; break; }
        if ((fm < 0) == (flo < 0)) { lo = mid; flo = fm; }
        else hi = mid;
    }
    *root = 0.5 * (lo + hi);
    return true;
}

/* minimum of |f| on [a, b] by golden-section search */
static bool touch(Fn *F, double a, double b, double *root)
{
    const double g = 0.6180339887498949;
    double c = b - g * (b - a), d = a + g * (b - a), fc, fd;
    for (int i = 0; i < 120; i++) {
        if (!fe(F, c, &fc) || !fe(F, d, &fd)) return false;
        if (fabs(fc) < fabs(fd)) b = d; else a = c;
        c = b - g * (b - a);
        d = a + g * (b - a);
    }
    *root = 0.5 * (a + b);
    return fe(F, *root, &fc) && fabs(fc) < 1e-10;
}

/* near-equal roots (relative 1e-6) are one root: keep the one that fits f better */
static int add_rootf(Fn *F, double *out, int n, int max, double x)
{
    for (int i = 0; i < n; i++) {
        if (fabs(out[i] - x) > 1e-6 * fmax(1.0, fabs(x))) continue;
        double a, b;
        if (fe(F, x, &a) && fe(F, out[i], &b) && fabs(a) < fabs(b)) out[i] = x;
        return n;
    }
    if (n < max) out[n++] = x;
    return n;
}

/* lo < hi: uniform scan of [lo, hi]; otherwise the wide scan of the whole real line */
static int scan(SvCtx *c, const Expr *f, double lo, double hi, double *out, int max)
{
    Fn F = { f, { 0 } };
    F.env.degrees = c->s ? c->s->degrees : true;
    F.env.var_names[0] = c->var;
    F.env.nvars = 1;
    bool rng = hi > lo;
    int n = 0, total = rng ? N2 : N1 + N2 + N3;
#define GRID(i) (rng ? lo + (hi - lo) * (i) / total : grid(i))
    double x0 = GRID(0), y0 = 0, xp = 0, yp = 0;
    bool v0 = fe(&F, x0, &y0), vp = false;
    for (int i = 1; i <= total && n < max; i++) {
        double x1 = GRID(i), y1, r;
        bool v1 = fe(&F, x1, &y1);
        if (v0 && y0 == 0) n = add_rootf(&F, out, n, max, x0);
        if (v0 && v1 && ((y0 < 0 && y1 > 0) || (y0 > 0 && y1 < 0)) && bisect_fn(&F, x0, x1, y0, &r)) {
            double fr;
            if (fe(&F, r, &fr) && fabs(fr) < 1e-6) n = add_rootf(&F, out, n, max, r);
        }
        if (vp && v0 && v1 && fabs(y0) < fabs(yp) && fabs(y0) <= fabs(y1) && fabs(y0) < 1e-3 &&
            (yp > 0) == (y0 > 0) && (y1 > 0) == (y0 > 0) && touch(&F, xp, x1, &r))
            n = add_rootf(&F, out, n, max, r);
        xp = x0; yp = y0; vp = v0;
        x0 = x1; y0 = y1; v0 = v1;
    }
#undef GRID
    if (v0 && y0 == 0) n = add_rootf(&F, out, n, max, x0);
    sort_d(out, n);
    return n;
}

int sv_expr_roots(SvCtx *c, const Expr *f, double *out, int max)
{
    return scan(c, f, 0, 0, out, max);
}

int sv_expr_roots_range(SvCtx *c, const Expr *f, double lo, double hi, double *out, int max)
{
    return scan(c, f, lo, hi, out, max);
}
