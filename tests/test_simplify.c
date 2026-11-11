/* ex_simplify / ex_tidy / ex_subst: canonical form, exact arithmetic, surds, index laws. */
#include <math.h>

#include "testlib.h"
#include "engine/arena.h"
#include "engine/expr.h"

static Expr *P(const char *s)
{
    char err[64];
    Expr *e = ex_parse(s, err, sizeof err, NULL);
    if (!e) printf("parse failed: %s (%s)\n", s, err);
    return e;
}

/* simplify then tidy, printed */
static const char *ST(const char *s)
{
    Expr *e = P(s);
    return e ? ex_print(ex_tidy(ex_simplify(e))) : "(parse)";
}

/* canonical (untidied) form, printed */
static const char *SC(const char *s)
{
    Expr *e = P(s);
    return e ? ex_print(ex_simplify(e)) : "(parse)";
}

#define T(s, w) CHECK_STR(ST(s), w)
#define C(s, w) CHECK_STR(SC(s), w)

/* simplified tree must evaluate to the same value as the original */
static void same_value(const char *s)
{
    EvalEnv env = {.degrees = false, .var_names = {"x", "y", "a", "b"}, .var_values = {1.7, -0.6, 2.3, 0.45}, .nvars = 4};
    Expr *e = P(s);
    double v0 = NAN, v1 = NAN, v2 = NAN;
    bool ok = e && ex_eval(e, &env, &v0);
    Expr *si = e ? ex_simplify(e) : NULL;
    ok = ok && ex_eval(si, &env, &v1) && ex_eval(ex_tidy(si), &env, &v2);
    CHECK(ok && fabs(v0 - v1) <= 1e-9 * fmax(1, fabs(v0)) && fabs(v0 - v2) <= 1e-9 * fmax(1, fabs(v0)),
          "value of %s: %.12g simplified %.12g tidy %.12g", s, v0, v1, v2);
}

/* simplify(simplify(e)) == simplify(e) */
static void idempotent(const char *s)
{
    Expr *e = P(s);
    Expr *a = e ? ex_simplify(e) : NULL, *b = a ? ex_simplify(a) : NULL;
    CHECK(a && b && ex_equal(a, b), "not idempotent: %s -> %s -> %s", s, a ? ex_print(a) : "?", b ? ex_print(b) : "?");
}

static const char *const SAMPLES[] = {
    "y(2x+4)", "2x^2y-8y+3x^2-12", "(x+2)(x-3)(2x+1)", "x^-2", "3/4+5/6", "x/2+x/3",
    "(2x+4)/2", "(4x+6)/4", "x^5/x^2", "(2x)^3", "(xy)^2/x", "x-(x+1)", "3x^2/(6x)",
    "\xE2\x88\x9A" "x\xE2\x88\x9A" "x", "a/b+b/a", "2^x2^y", "-(x+1)y", "x/y/a", "(a+b)/(x-y)",
    "\xE2\x88\x9A" "50x", "e^x e^2", "x^(1/2)x^(1/3)", "0.5x+0.25x", "ln(e^x)", "abs(-3)x",
};

int main(void)
{
    /* GCSE inputs */
    T("y(2x+4)", "y(2x+4)");
    T("2x^2y-8y+3x^2-12", "2x^2y+3x^2-8y-12");
    C("(x+2)(x-3)(2x+1)", "(x-3)(x+2)(2x+1)");
    T("3/4+5/6", "19/12");
    T("2_1/3\xC3\x97" "1_1/2", "7/2");
    T("\xE2\x88\x9A" "50", "5\xE2\x88\x9A" "2");
    T("x^-2", "1/x^2");
    C("x^-2", "x^-2");
    T("5!", "120");
    /* exact numbers */
    T("1/3+1/6", "1/2");
    T("2/4", "1/2");
    T("0.5+0.25", "0.75");
    T("0.1+0.2", "0.3");
    T("1/3+0.5", "5/6");
    T("1.5\xC3\x97" "2", "3");
    T("20%\xC3\x97" "300", "60");
    T("recur(0.(3))", "1/3");
    T("-3+3", "0");
    T("--3", "3");
    T("2^10", "1024");
    T("(1/2)^-3", "8");
    T("(-2)^3", "-8");
    T("(2/3)^2", "4/9");
    T("-2_1/2", "-5/2");
    /* overflow falls back to approximate */
    {
        Expr *e = ex_simplify(P("99999999999^3"));
        CHECK(e && e->kind == E_REAL && fabs(e->v / 9.9999999997e32 - 1) < 1e-12, "99999999999^3 -> REAL");
        e = ex_simplify(P("2^100"));
        CHECK(e && e->kind == E_REAL && e->v == ldexp(1, 100), "2^100 -> REAL");
        e = ex_simplify(P("9223372036854775807+1"));
        CHECK(e && e->kind == E_REAL, "int64 max + 1 -> REAL");
        e = ex_simplify(P("21!"));
        CHECK(e && e->kind == E_REAL && fabs(e->v / 51090942171709440000.0 - 1) < 1e-12, "21! -> REAL");
        e = ex_simplify(P("0.5+0.25"));
        CHECK(e && e->kind == E_NUM && (e->flags & NF_DECIMAL), "decimal flag kept");
        e = ex_simplify(P("1/2+1/4"));
        CHECK(e && e->kind == E_NUM && !(e->flags & NF_DECIMAL), "fraction stays fraction");
    }
    /* like terms, ordering */
    T("2x+3x", "5x");
    T("x+x-2x", "0");
    T("ab+ba", "2ab");
    T("xyx", "x^2y");
    T("x^2+2x+1-x^2", "2x+1");
    T("3+x", "x+3");
    T("y+x", "x+y");
    T("y^2+x^2+xy", "x^2+xy+y^2");
    T("x+x^3+x^2", "x^3+x^2+x");
    T("2\xCF\x80+3\xCF\x80", "5\xCF\x80");
    T("sin(x)^2+sin(x)^2", "2sin(x)^2");
    T("x/2+x/3", "5/6x");
    T("0.5x+0.5x", "x");
    T("x-(x+1)", "-1");
    T("-(-x)", "x");
    T("-x", "-x");
    /* index laws */
    T("x^3x^2", "x^5");
    T("x^5/x^2", "x^3");
    T("x^2x^-2", "1");
    T("(x^2)^3", "x^6");
    T("(2x)^3", "8x^3");
    T("(xy)^2/x", "xy^2");
    T("(x+1)^2(x+1)", "(x+1)^3");
    T("2^x2^y", "2^(x+y)");
    T("x^0", "1");
    T("\xCF\x80^0", "1");
    T("x^1", "x");
    T("1^x", "1");
    T("x\xC3\x97" "0", "0");
    T("x/y/a", "x/(ay)");
    T("(a+b)/(x-y)", "(a+b)/(x-y)");
    T("3/(2x)", "3/(2x)");
    T("3x^2/(6x)", "x/2");
    T("-3x/6", "-x/2");
    /* fraction cancelling into a sum */
    T("(2x+4)/2", "x+2");
    T("(4x+6)/4", "(2x+3)/2");
    T("(2x+3)/2", "(2x+3)/2");
    T("3(x+2)", "3(x+2)");
    /* surds */
    T("\xE2\x88\x9A" "12", "2\xE2\x88\x9A" "3");
    T("\xE2\x88\x9A" "8", "2\xE2\x88\x9A" "2");
    T("\xE2\x88\x9A" "2\xE2\x88\x9A" "3", "\xE2\x88\x9A" "6");
    T("\xE2\x88\x9A" "18/\xE2\x88\x9A" "2", "3");
    T("(\xE2\x88\x9A" "3)^2", "3");
    T("\xE2\x88\x9A" "x\xE2\x88\x9A" "x", "x");
    T("\xE2\x88\x9A" "(2/3)", "\xE2\x88\x9A" "6/3");
    T("1/\xE2\x88\x9A" "2", "\xE2\x88\x9A" "2/2");
    T("2/\xE2\x88\x9A" "8", "\xE2\x88\x9A" "2/2");
    T("\xE2\x88\x9A" "16", "4");
    T("\xE2\x88\x9A" "(9/4)", "3/2");
    T("\xE2\x88\x9A" "(-4)", "\xE2\x88\x9A(-4)");
    T("\xE2\x88\x9A" "(x^2)", "\xE2\x88\x9A(x^2)");
    T("8^(1/3)", "2");
    T("cbrt(54)", "3root(3,2)");
    T("(-8)^(1/3)", "-2");
    T("(-8)^(2/3)", "4");
    T("4^(-1/2)", "1/2");
    T("2^(2/3)2^(1/3)", "2");
    T("2^0.5", "\xE2\x88\x9A" "2");
    T("x^(1/2)", "\xE2\x88\x9A" "x");
    T("x^(-1/2)", "1/\xE2\x88\x9A" "x");
    T("x^(1/3)", "root(3,x)");
    /* functions */
    T("abs(-3/4)", "3/4");
    T("floor(-7/2)", "-4");
    T("ceil(7/2)", "4");
    T("round(5/2)", "3");
    T("round(-5/2)", "-3");
    T("log(10)", "1");
    T("log(2,2)", "1");
    T("ln(e)", "1");
    T("ln(1)", "0");
    T("exp(0)", "1");
    T("ln(e^x)", "x");
    T("nCr(52,5)", "2598960");
    T("nCr(60,30)", "118264581564861424");
    T("nPr(5,2)", "20");
    T("10!", "3628800");
    T("171!", "171!");
    T("sin(x)", "sin(x)");
    /* non-arithmetic nodes pass through */
    T("3x=6", "3x=6");
    T("x=2 \"or\" x=-3", "x=2 \"or\" x=-3");
    T("\xC2\xB1" "3", "\xC2\xB1" "3");
    T("30\xC2\xB0", "30\xC2\xB0");
    T("1/0", "1/0");
    CHECK(ex_simplify(NULL) == NULL && ex_tidy(NULL) == NULL, "NULL in, NULL out");
    /* values unchanged, idempotent */
    for (size_t i = 0; i < sizeof SAMPLES / sizeof *SAMPLES; i++) {
        same_value(SAMPLES[i]);
        idempotent(SAMPLES[i]);
    }
    /* substitution */
    {
        Expr *e = P("x^2+3x"), *s = ex_subst(e, "x", P("2"));
        CHECK_STR(ex_print(ex_simplify(s)), "10");
        CHECK_STR(ex_print(e), "x^2+3x");                 /* original untouched */
        s = ex_subst(P("x+y"), "x", P("y+1"));
        CHECK_STR(ex_print(ex_tidy(ex_simplify(s))), "2y+1");
        s = ex_subst(P("sin(x)+z"), "q", P("5"));
        CHECK_STR(ex_print(s), "sin(x)+z");
    }
    return test_finish();
}
