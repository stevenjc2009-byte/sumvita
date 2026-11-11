/* ex_eval: numeric (double) value of an expression tree.
 * Stand-alone: needs only expr.c and num.c, so the UI can graph and preview
 * without linking the simplifier. Angles follow env->degrees; a value marked
 * with ° is always degrees. */
#include <math.h>
#include <string.h>

#include "expr.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { bool deg; const EvalEnv *env; } Ctx;

static bool ev(const Expr *e, const Ctx *c, double *out);

static bool is_intval(double x) { return fabs(x - round(x)) < 1e-9 && fabs(x) < 9e15; }

/* sin/cos/tan with exact zeros at multiples of 90° / π/2 */
static bool trig(int f, double x, bool deg, double *out)
{
    double r;
    if (deg) {
        double m = fmod(x, 360.0);
        if (m < 0) m += 360.0;
        if (fmod(m, 90.0) == 0.0) {
            int q = (int)(m / 90.0);           /* 0..3 */
            static const double S[4] = {0, 1, 0, -1}, C[4] = {1, 0, -1, 0};
            if (f == F_SIN) r = S[q];
            else if (f == F_COS) r = C[q];
            else if (q == 1 || q == 3) return false;
            else r = 0;
            *out = r;
            return true;
        }
        x = m * M_PI / 180.0;
    }
    double cs = cos(x);
    if (f == F_SIN) r = sin(x);
    else if (f == F_COS) r = cs;
    else {
        if (fabs(cs) < 1e-15) return false;
        r = tan(x);
    }
    if (fabs(r) < 1e-14 * fmax(1.0, fabs(x)) && fabs(x) > 1e-6) r = 0;
    *out = r;
    return true;
}

static bool combi(int f, double n, double r, double *out)
{
    if (!is_intval(n) || !is_intval(r)) return false;
    n = round(n);
    r = round(r);
    if (n < 0 || r < 0 || r > n) return false;
    double v = 1;
    if (f == F_NCR) {
        if (r > n - r) r = n - r;
        for (double i = 1; i <= r; i++) v = v * (n - r + i) / i;
        if (v < 9e15) v = round(v);
    } else {
        for (double i = 0; i < r; i++) v *= n - i;
    }
    *out = v;
    return true;
}

static bool func(const Expr *e, const Ctx *c, double *out)
{
    double a[2] = {0, 0};
    if (e->nkids < 1 || e->nkids > 2) return false;
    for (int i = 0; i < e->nkids; i++)
        if (!ev(e->kids[i], c, &a[i])) return false;
    double x = a[0], r;
    double toang = c->deg ? 180.0 / M_PI : 1.0;
    switch (e->op) {
    case F_SIN: case F_COS: case F_TAN: return trig(e->op, x, c->deg, out);
    case F_ASIN: if (x < -1 || x > 1) return false; r = asin(x) * toang; break;
    case F_ACOS: if (x < -1 || x > 1) return false; r = acos(x) * toang; break;
    case F_ATAN: r = atan(x) * toang; break;
    case F_SINH: r = sinh(x); break;
    case F_COSH: r = cosh(x); break;
    case F_TANH: r = tanh(x); break;
    case F_LN: if (x <= 0) return false; r = log(x); break;
    case F_LOG:
        if (e->nkids == 1) { if (x <= 0) return false; r = log10(x); break; }
        if (x <= 0 || x == 1 || a[1] <= 0) return false;
        r = log(a[1]) / log(x);
        break;
    case F_SQRT: if (x < 0) return false; r = sqrt(x); break;
    case F_ROOT: {
        if (e->nkids != 2 || !is_intval(x) || round(x) < 1) return false;
        double n = round(x), v = a[1];
        if (v < 0) {
            if (fmod(n, 2.0) == 0) return false;
            r = -pow(-v, 1.0 / n);
        } else {
            r = pow(v, 1.0 / n);
        }
        if (fabs(r - round(r)) < 1e-12 * fmax(1.0, fabs(r))) r = round(r);
        break;
    }
    case F_ABS: r = fabs(x); break;
    case F_FLOOR: r = floor(x); break;
    case F_CEIL: r = ceil(x); break;
    case F_ROUND: r = round(x); break;
    case F_NCR: case F_NPR:
        if (e->nkids != 2 || !combi(e->op, x, a[1], &r)) return false;
        break;
    case F_EXP: r = exp(x); break;
    default: return false;
    }
    *out = r;
    return true;
}

static bool power(const Expr *e, const Ctx *c, double *out)
{
    double b, x;
    if (!ev(e->kids[0], c, &b) || !ev(e->kids[1], c, &x)) return false;
    if (b < 0 && !is_intval(x)) {
        Rat q;          /* odd-denominator exponent: real root of a negative */
        if (!rat_from_double(x, 1000, &q) || q.d % 2 == 0) return false;
        double r = pow(-b, x);
        *out = (q.n % 2 != 0) ? -r : r;
        return true;
    }
    *out = pow(b, x);
    return true;
}

static bool ev(const Expr *e, const Ctx *c, double *out)
{
    double a, b;
    switch (e->kind) {
    case E_NUM: case E_RECUR: *out = rat_to_double(e->r); return true;
    case E_REAL: *out = e->v; return true;
    case E_CONST: *out = e->op == C_PI ? M_PI : M_E; return true;
    case E_VAR:
        if (c->env)
            for (int i = 0; i < c->env->nvars && i < 8; i++)
                if (c->env->var_names[i] && strcmp(c->env->var_names[i], e->name) == 0) {
                    *out = c->env->var_values[i];
                    return true;
                }
        return false;
    case E_ADD:
        a = 0;
        for (int i = 0; i < e->nkids; i++) {
            if (!ev(e->kids[i], c, &b)) return false;
            a += b;
        }
        *out = a;
        return true;
    case E_MUL:
        a = 1;
        for (int i = 0; i < e->nkids; i++) {
            if (!ev(e->kids[i], c, &b)) return false;
            a *= b;
        }
        *out = a;
        return true;
    case E_DIV:
        if (!ev(e->kids[0], c, &a) || !ev(e->kids[1], c, &b) || b == 0) return false;
        *out = a / b;
        return true;
    case E_NEG:
        if (!ev(e->kids[0], c, &a)) return false;
        *out = -a;
        return true;
    case E_POW: return power(e, c, out);
    case E_FUNC: return func(e, c, out);
    case E_PCT:
        if (!ev(e->kids[0], c, &a)) return false;
        *out = a / 100.0;
        return true;
    case E_DEG:
        if (!ev(e->kids[0], c, &a)) return false;
        *out = c->deg ? a : a * M_PI / 180.0;
        return true;
    case E_FACT:
        if (!ev(e->kids[0], c, &a) || !is_intval(a) || a < 0 || a > 170) return false;
        b = 1;
        for (int i = 2; i <= (int)round(a); i++) b *= i;
        *out = b;
        return true;
    case E_MIXED:
        if (e->nkids != 3 || !ev(e->kids[0], c, &a)) return false;
        {
            double n, d;
            if (!ev(e->kids[1], c, &n) || !ev(e->kids[2], c, &d) || d == 0) return false;
            *out = a < 0 ? a - n / d : a + n / d;
        }
        return true;
    default:
        return false;      /* relations, lists, text, ± have no single value */
    }
}

bool ex_eval(const Expr *e, const EvalEnv *env, double *out)
{
    double v = NAN;
    Ctx c = {env ? env->degrees : false, env};
    bool ok = e && ev(e, &c, &v) && isfinite(v);
    *out = ok ? v : NAN;
    return ok;
}
