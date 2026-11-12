/* calc_run fixes from the T1 tester run (agent F2): standard form kept as
 * standard form, radians tan message, big-number factorise message, subtraction
 * titles, surd working on "=", expansion of surd brackets, GCSE conjugate,
 * standard-form working on "=", tidy -> Mixed / -> Decimal / recurring steps,
 * one-line working for small sign / decimal sums. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"

#define TIMES "\xC3\x97"
#define DIVS "\xC3\xB7"
#define ROOT "\xE2\x88\x9A"

static CalcSettings S;
static CalcResult R;

static const char *A(CalcAction a, const char *in)
{
    calc_run(a, in, &S, &R);
    return R.ok ? R.answer : asprintf_a("ERR:%s", R.error);
}

static const char *ALT(CalcAction a, const char *in)
{
    calc_run(a, in, &S, &R);
    return R.ok ? R.answer_alt : asprintf_a("ERR:%s", R.error);
}

static void dump(void)
{
    for (int i = 0; i < R.steps.n; i++)
        printf("    step %d: [%s] [%s] [%s]\n", i, R.steps.v[i].title, R.steps.v[i].note,
               R.steps.v[i].math ? R.steps.v[i].math : "");
}

static const Step *find(const char *t)
{
    for (int i = 0; i < R.steps.n; i++)
        if (strncmp(R.steps.v[i].title, t, strlen(t)) == 0) return &R.steps.v[i];
    return NULL;
}

static const char *step_math(const char *t)
{
    const Step *s = find(t);
    return s ? (s->math ? s->math : "") : "(no step)";
}

#define STEP(act, in, title, math) do { calc_run(act, in, &S, &R); \
    const char *m_ = step_math(title); CHECK_STR(m_, math); if (strcmp(m_, math)) dump(); } while (0)
#define NO_STEP(act, in, title) do { calc_run(act, in, &S, &R); \
    CHECK(find(title) == NULL, "%s: unexpected step '%s'", in, title); if (find(title)) dump(); } while (0)

static void v_items(void)
{
    const CalcAction X = ACT_STD_FORM;
    /* V1: already standard form stays standard form */
    CHECK_STR(A(X, "3" TIMES "10^5"), "3" TIMES "10^5");
    CHECK_STR(ALT(X, "3" TIMES "10^5"), "300000");
    CHECK_STR(A(X, "3.2" TIMES "10^5"), "3.2" TIMES "10^5");
    CHECK_STR(A(X, "10^6"), "1" TIMES "10^6");
    CHECK_STR(ALT(X, "10^6"), "1000000");
    STEP(X, "10^6", "Already in standard form", "10^6=1" TIMES "10^6");
    STEP(X, "4.2" TIMES "10^-4", "Move the decimal point 4 places left", "4.2" TIMES "10^-4=0.00042");
    CHECK_STR(A(X, "(3" TIMES "10^5)-(2" TIMES "10^5)"), "1" TIMES "10^5");
    /* V2: too big for int64 */
    CHECK_STR(A(X, "6.02" TIMES "10^23"), "6.02" TIMES "10^23");
    CHECK_STR(ALT(X, "6.02" TIMES "10^23"), "602000000000000000000000");
    CHECK_STR(A(X, "1.6" TIMES "10^-19"), "1.6" TIMES "10^-19");
    CHECK_STR(ALT(X, "1.6" TIMES "10^-19"), "0.00000000000000000016");
    CHECK_STR(A(X, "2" TIMES "10^200"), "2" TIMES "10^200");
    /* V3: radians message has no degrees */
    S.degrees = false;
    CHECK_STR(A(ACT_AUTO, "tan(\xCF\x80/2)"), "ERR:Maths error: tan(\xCF\x80/2) is undefined");
    S.degrees = true;
    CHECK_STR(A(ACT_AUTO, "tan(90)"), "ERR:Maths error: tan 90\xC2\xB0 is undefined");
    /* V4 */
    CHECK_STR(A(ACT_FACTOR, "12345678901234567890"), "ERR:Too big to factorise (largest is 10^12)");
    CHECK_STR(A(ACT_FACTOR, "2.5"), "ERR:Prime factors need a whole number");
}

static void w1_subtract(void)
{
    const CalcAction E = ACT_AUTO;
    STEP(E, "3/4-5/6", "Subtract the numerators", "9/12-10/12=-1/12");
    NO_STEP(E, "3/4-5/6", "Add the numerators");
    STEP(E, "7/8-3/8", "Subtract the numerators", "7/8-3/8=4/8");
    STEP(E, "3_3/4-2_1/8", "Subtract the numerators", "30/8-17/8=13/8");
    STEP(E, "2_1/2-3_3/4", "Subtract the numerators", "10/4-15/4=-5/4");
    STEP(E, "3/4+5/6", "Add the numerators", "9/12+10/12=19/12");
    STEP(E, "1/2+1/3-1/4", "Add and subtract the numerators", "6/12+4/12-3/12=7/12");
    calc_run(ACT_SIMPLIFY, "1/x-1/(x+1)", &S, &R);
    CHECK(find("Subtract the numerators") != NULL, "algebraic fraction subtract");
    if (!find("Subtract the numerators")) dump();
}

static void w2_w3_w4_surds(void)
{
    const CalcAction Q = ACT_SURD, E = ACT_AUTO;
    /* W4: GCSE conjugate, positive bottom */
    STEP(Q, "1/(" ROOT "3-1)", "Multiply the top and bottom by the conjugate",
         "1/(" ROOT "3-1)=1(" ROOT "3+1)/((" ROOT "3-1)(" ROOT "3+1))");
    STEP(Q, "1/(" ROOT "3-1)", "Difference of two squares", "(" ROOT "3-1)(" ROOT "3+1)=(" ROOT "3)^2-1^2=3-1=2");
    CHECK_STR(A(Q, "1/(" ROOT "3-1)"), "(1+" ROOT "3)/2");
    STEP(Q, "2/(3+" ROOT "5)", "Multiply the top and bottom by the conjugate",
         "2/(3+" ROOT "5)=2(3-" ROOT "5)/((3+" ROOT "5)(3-" ROOT "5))");
    /* W2: the same working on "=" */
    STEP(E, "1/(" ROOT "3-1)", "Multiply the top and bottom by the conjugate",
         "1/(" ROOT "3-1)=1(" ROOT "3+1)/((" ROOT "3-1)(" ROOT "3+1))");
    CHECK_STR(A(E, "1/(" ROOT "3-1)"), "(1+" ROOT "3)/2");
    STEP(E, "2/(3+" ROOT "5)", "Difference of two squares", "(3+" ROOT "5)(3-" ROOT "5)=3^2-(" ROOT "5)^2=9-5=4");
    CHECK_STR(A(E, "2/(3+" ROOT "5)"), "(3-" ROOT "5)/2");
    STEP(E, "(3+" ROOT "2)(1+" ROOT "2)", "Multiply out the brackets",
         "(3+" ROOT "2)(1+" ROOT "2)=3+3" ROOT "2+" ROOT "2+2");
    CHECK_STR(A(E, "(3+" ROOT "2)(1+" ROOT "2)"), "5+4" ROOT "2");
    /* W3: expansion of a square */
    STEP(Q, "(1+" ROOT "3)^2", "Write the square as two brackets", "(1+" ROOT "3)^2=(1+" ROOT "3)(1+" ROOT "3)");
    STEP(Q, "(1+" ROOT "3)^2", "Multiply out the brackets", "(1+" ROOT "3)(1+" ROOT "3)=1+" ROOT "3+" ROOT "3+3");
    STEP(Q, "(1+" ROOT "3)^2", "Collect like terms", "1+" ROOT "3+" ROOT "3+3=4+2" ROOT "3");
    CHECK_STR(A(Q, "(1+" ROOT "3)^2"), "4+2" ROOT "3");
    STEP(Q, "(3+" ROOT "2)(1+" ROOT "2)", "Collect like terms", "3+3" ROOT "2+" ROOT "2+2=5+4" ROOT "2");
    STEP(Q, "(2-" ROOT "3)(2+" ROOT "3)", "Multiply out the brackets",
         "(2-" ROOT "3)(2+" ROOT "3)=4+2" ROOT "3-2" ROOT "3-3");
    CHECK_STR(A(Q, "(2-" ROOT "3)(2+" ROOT "3)"), "1");
}

static void w5_stdform_working(void)
{
    const CalcAction E = ACT_AUTO;
    const char *d = "(6" TIMES "10^8)/(3" TIMES "10^3)";
    STEP(E, d, "Divide the numbers", "6" DIVS "3=2");
    STEP(E, d, "Subtract the powers of 10", "10^8" DIVS "10^3=10^5");
    STEP(E, d, "Write as an ordinary number", "2" TIMES "10^5=200000");
    NO_STEP(E, d, "Brackets first");
    CHECK_STR(A(E, d), "200000");
    const char *a = "(2" TIMES "10^5)+(3" TIMES "10^4)";
    STEP(E, a, "Write both with the same power of 10", "2" TIMES "10^5+3" TIMES "10^4=2" TIMES "10^5+0.3" TIMES "10^5");
    STEP(E, a, "Add the numbers", "2" TIMES "10^5+0.3" TIMES "10^5=2.3" TIMES "10^5");
    STEP(E, a, "Write as an ordinary number", "2.3" TIMES "10^5=230000");
    NO_STEP(E, a, "Brackets first");
    CHECK_STR(A(E, a), "230000");
    const char *m = "(3" TIMES "10^4)*(2" TIMES "10^3)";
    STEP(E, m, "Add the powers of 10", "10^4" TIMES "10^3=10^7");
    CHECK_STR(A(E, m), "60000000");
    STEP(E, "(3" TIMES "10^5)-(2" TIMES "10^5)", "Subtract the numbers",
         "3" TIMES "10^5-2" TIMES "10^5=1" TIMES "10^5");
}

static void w6_tidy(void)
{
    calc_run(ACT_TO_MIXED, "-7/3", &S, &R);
    CHECK(R.steps.n == 1 && strcmp(R.steps.v[0].title, "Divide the top by the bottom") == 0, "-7/3 one step");
    if (R.steps.n != 1) dump();
    CHECK_STR(A(ACT_TO_MIXED, "-7/3"), "-2_1/3");
    calc_run(ACT_TO_DECIMAL, "1/3", &S, &R);
    CHECK_STR(R.steps.v[0].note, "The denominator 3 has a prime factor other than 2 or 5, so the decimal recurs");
    const CalcAction F = ACT_TO_FRACTION;
    const char *x = "recur(0.0(4))";
    STEP(F, x, "Multiply by 10", "10x=recur(0.(4))");
    STEP(F, x, "Multiply by 100", "100x=recur(4.(4))");
    STEP(F, x, "Subtract to remove", "90x=4");
    STEP(F, x, "Divide by 90", "x=4/90");
    STEP(F, x, "Simplify by the HCF 2", "4/90=2/45");
    NO_STEP(F, x, "Make the top a whole number");
    CHECK_STR(A(F, x), "2/45");
}

static const char *last_math(void) { return R.steps.n ? R.steps.v[R.steps.n - 1].math : "(no steps)"; }

static void w7_one_liners(void)
{
    const CalcAction E = ACT_AUTO;
    calc_run(E, "(-3)^2", &S, &R);
    CHECK_STR(last_math(), "(-3)^2=(-3)" TIMES "(-3)=9");
    calc_run(E, "-3^2", &S, &R);
    CHECK_STR(last_math(), "-3^2=-(3^2)=-9");
    calc_run(E, "-2*-3", &S, &R);
    CHECK(R.steps.n == 1 && strstr(R.steps.v[0].note, "negative") != NULL, "-2*-3 sign step");
    if (R.steps.n != 1) dump();
    calc_run(E, "0.5+1/2", &S, &R);
    CHECK_STR(last_math(), "0.5+1/2=0.5+0.5=1");
    calc_run(E, "80*1.15", &S, &R);
    CHECK_STR(last_math(), "80" TIMES "1.15=92");
    calc_run(E, "250/1.25", &S, &R);
    CHECK_STR(last_math(), "250/1.25=200");
    calc_run(E, "3+4", &S, &R);
    CHECK(R.steps.n == 0, "3+4 still has no steps");
}

int main(void)
{
    calc_settings_default(&S);
    v_items();
    arena_reset(arena_current());
    w1_subtract();
    arena_reset(arena_current());
    w2_w3_w4_surds();
    arena_reset(arena_current());
    w5_stdform_working();
    arena_reset(arena_current());
    w6_tidy();
    arena_reset(arena_current());
    w7_one_liners();
    return test_finish();
}
