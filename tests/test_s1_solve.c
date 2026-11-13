/* v1.2 solver fixes: trig equations in a range, matching powers, surd equations with
 * checking, numeric fallback (no giant lists, merged roots, one precision), trial and
 * improvement. Answers are checked by VALUE (sorted, with a tolerance) and by putting
 * every answer back into the typed equation in the right angle mode. */
#include "solve_check.h"
#if __has_include("engine/solve_ext.h")
#include "engine/solve_ext.h"
#define HAVE_EXT 1
#endif

static void reset(bool deg)
{
    arena_reset(arena_current());
    calc_settings_default(&S);
    S.degrees = deg;
}

static bool has_step(const char *title)
{
    for (int i = 0; i < ST.n; i++)
        if (strstr(ST.v[i].title, title)) return true;
    return false;
}

static bool has_note(const char *text)
{
    for (int i = 0; i < ST.n; i++)
        if (ST.v[i].note && strstr(ST.v[i].note, text)) return true;
    return false;
}

static bool has_math(const char *m)
{
    for (int i = 0; i < ST.n; i++)
        if (ST.v[i].math && strcmp(ST.v[i].math, m) == 0) return true;
    return false;
}

static double evd(const Expr *e, const char *var, double x, bool deg)
{
    EvalEnv env;
    memset(&env, 0, sizeof env);
    env.degrees = deg;
    env.var_names[0] = var;
    env.var_values[0] = x;
    env.nvars = 1;
    double v;
    return ex_eval(e, &env, &v) ? v : NAN;
}

/* solve, then compare the answer values with `want` (ascending) and substitute each back */
static void VALS(const char *in, bool deg, const double *want, int n, double tol)
{
    reset(deg);
    Expr *ans = NULL;
    const char *got = run_alg(A_SOLVE, in, NULL, &ans);
    Sol s[32];
    int m = ans ? collect(ans, s, 32) : 0;
    CHECK(m == n, "%s (%s): %d answers, want %d (%s)", in, deg ? "deg" : "rad", m, n, got);
    if (m != n) return;
    Expr *e = P(in);
    for (int i = 0; i < n; i++) {
        CHECK(fabs(s[i].val[0] - want[i]) <= tol * (1 + fabs(want[i])), "%s (%s): answer %d is %.10g, want %.10g", in,
              deg ? "deg" : "rad", i, s[i].val[0], want[i]);
        double lhs = evd(e->kids[0], s[i].name[0], s[i].val[0], deg), rhs = evd(e->kids[1], s[i].name[0], s[i].val[0], deg);
        CHECK(fabs(lhs - rhs) <= 1e-6 * (1 + fabs(lhs)), "%s (%s): x=%.10g gives L=%.10g R=%.10g", in, deg ? "deg" : "rad",
              s[i].val[0], lhs, rhs);
        double top = deg ? 360 : 2 * M_PI;
        CHECK(s[i].val[0] >= -1e-9 && s[i].val[0] < top - 1e-9, "%s: x=%.10g outside the range", in, s[i].val[0]);
    }
}

/* the app's answer line (calc_run): alternatives read with "or"; steps stay in ST */
static const char *app_answer(const char *in)
{
    run_alg(A_SOLVE, in, NULL, NULL);
    CalcResult r;
    calc_run(ACT_SOLVE, in, &S, &r);
    return r.ok ? r.answer : asprintf_a("ERR:%s", r.error);
}

static void STR(const char *in, bool deg, const char *want)
{
    reset(deg);
    CHECK_STR(app_answer(in), want);
}

static const char *NOSOL_SIN = "\"No solutions: sin x is always between \xE2\x88\x92" "1 and 1\"";
static const char *NOSOL_COS = "\"No solutions: cos x is always between \xE2\x88\x92" "1 and 1\"";

static void trig_deg(void)
{
    STR("sin(x)=0.5", true, "x=30 \"or\" x=150");
    CHECK(has_step("first solution") && has_step("second solution"), "sin=0.5 steps");
    CHECK(has_math("x=asin(0.5)=30") && has_math("x=180-30=150"), "sin=0.5 working lines");
    STR("sin(x)=1/2", true, "x=30 \"or\" x=150");
    STR("cos(x)=0.5", true, "x=60 \"or\" x=300");
    CHECK(has_math("x=360-60=300"), "cos symmetry line");
    STR("tan(x)=-1", true, "x=135 \"or\" x=315");
    CHECK(has_math("x=atan(-1)=-45") && has_math("x=-45+180=135"), "tan working lines");
    STR("2cos(x)-1=0", true, "x=60 \"or\" x=300");
    CHECK(has_math("2cos(x)=1") && has_math("cos(x)=1/2"), "2cos(x)-1=0 isolating lines");
    STR("sin(2x)=1", true, "x=45 \"or\" x=225");
    CHECK(has_math("2x=90 \"or\" 2x=450"), "sin(2x)=1 lists 2x");
    STR("3tan(x)+1=4", true, "x=45 \"or\" x=225");
    STR("sin(x)=2", true, NOSOL_SIN);
    STR("cos(x)=-1.5", true, NOSOL_COS);
    STR("sin(x)=-1", true, "x=270");
    STR("cos(x)=1", true, "x=0");
    STR("cos(x)=-1", true, "x=180");
    STR("sin(x)=0", true, "x=0 \"or\" x=180");
    STR("tan(x)=0", true, "x=0 \"or\" x=180");
    STR("sin(x)=√3/2", true, "x=60 \"or\" x=120");
    STR("cos(x)=√2/2", true, "x=45 \"or\" x=315");
    STR("sin(x)=-√2/2", true, "x=225 \"or\" x=315");
    STR("tan(x)=√3", true, "x=60 \"or\" x=240");
    STR("sin(θ)=0.5", true, "θ=30 \"or\" θ=150");
    STR("4sin(x)-2=0", true, "x=30 \"or\" x=150");
    STR("2sin(x)=-1", true, "x=210 \"or\" x=330");
    STR("sin(x)=-0.5", true, "x=210 \"or\" x=330");
    STR("1=2cos(x)", true, "x=60 \"or\" x=300");
    STR("cos(2x)=0", true, "x=45 \"or\" x=135 \"or\" x=225 \"or\" x=315");
    STR("tan(2x)=1", true, "x=22.5 \"or\" x=112.5 \"or\" x=202.5 \"or\" x=292.5");
    {
        const double w[] = { 30, 150 }, w2[] = { 60, 300 }, w3[] = { 135, 315 }, w4[] = { 45, 225 };
        VALS("sin(x)=0.5", true, w, 2, 1e-9);
        VALS("cos(x)=0.5", true, w2, 2, 1e-9);
        VALS("tan(x)=-1", true, w3, 2, 1e-9);
        VALS("sin(2x)=1", true, w4, 2, 1e-9);
        VALS("3tan(x)+1=4", true, w4, 2, 1e-9);
        const double d1[] = { 17.45760312, 162.5423969 }, d2[] = { 197.4576031, 342.5423969 };
        VALS("sin(x)=0.3", true, d1, 2, 1e-8);
        VALS("sin(x)=-0.3", true, d2, 2, 1e-8);
        const double d3[] = { 66.42182152, 293.5781785 };
        VALS("cos(x)=0.4", true, d3, 2, 1e-8);
        const double d4[] = { 5.710593137, 185.7105931 };
        VALS("tan(x)=0.1", true, d4, 2, 1e-8);
        const double w6[] = { 16.66666667, 96.66666667, 136.6666667, 216.6666667, 256.6666667, 336.6666667 };
        VALS("cos(3x+10)=0.5", true, w6, 6, 1e-8);
        const double w7[] = { 10, 40, 190, 220 };
        VALS("sin(2x+40)=sin(60)", true, w7, 4, 1e-6);
    }
    reset(true);
    Expr *ans;
    run_alg(A_SOLVE, "sin(x)=0.5", NULL, &ans);
    CHECK(ans && ans->kind == E_LIST && ans->nkids == 2, "sin=0.5 has two answers, not a long list");
}

static void trig_rad(void)
{
    STR("sin(x)=0.5", false, "x=π/6 \"or\" x=5π/6");
    CHECK(has_math("x=π-π/6=5π/6"), "radian symmetry line");
    STR("cos(x)=0.5", false, "x=π/3 \"or\" x=5π/3");
    STR("cos(x)=-1/2", false, "x=2π/3 \"or\" x=4π/3");
    STR("tan(x)=-1", false, "x=3π/4 \"or\" x=7π/4");
    STR("2cos(x)-1=0", false, "x=π/3 \"or\" x=5π/3");
    STR("sin(2x)=1", false, "x=π/4 \"or\" x=5π/4");
    STR("sin(x)=1", false, "x=π/2");
    STR("cos(x)=1", false, "x=0");
    STR("sin(x)=2", false, NOSOL_SIN);
    {
        const double w[] = { 0.3046926540, 2.83689999957 };
        VALS("sin(x)=0.3", false, w, 2, 1e-8);
        const double w2[] = { M_PI / 6, 5 * M_PI / 6 };
        VALS("sin(x)=0.5", false, w2, 2, 1e-9);
        const double w3[] = { M_PI / 4, 5 * M_PI / 4 };
        VALS("sin(2x)=1", false, w3, 2, 1e-9);
        const double w4[] = { M_PI / 4, 5 * M_PI / 4 };
        VALS("sin(x)=cos(x)", false, w4, 2, 1e-7);
        const double w5[] = { 0.4636476090, 3.605240263 };
        VALS("tan(x)=0.5", false, w5, 2, 1e-8);
    }
}

static void trig_numeric(void)
{
    const double w[] = { 45, 225 };
    VALS("sin(x)=cos(x)", true, w, 2, 1e-7);
    CHECK(has_step("numerically"), "sin=cos is the numeric method");
    reset(true);
    CHECK(strncmp(run_alg(A_SOLVE, "sin(x)=cos(x)", NULL, NULL), "x=45", 4) == 0, "sin=cos answer starts x=45");
}

static void powers(void)
{
    STR("2^(x+3)=4^x", true, "x=3");
    CHECK(has_step("powers of 2") && has_math("2^(x+3)=2^(2x)") && has_math("x+3=2x"), "2^(x+3)=4^x working");
    STR("9^x=3^(x+1)", true, "x=1");
    CHECK(has_math("3^(2x)=3^(x+1)"), "9^x working");
    STR("8^x=2^(x+4)", true, "x=2");
    STR("27^(x-1)=9^x", true, "x=3");
    CHECK(has_math("3^(3(x-1))=3^(2x)") && has_math("3(x-1)=2x"), "27^(x-1) working");
    STR("4^x=2^(x+3)", true, "x=3");
    STR("25^x=5^(x+2)", true, "x=2");
    STR("3^x=9^(x-1)", true, "x=2");
    STR("2^(2x)=8^(x-1)", true, "x=3");
    STR("2^(x+1)=2^(x+2)", true, "\"no solution\"");
    for (int i = 0; i < 4; i++) {
        const char *in[] = { "2^(x+3)=4^x", "9^x=3^(x+1)", "8^x=2^(x+4)", "27^(x-1)=9^x" };
        reset(true);
        Expr *ans;
        run_alg(A_SOLVE, in[i], NULL, &ans);
        CHECK(ans && verify(in[i], ans) == 1, "%s: the answer does not fit the equation", in[i]);
    }
}

static void surds(void)
{
    STR("√(x+3)=x-3", true, "x=6");
    CHECK(has_step("Square both sides") && has_math("x+3=(x-3)^2") && has_step("Check x=1") && has_step("Check x=6"),
          "√(x+3)=x-3 working");
    CHECK(has_note("reject") && has_note("match"), "false root rejected, true root accepted");
    STR("√x=x-2", true, "x=4");
    CHECK(has_math("x=(x-2)^2") && has_math("√1=1-2") && has_math("√4=4-2"), "√x=x-2 working");
    STR("√(2x-1)=x-2", true, "x=5");
    STR("√(2x-1)=x", true, "x=1");
    STR("√(x+3)+3=x", true, "x=6");
    CHECK(has_step("Isolate the square root") && has_math("√(x+3)=x-3"), "isolate the root first");
    STR("√(x+1)=x-5", true, "x=8");
    STR("√(x+2)=x", true, "x=2");
    STR("2√(x)=x-3", true, "x=9");
    STR("√(x-1)=x", true, "\"no real solutions\"");
    CHECK(has_step("Square both sides"), "no-root case still shows the squaring");
    STR("√(x)=-x-1", true, "\"no real solutions\"");
    {
        const double w[] = { (3 + sqrt(5.0)) / 2 };
        VALS("√x=x-1", true, w, 1, 1e-9);
        const double w2[] = { 1, 3 };
        VALS("√(4x-3)=x", true, w2, 2, 1e-9);
    }
    STR("√(4x-3)=x", true, "x=1 \"or\" x=3");
    reset(true);
    Expr *ans;
    run_alg(A_SOLVE, "√x=x-2", NULL, &ans);
    CHECK(ans && verify("√x=x-2", ans) == 1, "√x=x-2 answer fits");
}

static void numeric(void)
{
    STR("floor(x)=x", true, "ERR:I couldn't solve this equation.");
    STR("√(2x-1)-x=0", true, "x=1");
    CHECK(has_math("x=1"), "double root printed once in the step too");
    STR("√(4x-3)-x=0", true, "x=1 \"or\" x=3");
    STR("2^x=x^2", true, "x=-0.766664696 \"or\" x=2 \"or\" x=4");
    reset(true);
    CHECK(strncmp(run_alg(A_SOLVE, "floor(x)=x", NULL, NULL), "ERR:", 4) == 0, "no list for floor(x)=x");
    /* one precision: the step and the answer line agree */
    STR("2^x=5", true, "x=2.321928095");
    CHECK(has_math("x=2.321928095"), "2^x=5: step shows the answer's digits");
    reset(true);
    S.dp = 3;
    CHECK_STR(app_answer("2^x=5"), "x=2.322");
    CHECK(has_math("x=2.322"), "dp=3: step shows 3 d.p. like the answer");
    reset(true);
    S.dp = 2;
    CHECK_STR(app_answer("√(2x-1)-x=0"), "x=1");
    reset(true);
    S.dp = 2;
    CHECK_STR(app_answer("2^x=x^2"), "x=-0.77 \"or\" x=2 \"or\" x=4");
    CHECK(has_math("x=-0.77 \"or\" x=2 \"or\" x=4"), "dp=2 numeric step matches answer");
    reset(true);
    S.dp = 3;
    CHECK_STR(app_answer("sin(x)=0.3"), "x=17.458 \"or\" x=162.542");
    CHECK(has_math("x=17.458 \"or\" x=162.542"), "dp=3 trig step matches answer");
}

static void through_calc(void)
{
    static const char *const in[] = { "sin(x)=0.5", "2cos(x)-1=0", "2^(x+3)=4^x", "√(x+3)=x-3" };
    static const char *const want[] = { "x=30 \"or\" x=150", "x=60 \"or\" x=300", "x=3", "x=6" };
    for (int i = 0; i < 4; i++) {
        for (int act = 0; act < 2; act++) {
            reset(true);
            CalcResult r;
            calc_run(act ? ACT_SOLVE : ACT_AUTO, in[i], &S, &r);
            CHECK(r.ok, "%s: %s", in[i], r.error);
            if (r.ok) CHECK_STR(r.answer, want[i]);
        }
    }
    reset(false);
    CalcResult r;
    calc_run(ACT_AUTO, "sin(x)=0.5", &S, &r);
    CHECK(r.ok && r.answer && strcmp(r.answer, "x=π/6 \"or\" x=5π/6") == 0, "rad mode through calc_run");
}

#ifdef HAVE_EXT
static void trial(void)
{
    SvCtx c;
    memset(&c, 0, sizeof c);
    snprintf(c.var, sizeof c.var, "x");
    reset(true);
    c.s = &S;
    c.st = &ST;
    steps_init(&ST);
    const double cub[] = { -12, 1, 0, 1 };
    sv_trial_steps(&c, cub, 3, 2.144040433);
    CHECK(ST.n >= 3, "trial: %d steps", ST.n);
    CHECK(has_math("2<x<3") && has_math("2.1<x<2.2") && has_math("2.14<x<2.15"), "trial brackets for x^3+x=12");
    CHECK(has_note("-2") && has_note("18"), "trial shows f(2)=-2 and f(3)=18");
    steps_init(&ST);
    const double c2[] = { 1, -5, 0, 1 };
    sv_trial_steps(&c, c2, 3, 0.2016396757);
    CHECK(has_math("0<x<1") && has_math("0.2<x<0.3") && has_math("0.20<x<0.21"), "trial brackets for x^3-5x+1 (small root)");
    steps_init(&ST);
    sv_trial_steps(&c, c2, 3, -2.33005874);
    CHECK(has_math("-3<x<-2") && has_math("-2.4<x<-2.3") && has_math("-2.34<x<-2.33"), "trial brackets for the negative root");
    for (int i = 0; i < ST.n; i++) {
        char err[96];
        int pos;
        CHECK(ex_parse(ST.v[i].math, err, sizeof err, &pos) != NULL, "trial maths parses: %s", ST.v[i].math);
    }
}
#endif

int main(void)
{
    trig_deg();
    trig_rad();
    trig_numeric();
    powers();
    surds();
    numeric();
    through_calc();
#ifdef HAVE_EXT
    trial();
#endif
    return test_finish();
}
