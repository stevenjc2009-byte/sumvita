/* cu_prepare: Ans -> its value, and exact trig.
 * sin/cos/tan of multiples of 30° and 45° (any quadrant, any size) become exact
 * values (sin 150 = 1/2, tan 60 = √3); tan 90 is a maths error. Inverse trig of
 * those exact values gives the exact angle (sin⁻¹(1/2) = 30). In radians mode the
 * same works for rational multiples of π (sin(π/6) = 1/2, asin(1/2) = π/6). */
#include <math.h>
#include <string.h>

#include "arena.h"
#include "calc_util.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Exact values for reference angles 0, 30, 45, 60, 90 (index 0..4). */
static Expr *sqrt_over(int64_t b, int64_t d)
{
    Expr *s = ex_func1(F_SQRT, ex_int(b));
    return d == 1 ? s : ex_div2(s, ex_int(d));
}

static Expr *sin_ref(int i)
{
    switch (i) {
    case 0: return ex_int(0);
    case 1: return ex_num(RAT(1, 2));
    case 2: return sqrt_over(2, 2);
    case 3: return sqrt_over(3, 2);
    default: return ex_int(1);
    }
}

static Expr *tan_ref(int i)
{
    switch (i) {
    case 0: return ex_int(0);
    case 1: return sqrt_over(3, 3);
    case 2: return ex_int(1);
    default: return sqrt_over(3, 1);
    }
}

static int ref_index(int64_t r)
{
    switch (r) {
    case 0: return 0;
    case 30: return 1;
    case 45: return 2;
    case 60: return 3;
    default: return 4;
    }
}

static int sgn(double x) { return x > 1e-9 ? 1 : (x < -1e-9 ? -1 : 0); }

/* Angle in degrees as an exact rational, if the argument gives one. */
static bool exact_degrees(const Expr *arg, bool degrees, Rat *deg)
{
    if (arg->kind == E_DEG) {
        Rat r;
        if (!cu_rational(arg->kids[0], &r)) return false;
        *deg = r;
        return true;
    }
    if (degrees) return cu_rational(arg, deg);
    Expr *s = ex_simplify(arg);
    Rat k;
    if (s->kind == E_NUM && rat_is_zero(s->r)) k = rat_int(0);
    else if (s->kind == E_CONST && s->op == C_PI) k = rat_int(1);
    else if (s->kind == E_MUL && s->nkids == 2 && s->kids[0]->kind == E_NUM && s->kids[1]->kind == E_CONST &&
             s->kids[1]->op == C_PI) k = s->kids[0]->r;
    else return false;
    return rat_mul(k, rat_int(180), deg);
}

static const char *QUAD[4] = {"1st", "2nd", "3rd", "4th"};

static Expr *exact_trig(int f, Rat deg, const Expr *orig, Steps *st, CalcResult *out, bool *err)
{
    if (deg.d != 1) return NULL;
    int64_t a = deg.n;
    if (a % 30 != 0 && a % 45 != 0) return NULL;
    int64_t m = a % 360;
    if (m < 0) m += 360;
    double rad = (double)m * M_PI / 180.0;
    int ss = sgn(sin(rad)), cs = sgn(cos(rad));
    const char *name = f == F_SIN ? "sin" : (f == F_COS ? "cos" : "tan");
    if (f == F_TAN && cs == 0) {
        cu_fail(out, "Maths error: tan %lld\xC2\xB0 is undefined", (long long)a);
        *err = true;
        return NULL;
    }
    int64_t r = m <= 90 ? m : (m <= 180 ? 180 - m : (m <= 270 ? m - 180 : 360 - m));
    int ri = ref_index(r);
    Expr *v;
    int sign;
    if (f == F_SIN) { v = sin_ref(ri); sign = ss; }
    else if (f == F_COS) { v = sin_ref(4 - ri); sign = cs; }
    else { v = tan_ref(ri); sign = ss * cs; }
    if (sign < 0) v = ex_neg(v);
    const char *vs = ex_print(v);
    v->flags |= EF_PAREN;
    const char *lhs = ex_print(orig);
    if (r != m || m != a) {
        int q = m < 90 ? 0 : (m < 180 ? 1 : (m < 270 ? 2 : 3));
        bool on_axis = m % 90 == 0;
        const char *note = on_axis ? asprintf_a("%lld\xC2\xB0 is on an axis", (long long)m)
                                   : asprintf_a("%lld\xC2\xB0 is in the %s quadrant, where %s is %s", (long long)m,
                                                QUAD[q], name, sign > 0 ? "positive" : "negative");
        if (!on_axis)
            steps_add(st, "Exact trig value", note, "%s=%s%s(%lld)=%s", lhs, sign < 0 ? "-" : "", name,
                      (long long)r, vs);
        else
            steps_add(st, "Exact trig value", note, "%s=%s", lhs, vs);
    } else {
        steps_add(st, "Exact trig value", "", "%s=%s", lhs, vs);
    }
    return v;
}

/* Inverse trig of an exact special value. */
static Expr *exact_inverse(int f, const Expr *arg, bool degrees, const Expr *orig, Steps *st)
{
    CVal cv;
    if (!cu_value(arg, degrees, &cv) || !cv.exact) return NULL;
    double x = cv.v;
    static const double SINV[9] = {-1, -0.86602540378443865, -0.70710678118654752, -0.5, 0,
                                   0.5, 0.70710678118654752, 0.86602540378443865, 1};
    static const int SDEG[9] = {-90, -60, -45, -30, 0, 30, 45, 60, 90};
    static const double TINV[7] = {-1.7320508075688773, -1, -0.57735026918962576, 0,
                                   0.57735026918962576, 1, 1.7320508075688773};
    static const int TDEG[7] = {-60, -45, -30, 0, 30, 45, 60};
    int deg = 0;
    bool found = false;
    if (f == F_ATAN) {
        for (int i = 0; i < 7 && !found; i++)
            if (fabs(x - TINV[i]) < 1e-12) { deg = TDEG[i]; found = true; }
    } else {
        for (int i = 0; i < 9 && !found; i++)
            if (fabs(x - SINV[i]) < 1e-12) { deg = f == F_ASIN ? SDEG[i] : 90 - SDEG[i]; found = true; }
    }
    if (!found) return NULL;
    Expr *v;
    if (degrees) {
        v = ex_int(deg);
    } else {
        Rat k = RAT(deg, 180);
        v = k.n == 0 ? ex_int(0) : cu_pretty(ex_tidy(ex_simplify(ex_mul2(ex_num(k), ex_const(C_PI)))));
    }
    steps_add(st, "Exact angle", degrees ? "Angles in degrees" : "Angle in radians", "%s=%s", ex_print(orig),
              ex_print(v));
    v->flags |= EF_PAREN;
    return v;
}

typedef struct { const CalcSettings *s; CalcResult *out; bool err; int depth; } Prep;

static Expr *prep(const Expr *e, Prep *p)
{
    if (p->err) return NULL;
    if (++p->depth > 400) { cu_fail(p->out, "Too complicated"); p->err = true; return NULL; }
    Expr *c = amalloc(sizeof(Expr));
    *c = *e;
    if (e->nkids > 0) {
        c->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
        for (int i = 0; i < e->nkids; i++)
            if (!(c->kids[i] = prep(e->kids[i], p))) return NULL;
    }
    p->depth--;
    if (c->kind == E_VAR && strcmp(c->name, "Ans") == 0) {
        Rat r;
        double a = p->s->ans;
        Expr *v = (isfinite(a) && rat_from_double(a, 1000000, &r)) ? ex_num(r) : ex_real(isfinite(a) ? a : 0);
        if (!p->s->ans_set) v = ex_int(0);
        v->flags |= EF_PAREN;
        return v;
    }
    if (c->kind != E_FUNC || c->nkids != 1) return c;
    if (ex_has_var(c->kids[0], NULL)) return c;
    Expr *v = NULL;
    if (c->op == F_SIN || c->op == F_COS || c->op == F_TAN) {
        Rat deg;
        EvalEnv env = {.degrees = p->s->degrees};
        double tmp;
        if (!ex_eval(c->kids[0], &env, &tmp)) return c;
        if (exact_degrees(c->kids[0], p->s->degrees, &deg))
            v = exact_trig(c->op, deg, c, &p->out->steps, p->out, &p->err);
        if (p->err) return NULL;
    } else if (c->op == F_ASIN || c->op == F_ACOS || c->op == F_ATAN) {
        v = exact_inverse(c->op, c->kids[0], p->s->degrees, c, &p->out->steps);
    }
    return v ? v : c;
}

Expr *cu_prepare(const Expr *e, const CalcSettings *s, CalcResult *out)
{
    Prep p = {s, out, false, 0};
    Expr *r = prep(e, &p);
    return p.err ? NULL : r;
}
