#include "tools_util.h"

#include <stdarg.h>
#include <stdlib.h>

/* ---------------- result helpers ---------------- */

void tu_begin(CalcResult *o)
{
    steps_init(&o->steps);
    o->ok = false;
    o->input_echo = "";
    o->answer = "";
    o->answer_alt = "";
    o->value = 0;
    o->has_value = false;
    o->error[0] = 0;
    o->error_pos = -1;
}

void tu_fail(CalcResult *o, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(o->error, sizeof o->error, fmt, ap);
    va_end(ap);
    o->ok = false;
    o->answer = "";
    o->answer_alt = "";
    o->has_value = false;
}

void tu_ans(CalcResult *o, const char *answer, const char *alt)
{
    o->ok = true;
    o->answer = answer ? answer : "";
    o->answer_alt = alt ? alt : "";
}

void tu_val(CalcResult *o, double x)
{
    o->value = x;
    o->has_value = isfinite(x);
}

void tu_text(CalcResult *o, const char *title, const char *note)
{
    steps_add(&o->steps, title, note, "%s", "");
}

void tu_pair(CalcResult *o, const CalcSettings *s, const char *decimal, const char *exact)
{
    if (exact && s && s->prefer_exact) tu_ans(o, exact, decimal);
    else tu_ans(o, decimal, exact ? exact : "");
}

/* ---------------- parsing ---------------- */

bool tu_blank(const char *txt)
{
    if (!txt) return true;
    while (*txt == ' ' || *txt == '\t') txt++;
    return *txt == 0;
}

Expr *tu_expr(CalcResult *o, const char *label, const char *txt)
{
    if (tu_blank(txt)) { tu_fail(o, "%s is empty", label); return NULL; }
    char err[64];
    int pos = -1;
    Expr *e = ex_parse(txt, err, sizeof err, &pos);
    if (!e) { tu_fail(o, "%s: %s", label, err); return NULL; }
    return e;
}

bool tu_eval_x(const Expr *e, const char *var, double x, double *out)
{
    EvalEnv env;
    memset(&env, 0, sizeof env);
    env.degrees = true;
    env.nvars = 1;
    env.var_names[0] = var;
    env.var_values[0] = x;
    return ex_eval(e, &env, out) && isfinite(*out);
}

bool tu_num(CalcResult *o, const char *label, const char *txt, double *x)
{
    Expr *e = tu_expr(o, label, txt);
    if (!e) return false;
    EvalEnv env;
    memset(&env, 0, sizeof env);
    env.degrees = true;
    if (!ex_eval(e, &env, x) || !isfinite(*x)) { tu_fail(o, "%s: not a number", label); return false; }
    return true;
}

bool tu_rat(CalcResult *o, const char *label, const char *txt, Rat *r)
{
    double x;
    if (!tu_num(o, label, txt, &x)) return false;
    if (!rat_from_double(x, 1000000, r)) { tu_fail(o, "%s: use a fraction or decimal", label); return false; }
    return true;
}

bool tu_int(CalcResult *o, const char *label, const char *txt, int64_t *x)
{
    double d;
    if (!tu_num(o, label, txt, &d)) return false;
    if (fabs(d - round(d)) > 1e-9 || fabs(d) > 4e15) { tu_fail(o, "%s must be a whole number", label); return false; }
    *x = (int64_t)llround(d);
    return true;
}

bool tu_list(CalcResult *o, const char *label, const char *txt, double *a, int max, int *n)
{
    if (tu_blank(txt)) { tu_fail(o, "%s is empty", label); return false; }
    char *buf = astrdup(txt);
    int k = 0;
    char *p = buf;
    for (;;) {
        char *q = p;
        while (*q && *q != ',' && *q != ';') q++;
        char save = *q;
        *q = 0;
        if (!tu_blank(p)) {
            if (k >= max) { tu_fail(o, "%s: too many values (max %d)", label, max); return false; }
            if (!tu_num(o, label, p, &a[k])) return false;
            k++;
        }
        if (!save) break;
        p = q + 1;
    }
    if (k == 0) { tu_fail(o, "%s is empty", label); return false; }
    *n = k;
    return true;
}

bool tu_ratlist(CalcResult *o, const char *label, const char *txt, Rat *a, int max, int *n)
{
    double d[TU_MAXLIST];
    int k;
    if (max > TU_MAXLIST) max = TU_MAXLIST;
    if (!tu_list(o, label, txt, d, max, &k)) return false;
    for (int i = 0; i < k; i++)
        if (!rat_from_double(d[i], 1000000, &a[i])) { tu_fail(o, "%s: use fractions or decimals", label); return false; }
    *n = k;
    return true;
}

int tu_choice(const char *choices, const char *val)
{
    if (!choices || !val) return 0;
    size_t vl = strlen(val);
    int idx = 0;
    const char *p = choices;
    for (;;) {
        const char *q = strchr(p, '|');
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len == vl && strncmp(p, val, vl) == 0) return idx;
        if (!q) break;
        p = q + 1;
        idx++;
    }
    return 0;
}

/* ---------------- formatting ---------------- */

static void trim0(char *b)
{
    if (strchr(b, '.')) {
        char *e = b + strlen(b);
        while (e > b && e[-1] == '0') e--;
        if (e > b && e[-1] == '.') e--;
        *e = 0;
    }
    if (strcmp(b, "-0") == 0) strcpy(b, "0");
}

static double round_dp_d(double x, int dp)
{
    if (dp > 12) return x;
    double p = pow(10.0, dp);
    return round(x * p + copysign(1e-9, x)) / p;
}

const char *tu_dp(double x, int dp)
{
    char b[96];
    if (!isfinite(x)) return astrdup("0");
    if (dp < 0) dp = 0;
    if (dp > 12) dp = 12;
    snprintf(b, sizeof b, "%.*f", dp, round_dp_d(x, dp));
    /* "-0.00" -> "0.00" */
    if (b[0] == '-') {
        bool zero = true;
        for (char *c = b + 1; *c; c++) if (*c != '0' && *c != '.') zero = false;
        if (zero) memmove(b, b + 1, strlen(b));
    }
    return astrdup(b);
}

double tu_rsf(double x, int sf)
{
    if (x == 0 || !isfinite(x)) return x;
    int mag = (int)floor(log10(fabs(x)));
    int dec = sf - 1 - mag;
    if (dec >= 0) return round_dp_d(x, dec);
    double p = pow(10.0, -dec);
    return round(x / p) * p;
}

const char *tu_sf(double x, int sf)
{
    char b[96];
    if (x == 0 || !isfinite(x)) return astrdup("0");
    if (sf < 1) sf = 1;
    double r = tu_rsf(x, sf);
    int mag = (int)floor(log10(fabs(r)));
    int dec = sf - 1 - mag;
    if (dec < 0) dec = 0;
    if (dec > 12) dec = 12;
    snprintf(b, sizeof b, "%.*f", dec, r);
    trim0(b);
    return astrdup(b);
}

const char *tu_w(double x) { return tu_sf(x, 6); }

const char *tu_f(double x, const CalcSettings *s, int def_sf, int def_dp)
{
    if (s && s->dp >= 0) return tu_dp(x, s->dp);
    if (s && s->sf > 0) return tu_sf(x, s->sf);
    if (def_dp >= 0) return tu_dp(x, def_dp);
    return tu_sf(x, def_sf);
}

const char *tu_rs(Rat r)
{
    char b[64];
    rat_to_str(r, b, sizeof b);
    return astrdup(b);
}

const char *tu_u(const char *num, const char *unit)
{
    if (!unit || !*unit) return num;
    return asprintf_a("%s \"%s\"", num, unit);
}

const char *tu_unit_pow(const char *unit, int pow)
{
    if (pow == 2) return asprintf_a("%s\xC2\xB2", unit);
    if (pow == 3) return asprintf_a("%s\xC2\xB3", unit);
    return unit;
}

static const char *coef_str(double a)   /* a >= 0 */
{
    if (fabs(a - round(a)) < 1e-9) return asprintf_a("%lld", (long long)llround(a));
    Rat r;
    if (rat_from_double(a, 100000, &r)) return asprintf_a("(%lld/%lld)", (long long)r.n, (long long)r.d);
    return tu_w(a);
}

const char *tu_pi(double coef)
{
    if (!isfinite(coef) || coef == 0) return NULL;
    double a = fabs(coef);
    const char *sg = coef < 0 ? "-" : "";
    if (fabs(a - round(a)) < 1e-9) {
        long long k = llround(a);
        if (k == 1) return asprintf_a("%s\xCF\x80", sg);
        return asprintf_a("%s%lld\xCF\x80", sg, k);
    }
    Rat r;
    if (rat_from_double(a, 100000, &r)) return asprintf_a("%s(%lld/%lld)\xCF\x80", sg, (long long)r.n, (long long)r.d);
    return NULL;
}

const char *tu_poly(const double *c, int deg, const char *var)
{
    char out[256];
    out[0] = 0;
    size_t len = 0;
    for (int i = 0; i <= deg; i++) {
        double a = c[i];
        if (fabs(a) < 1e-12) continue;
        int p = deg - i;
        const char *sign = a < 0 ? "-" : (len ? "+" : "");
        double m = fabs(a);
        const char *cs = (p > 0 && fabs(m - 1) < 1e-12) ? "" : coef_str(m);
        const char *term;
        if (p == 0) term = asprintf_a("%s%s", sign, cs);
        else if (p == 1) term = asprintf_a("%s%s%s", sign, cs, var);
        else term = asprintf_a("%s%s%s^%d", sign, cs, var, p);
        len += (size_t)snprintf(out + len, sizeof out - len, "%s", term);
        if (len >= sizeof out - 1) break;
    }
    if (!len) return "0";
    return astrdup(out);
}

const char *tu_ratio_str(const int64_t *p, int n)
{
    char out[160];
    size_t len = 0;
    out[0] = 0;
    for (int i = 0; i < n && len < sizeof out - 24; i++)
        len += (size_t)snprintf(out + len, sizeof out - len, "%s%lld", i ? ":" : "", (long long)p[i]);
    return astrdup(out);
}
