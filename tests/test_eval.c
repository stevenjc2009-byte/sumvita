/* ex_eval: numeric values, angle modes, domain errors, variables. */
#include <math.h>

#include "testlib.h"
#include "engine/arena.h"
#include "engine/expr.h"

static EvalEnv deg = {.degrees = true}, rad = {.degrees = false};

static int near(double a, double b) { return fabs(a - b) <= 1e-9 * fmax(1.0, fabs(b)); }

static void ok_(const char *s, const EvalEnv *env, double want, int line)
{
    double v;
    Expr *e = ex_parse(s, NULL, 0, NULL);
    t_total++;
    if (!e || !ex_eval(e, env, &v) || !near(v, want)) {
        t_fail++;
        printf("FAIL line %d: %s = %.15g want %.15g\n", line, s, e ? v : NAN, want);
    }
}

static void bad_(const char *s, const EvalEnv *env, int line)
{
    double v = 0;
    Expr *e = ex_parse(s, NULL, 0, NULL);
    t_total++;
    if (!e || ex_eval(e, env, &v) || !isnan(v)) {
        t_fail++;
        printf("FAIL line %d: %s should fail (got %g, parsed %d)\n", line, s, v, e != NULL);
    }
}
#define OK(s, w) ok_(s, &deg, w, __LINE__)
#define OKR(s, w) ok_(s, &rad, w, __LINE__)
#define BAD(s) bad_(s, &deg, __LINE__)
#define BADR(s) bad_(s, &rad, __LINE__)

int main(void)
{
    OK("1+2*3", 7);
    OK("2^3^2", 512);
    OK("-2^2", -4);
    OK("(-2)^2", 4);
    OK("3/4+5/6", 19.0 / 12.0);
    OK("2_1/3\xC3\x97" "1_1/2", 3.5);
    OK("\xE2\x88\x9A" "50", sqrt(50));
    OK("5!", 120);
    OK("0!", 1);
    OK("20%", 0.2);
    OK("20%\xC3\x97" "300", 60);
    OK("recur(0.1(27))", 7.0 / 55.0);
    OK("3.2\xC3\x97" "10^5", 320000);
    OK("99999999999^3", 9.9999999997e32);
    OK("\xCF\x80", M_PI);
    OK("e", M_E);
    OK("2\xCF\x80", 2 * M_PI);
    OK("e^2", exp(2));
    OK("exp(1)", M_E);
    /* trig, degrees */
    OK("sin 30", 0.5);
    OK("sin(90)", 1);
    OK("sin(180)", 0);
    OK("cos(90)", 0);
    OK("cos(60)", 0.5);
    OK("tan(45)", 1);
    OK("tan(180)", 0);
    BAD("tan(90)");
    BAD("tan(-270)");
    OK("sin(-30)", -0.5);
    OK("asin(0.5)", 30);
    OK("sin^-1(1)", 90);
    OK("acos(0)", 90);
    OK("atan(1)", 45);
    BAD("asin(2)");
    OK("sin 30\xC2\xB0", 0.5);
    /* radians */
    OKR("sin(\xCF\x80/6)", 0.5);
    OKR("sin(\xCF\x80)", 0);
    OKR("cos(\xCF\x80/2)", 0);
    BADR("tan(\xCF\x80/2)");
    OKR("sin 30\xC2\xB0", 0.5);
    OKR("asin(1)", M_PI / 2);
    OKR("sin(0.001)", sin(0.001));
    OKR("sinh(1)", sinh(1));
    OKR("cosh(1)", cosh(1));
    OKR("tanh(1)", tanh(1));
    /* logs, roots */
    OK("ln(e)", 1);
    OK("log(1000)", 3);
    OK("log(2,8)", 3);
    BAD("ln(0)");
    BAD("log(-1)");
    BAD("log(1,5)");
    OK("sqrt(16)", 4);
    BAD("\xE2\x88\x9A(-4)");
    OK("cbrt(27)", 3);
    OK("cbrt(-8)", -2);
    OK("root(4,16)", 2);
    BAD("root(4,-16)");
    OK("(-8)^(1/3)", -2);
    OK("(-8)^(2/3)", 4);
    BAD("(-8)^(1/2)");
    BAD("0^-1");
    BAD("x^-2");
    /* misc functions */
    OK("abs(-3)", 3);
    OK("|2-5|", 3);
    OK("floor(2.7)", 2);
    OK("floor(-2.5)", -3);
    OK("ceil(2.1)", 3);
    OK("round(2.5)", 3);
    OK("round(-2.5)", -3);
    OK("nCr(5,2)", 10);
    OK("nCr(52,5)", 2598960);
    OK("nPr(5,2)", 20);
    OK("nCr(5,0)", 1);
    BAD("nCr(2,5)");
    BAD("nCr(5.5,2)");
    BAD("(-1)!");
    BAD("2.5!");
    OK("170!", 7.257415615307994e306);
    BAD("171!");
    BAD("1/0");
    BAD("x=2");
    BAD("1,2");
    BAD("\"or\"");
    BAD("\xC2\xB1" "3");
    /* variables */
    {
        EvalEnv env = {.degrees = false, .var_names = {"x", "y", "Ans", "\xCE\xB8"}, .var_values = {3, -2, 10, 0.5}, .nvars = 4};
        ok_("2x^2y-8y+3x^2-12", &env, 2 * 9 * -2 + 16 + 27 - 12, __LINE__);
        ok_("y(2x+4)", &env, -20, __LINE__);
        ok_("(x+2)(x-3)(2x+1)", &env, 0, __LINE__);
        ok_("x^-2", &env, 1.0 / 9, __LINE__);
        ok_("Ans+1", &env, 11, __LINE__);
        ok_("2/3x", &env, 2, __LINE__);
        ok_("sin(\xCE\xB8)", &env, sin(0.5), __LINE__);
        ok_("xy", &env, -6, __LINE__);
        bad_("z+1", &env, __LINE__);
        bad_("x/(y+2)", &env, __LINE__);
    }
    {
        double v = 1;
        CHECK(!ex_eval(NULL, &deg, &v) && isnan(v), "NULL expr");
        Expr *e = ex_parse("sin(90)", NULL, 0, NULL);
        CHECK(ex_eval(e, NULL, &v) && near(v, sin(90.0)), "NULL env = radians");
    }
    return test_finish();
}
