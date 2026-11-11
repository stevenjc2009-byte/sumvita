/* calc_run: Evaluate (exact answers, BIDMAS / fraction / index / surd working),
 * Auto dispatch, Simplify, algebra hand-off, "of", Ans, dp / sf rounding. */
#include <math.h>

#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"
#include "engine/expr.h"

static CalcSettings S;
static CalcResult R;

static void reset(void)
{
    arena_reset(arena_current());
    calc_settings_default(&S);
}

static const CalcResult *run(CalcAction a, const char *in)
{
    calc_run(a, in, &S, &R);
    return &R;
}

/* answer, or "ERR:<message>" */
static const char *A(CalcAction a, const char *in)
{
    run(a, in);
    return R.ok ? R.answer : asprintf_a("ERR:%s", R.error);
}

static const char *ALT(CalcAction a, const char *in)
{
    run(a, in);
    return R.ok ? R.answer_alt : asprintf_a("ERR:%s", R.error);
}

/* index of the first step whose title starts with t, or -1 */
static int step_at(const char *t)
{
    for (int i = 0; i < R.steps.n; i++)
        if (strncmp(R.steps.v[i].title, t, strlen(t)) == 0) return i;
    return -1;
}

static const char *step_math(const char *t)
{
    int i = step_at(t);
    return i < 0 ? "(no step)" : R.steps.v[i].math;
}

static void dump(void)
{
    for (int i = 0; i < R.steps.n; i++)
        printf("    step %d: [%s] [%s] [%s]\n", i, R.steps.v[i].title, R.steps.v[i].note, R.steps.v[i].math);
}

#define E(in, want) CHECK_STR(A(ACT_EVAL, in), want)
#define EA(in, want) CHECK_STR(ALT(ACT_EVAL, in), want)
#define STEP(in, title, math) do { run(ACT_EVAL, in); \
    const char *m_ = step_math(title); CHECK_STR(m_, math); if (strcmp(m_, math)) dump(); } while (0)

static void defaults(void)
{
    calc_settings_default(&S);
    CHECK(S.degrees && S.dp == -1 && S.sf == 0 && S.prefer_exact && S.target_var[0] == 0 && S.ans == 0 && !S.ans_set,
          "defaults");
    const char *names[ACT_COUNT] = {"Auto", "Evaluate", "Simplify", "Expand", "Factorise", "Solve", "Complete square",
                                    "Rearrange", "\xE2\x86\x92 Fraction", "\xE2\x86\x92 Decimal", "\xE2\x86\x92 %",
                                    "\xE2\x86\x92 Mixed", "Standard form", "Surds"};
    for (int i = 0; i < ACT_COUNT; i++) CHECK_STR(calc_action_name((CalcAction)i), names[i]);
}

static void exact_eval(void)
{
    reset();
    E("3/4+5/6", "19/12");
    EA("3/4+5/6", "1.583333333");
    CHECK(step_at("As a mixed number") >= 0, "mixed number step");
    CHECK_STR(step_math("As a mixed number"), "19/12=1_7/12");
    E("√8+√18", "5√2");
    E("sqrt(72)", "6√2");
    E("8^(2/3)", "4");
    E("2^-3", "1/8");
    E("20% of 300", "60");
    E("15% of 80", "12");
    E("(1/2) of 10", "5");
    E("20%of300", "60");
    run(ACT_EVAL, "20% of 300");
    CHECK_STR(R.input_echo, "20%\xC3\x97" "300");
    run(ACT_EVAL, "20% of");
    CHECK(!R.ok, "\"20%% of\" with nothing after is an error");
    E("π/2+π", "3π/2");
    E("recur(0.3(18))", "7/22");
    E("2/3+1/6", "5/6");
    E("(3/4)\xC3\x97(2/9)", "1/6");
    E("(3/4)\xC3\xB7(1/2)", "3/2");
    E("12/18", "2/3");
    E("1_3/4+2_1/2", "4_1/4");             /* typed mixed -> answer mixed */
    EA("1_3/4+2_1/2", "4.25");
    E("1/3", "1/3");
    EA("1/3", "0.3333333333");
    E("0.1+0.2", "0.3");
    EA("0.1+0.2", "3/10");
    E("√2\xC3\x97\xE2\x88\x9A" "3", "√6");
    E("2√3\xC3\x97\xE2\x88\x9A" "3", "6");
    E("5!", "120");
    E("nCr(5,2)", "10");
    E("nPr(5,2)", "20");
    E("-7+3", "-4");
    E("2^10", "1024");
    E("3+4\xC3\x97" "2", "11");
    E("(3+4)\xC3\x97" "2", "14");
    E("2+3^2", "11");
    E("10-2\xC3\x97" "3+1", "5");
    E("|-5|", "5");
    E("1/√2", "√2/2");
    E("e", "2.718281828");
    E("2^0.5", "√2");
    E("27^(1/3)", "3");
    E("4^(-1/2)", "1/2");
    E("(2/3)^2", "4/9");
    E("(2/3)^-2", "9/4");
    E("50% of 50% of 80", "20");
    E("3.5\xC3\x97" "2", "7");
    EA("2", "");
    E("1-1", "0");
    E("2<3", "\"true\"");
    E("5=6", "\"false\"");
}

static void working(void)
{
    reset();
    run(ACT_EVAL, "3+4\xC3\x97" "2");
    CHECK(R.steps.n == 2, "3+4×2 has 2 steps, got %d", R.steps.n);
    if (R.steps.n == 2) {
        CHECK_STR(R.steps.v[0].title, "Multiply first");
        CHECK_STR(R.steps.v[0].math, "4\xC3\x97" "2=8");
        CHECK_STR(R.steps.v[1].title, "Add");
        CHECK_STR(R.steps.v[1].math, "3+8=11");
    } else dump();
    run(ACT_EVAL, "(3+4)\xC3\x97" "2");
    CHECK(R.steps.n == 2, "(3+4)×2 has 2 steps, got %d", R.steps.n);
    if (R.steps.n == 2) {
        CHECK_STR(R.steps.v[0].title, "Brackets first: Add");
        CHECK_STR(R.steps.v[0].math, "3+4=7");
        CHECK_STR(R.steps.v[1].math, "7\xC3\x97" "2=14");
    } else dump();
    run(ACT_EVAL, "2+3^2");
    CHECK(R.steps.n == 2 && strcmp(R.steps.v[0].title, "Indices first") == 0, "2+3^2 indices first");
    run(ACT_EVAL, "3+4");
    CHECK(R.steps.n == 0, "3+4 needs no working, got %d steps", R.steps.n);
    STEP("3/4+5/6", "Use a common denominator", "3/4+5/6=9/12+10/12");
    STEP("3/4+5/6", "Add the numerators", "9/12+10/12=19/12");
    STEP("(3/4)\xC3\x97(2/9)", "Multiply the tops", "3/4\xC3\x97" "2/9=6/36");
    STEP("(3/4)\xC3\x97(2/9)", "Simplify by the HCF", "6/36=1/6");
    STEP("(3/4)\xC3\xB7(1/2)", "Flip the second fraction", "(3/4)\xC3\xB7(1/2)=(3/4)\xC3\x97(2/1)");
    STEP("1_1/2+1_1/4", "Change the mixed number", "1_1/2=(1\xC3\x97" "2+1)/2=3/2");
    STEP("sqrt(72)", "Simplify the surd", "√72=√(36\xC3\x97" "2)=6√2");
    STEP("8^(2/3)", "Fractional power", "8^(2/3)=(root(3,8))^2=2^2=4");
    STEP("2^-3", "A negative power", "2^-3=1/2^3=1/8");
    STEP("20% of 300", "Find the percentage", "20% of 300=20/100\xC3\x97" "300=60");
    STEP("nCr(5,2)", "Use nCr", "nCr(5,2)=5!/(2!\xC3\x97" "3!)=120/(2\xC3\x97" "6)=10");
    STEP("sin(20)", "Round to 10 significant figures", "0.3420201433");
}

static void gcse_noncalc(void)
{
    reset();
    E("2/5+1/3", "11/15");
    E("3_1/2-1_3/4", "1_3/4");
    E("2_1/3\xC3\x97" "1_1/2", "3_1/2");
    E("2_1/3+1_3/4", "4_1/12");
    EA("2_1/3+1_3/4", "4.083333333");
    E("7/3+7/4", "49/12");                  /* no mixed typed: improper stays */
    E("1_1/2+1/2", "2");
    E("1_1/2-3", "-1_1/2");
    E("1_1/4-1/2", "3/4");
    E("1_1/2+0.25", "1.75");
    E("(4/5)\xC3\xB7(2/15)", "6");
    E("0.3\xC3\x97" "0.2", "0.06");
    E("0.6\xC3\xB7" "0.02", "30");
    E("16^(1/2)", "4");
    E("16^(3/4)", "8");
    E("125^(-1/3)", "1/5");
    E("(8/27)^(2/3)", "4/9");
    E("5^0", "1");
    E("√50-√8", "3√2");
    E("(2+√3)(2-√3)", "1");
    E("(1+√2)^2", "3+2√2");
    E("√12\xC3\x97\xE2\x88\x9A" "3", "6");
    E("√20/√5", "2");
    E("35% of 60", "21");
    E("2.5% of 400", "10");
    E("120\xC3\x97" "1.15", "138");
    E("3^2+4^2", "25");
    E("√(3^2+4^2)", "5");
    E("(-3)^2", "9");
    E("-3^2", "-9");
    E("2\xC3\x97(3+4)^2", "98");
    E("1/2+1/4+1/8", "7/8");
    E("4\xC3\x97\xCF\x80", "4π");
    E("\xCF\x80\xC3\x97" "3^2", "9π");
}

static void rounding_ans(void)
{
    reset();
    E("sin(20)", "0.3420201433");
    S.dp = 3;
    E("sin(20)", "0.342");
    EA("√2", "1.414");
    E("1.23456", "1.23456");
    S.dp = -1;
    S.sf = 3;
    EA("2/7", "0.286");
    EA("123456\xC3\x97\xCF\x80", "388000");
    E("1000sin(20)", "342");
    run(ACT_EVAL, "1000sin(20)");
    CHECK(step_at("Round to 3 significant figures") >= 0, "sf rounding step");
    S.sf = 0;
    S.prefer_exact = false;
    E("3/4+5/6", "1.583333333");
    EA("3/4+5/6", "19/12");
    S.prefer_exact = true;
    E("Ans+1", "1");
    S.ans = 5;
    S.ans_set = true;
    E("Ans+1", "6");
    E("2Ans", "10");
    S.ans = 0.25;
    E("Ans\xC3\x97" "4", "1");
    E("Ans", "1/4");
}

static void errors(void)
{
    reset();
    CHECK_STR(A(ACT_EVAL, "1/0"), "ERR:Maths error: can't divide by 0");
    CHECK(strstr(A(ACT_EVAL, "√(-4)"), "Maths error") != NULL, "sqrt -4");
    CHECK(strstr(A(ACT_EVAL, "ln(0)"), "Maths error") != NULL, "ln 0");
    CHECK(strstr(A(ACT_EVAL, "log(-1)"), "Maths error") != NULL, "log -1");
    CHECK(strstr(A(ACT_EVAL, "tan(90)"), "Maths error") != NULL, "tan 90");
    CHECK(strstr(A(ACT_EVAL, "tan(270)"), "Maths error") != NULL, "tan 270");
    CHECK(strstr(A(ACT_EVAL, "sin\xE2\x81\xBB\xC2\xB9(2)"), "Maths error") != NULL, "asin 2");
    CHECK(strstr(A(ACT_EVAL, "0^-1"), "Maths error") != NULL, "0^-1");
    CHECK(strstr(A(ACT_EVAL, "10^400"), "Maths error") != NULL, "huge");
    CHECK(strstr(A(ACT_EVAL, "(-1)!"), "Maths error") != NULL, "(-1)!");
    run(ACT_EVAL, "3+");
    CHECK(!R.ok && R.error_pos >= 0, "parse error has a position");
    run(ACT_EVAL, "");
    CHECK(!R.ok, "empty input is an error");
    calc_run(ACT_COUNT, "1", &S, &R);
    CHECK(!R.ok, "bad action");
    calc_run(ACT_EVAL, NULL, NULL, &R);
    CHECK(!R.ok, "NULL input");
}

static void dispatch_algebra(void)
{
    reset();
    CHECK_STR(A(ACT_AUTO, "3+4"), "7");
    CHECK_STR(A(ACT_AUTO, "2x+3x"), "5x");
    CHECK_STR(A(ACT_SIMPLIFY, "2x+3x"), "5x");
    CHECK(step_at("Collect like terms") >= 0, "collect like terms step");
    CHECK_STR(A(ACT_SIMPLIFY, "x^2\xC3\x97x^3"), "x^5");
    CHECK(step_at("Use the index laws") >= 0, "index laws step");
    CHECK_STR(A(ACT_REARRANGE, "y=2x+1"), "ERR:Choose the letter to make the subject");
    CHECK_STR(A(ACT_FACTOR, "360"), "2^3\xC3\x97" "3^2\xC3\x97" "5");
    CHECK(step_at("Factor tree") >= 0, "factor tree step");
    CHECK_STR(A(ACT_FACTOR, "17"), "17");
    CHECK_STR(A(ACT_FACTOR, "2"), "2");
    CHECK_STR(A(ACT_FACTOR, "1001"), "7\xC3\x97" "11\xC3\x97" "13");
    CHECK_STR(A(ACT_FACTOR, "1024"), "2^10");
    /* the algebra maths belongs to other modules: only check each action is handed over and never crashes */
    const char *alg[] = {"(x+1)(x+2)", "x^2+3x+2", "x+1=3", "x^2+6x+1", "2x+1>5", "x+y=3,x-y=1"};
    for (int a = ACT_AUTO; a <= ACT_REARRANGE; a++)
        for (int i = 0; i < 6; i++) {
            run((CalcAction)a, alg[i]);
            CHECK(R.ok ? R.answer != NULL : R.error[0] != 0, "action %d on %s", a, alg[i]);
        }
    snprintf(S.target_var, sizeof S.target_var, "x");
    run(ACT_REARRANGE, "y=2x+1");
    CHECK(R.ok || strcmp(R.error, "Choose the letter to make the subject") != 0, "rearrange hands over");
    CHECK_STR(A(ACT_TO_DECIMAL, "x+1"), "ERR:Decimal needs a number, not letters");
}

/* solve answers that are alternatives read "or"; one answer of several letters keeps commas */
static void solve_or(void)
{
    reset();
    CHECK_STR(A(ACT_SOLVE, "x^2-5x+6=0"), "x=2 \"or\" x=3");
    CHECK_STR(A(ACT_AUTO, "x^2-5x+6=0"), "x=2 \"or\" x=3");
    CHECK_STR(A(ACT_SOLVE, "x^3-6x^2+11x-6=0"), "x=1 \"or\" x=2 \"or\" x=3");
    CHECK_STR(A(ACT_SOLVE, "2x+3y=12, x-y=1"), "x=3, y=2");
    CHECK_STR(A(ACT_SOLVE, "y=x+1, y=x^2-1"), "x=-1, y=0 \"or\" x=2, y=3");
    CHECK_STR(A(ACT_SOLVE, "2x+1=7"), "x=3");
    CHECK_STR(A(ACT_SOLVE, "x^2>4"), "x<-2 \"or\" x>2");
    /* the answer is display syntax: it parses back and prints the same */
    const char *ans = A(ACT_SOLVE, "x^2-5x+6=0");
    char err[64];
    int pos;
    Expr *e = ex_parse(ans, err, sizeof err, &pos);
    CHECK(e != NULL && strcmp(ex_print(e), ans) == 0, "solve answer round-trips: %s", ans);
}

int main(void)
{
    defaults();
    exact_eval();
    working();
    gcse_noncalc();
    rounding_ans();
    errors();
    dispatch_algebra();
    solve_or();
    return test_finish();
}
