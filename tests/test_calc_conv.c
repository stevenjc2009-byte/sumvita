/* calc_run conversions: -> Fraction / Decimal / % / Mixed, standard form,
 * surds (simplify + rationalise), prime factors - answers and working. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"

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
        printf("    step %d: [%s] [%s] [%s]\n", i, R.steps.v[i].title, R.steps.v[i].note, R.steps.v[i].math);
}

static const char *step_math(const char *t)
{
    for (int i = 0; i < R.steps.n; i++)
        if (strncmp(R.steps.v[i].title, t, strlen(t)) == 0) return R.steps.v[i].math;
    return "(no step)";
}

/* run, then check one step's maths line */
#define STEP(act, in, title, math) do { calc_run(act, in, &S, &R); \
    const char *m_ = step_math(title); CHECK_STR(m_, math); if (strcmp(m_, math)) dump(); } while (0)

#define TIMES "\xC3\x97"

static void to_fraction(void)
{
    const CalcAction F = ACT_TO_FRACTION;
    CHECK_STR(A(F, "0.375"), "3/8");
    STEP(F, "0.375", "Write over 1000", "0.375=375/1000");
    STEP(F, "0.375", "Simplify by the HCF 125", "375/1000=3/8");
    CHECK_STR(A(F, "recur(0.3(18))"), "7/22");
    STEP(F, "recur(0.3(18))", "Call the number x", "x=recur(0.3(18))");
    STEP(F, "recur(0.3(18))", "Multiply by 10", "10x=recur(3.(18))");
    STEP(F, "recur(0.3(18))", "Multiply by 1000", "1000x=recur(318.(18))");
    STEP(F, "recur(0.3(18))", "Subtract to remove", "990x=315");
    STEP(F, "recur(0.3(18))", "Divide by 990", "x=315/990");
    STEP(F, "recur(0.3(18))", "Simplify by the HCF 45", "315/990=7/22");
    CHECK_STR(A(F, "recur(0.(3))"), "1/3");
    CHECK_STR(A(F, "0.3(18)"), "7/22");
    STEP(F, "0.3(18)", "Call the number x", "x=recur(0.3(18))");
    CHECK_STR(A(ACT_EVAL, "0.3" TIMES "(18)"), "5.4");
    CHECK_STR(A(ACT_EVAL, "2(18)"), "36");
    CHECK_STR(A(F, "recur(0.(142857))"), "1/7");
    CHECK_STR(A(F, "35%"), "7/20");
    STEP(F, "35%", "Percent means out of 100", "35%=35/100");
    STEP(F, "35%", "Simplify by the HCF 5", "35/100=7/20");
    CHECK_STR(A(F, "12.5%"), "1/8");
    CHECK_STR(A(F, "-0.25"), "-1/4");
    CHECK_STR(A(F, "-(1_1/4)"), "-5/4");
    CHECK_STR(step_math("Put the minus sign back"), "-(1_1/4)=-5/4");
    CHECK_STR(A(F, "1_3/4"), "7/4");
    CHECK_STR(A(F, "6/8"), "3/4");
    CHECK_STR(A(F, "5"), "5");
    CHECK_STR(ALT(F, "0.375"), "0.375");
    CHECK_STR(A(F, "√2"), "ERR:Can't write this exactly as a fraction");
}

static void to_decimal(void)
{
    const CalcAction D = ACT_TO_DECIMAL;
    CHECK_STR(A(D, "3/8"), "0.375");
    CHECK_STR(ALT(D, "3/8"), "3/8");
    STEP(D, "3/8", "Make the bottom 1000", "3/8=375/1000=0.375");
    CHECK(strstr(R.steps.v[0].note, "only 2s and 5s") != NULL, "terminates note");
    CHECK_STR(A(D, "7/22"), "recur(0.3(18))");
    CHECK_STR(ALT(D, "7/22"), "0.3181818182");
    CHECK(strstr(R.steps.v[0].note, "prime factor 11") != NULL, "recurs note");
    CHECK_STR(A(D, "1/3"), "recur(0.(3))");
    CHECK_STR(A(D, "-2/3"), "-recur(0.(6))");
    CHECK_STR(A(D, "1/7"), "recur(0.(142857))");
    CHECK_STR(A(D, "7/4"), "1.75");
    CHECK_STR(A(D, "1_1/2"), "1.5");
    CHECK_STR(A(D, "6"), "6");
    CHECK_STR(A(D, "√2"), "1.414213562");
    CHECK_STR(A(D, "35%"), "0.35");
}

static void to_percent(void)
{
    const CalcAction P = ACT_TO_PERCENT;
    CHECK_STR(A(P, "3/8"), "37.5%");
    STEP(P, "3/8", "Multiply by 100%", "(3/8)" TIMES "100%=37.5%");
    CHECK_STR(A(P, "0.45"), "45%");
    CHECK_STR(A(P, "2"), "200%");
    CHECK_STR(A(P, "1/3"), "recur(33.(3))%");
    CHECK_STR(A(P, "7/20"), "35%");
    CHECK_STR(A(P, "1_1/4"), "125%");
}

static void to_mixed(void)
{
    const CalcAction M = ACT_TO_MIXED;
    CHECK_STR(A(M, "17/5"), "3_2/5");
    STEP(M, "17/5", "Divide the top by the bottom", "17/5=3_2/5");
    CHECK_STR(ALT(M, "17/5"), "3.4");
    CHECK_STR(A(M, "1_3/4"), "7/4");
    STEP(M, "1_3/4", "Multiply the whole number", "1" TIMES "4=4");
    STEP(M, "1_3/4", "Add the top", "1_3/4=7/4");
    CHECK_STR(A(M, "-17/5"), "-3_2/5");
    CHECK_STR(A(M, "3/4"), "3/4");
    CHECK_STR(A(M, "8/4"), "2");
    CHECK_STR(A(M, "19/12"), "1_7/12");
    CHECK_STR(A(M, "2.75"), "2_3/4");
}

static void std_form(void)
{
    const CalcAction X = ACT_STD_FORM;
    CHECK_STR(A(X, "0.00042"), "4.2" TIMES "10^-4");
    CHECK_STR(A(X, "3200000"), "3.2" TIMES "10^6");
    CHECK_STR(A(X, "4.2" TIMES "10^-4"), "4.2" TIMES "10^-4");
    STEP(X, "4.2" TIMES "10^-4", "Move the decimal point 4 places left", "4.2" TIMES "10^-4=0.00042");
    CHECK_STR(A(X, "3.2" TIMES "10^6"), "3.2" TIMES "10^6");
    CHECK_STR(A(X, "(3" TIMES "10^4)" TIMES "(2" TIMES "10^5)"), "6" TIMES "10^9");
    STEP(X, "(3" TIMES "10^4)" TIMES "(2" TIMES "10^5)", "Add the powers of 10", "10^4" TIMES "10^5=10^9");
    CHECK_STR(A(X, "(5" TIMES "10^3)" TIMES "(4" TIMES "10^2)"), "2" TIMES "10^6");
    CHECK_STR(A(X, "(6" TIMES "10^8)\xC3\xB7(2" TIMES "10^3)"), "3" TIMES "10^5");
    STEP(X, "(6" TIMES "10^8)\xC3\xB7(2" TIMES "10^3)", "Subtract the powers of 10", "10^8\xC3\xB7" "10^3=10^5");
    CHECK_STR(A(X, "1/3"), "3.333333333" TIMES "10^-1");
    CHECK_STR(A(X, "-45000"), "-4.5" TIMES "10^4");
    CHECK_STR(A(X, "0"), "0");
}

static void surds(void)
{
    const CalcAction Q = ACT_SURD;
    CHECK_STR(A(Q, "√72"), "6√2");
    STEP(Q, "√72", "Simplify the surd", "√72=√(36" TIMES "2)=6√2");
    CHECK_STR(A(Q, "√8+√18"), "5√2");
    STEP(Q, "√8+√18", "Collect like surds", "2√2+3√2=5√2");
    CHECK_STR(A(Q, "1/√2"), "√2/2");
    STEP(Q, "1/√2", "Multiply the top and bottom by √2", "1/√2=(1" TIMES "√2)/(√2" TIMES "√2)");
    CHECK_STR(A(Q, "2/√3"), "2√3/3");
    CHECK_STR(A(Q, "3/(2+√3)"), "6-3√3");
    STEP(Q, "3/(2+√3)", "Multiply the top and bottom by the conjugate", "3/(2+√3)=3(2-√3)/((2+√3)(2-√3))");
    STEP(Q, "3/(2+√3)", "Difference of two squares", "(2+√3)(2-√3)=2^2-(√3)^2=4-3=1");
    STEP(Q, "3/(2+√3)", "Expand the top", "3(2-√3)=6-3√3");
    CHECK_STR(A(Q, "1/(√3-1)"), "(1+√3)/2");
    CHECK_STR(A(Q, "5/√5"), "√5");
    CHECK_STR(A(Q, "√12/√3"), "2");
    CHECK_STR(A(Q, "1/(1+√2+√3)"), "ERR:Can't rationalise a bottom with 3 or more terms");
    CHECK_STR(A(Q, "√7"), "√7");
    CHECK_STR(R.steps.v[0].title, "Already as simple as it can be");
    CHECK_STR(A(Q, "2+3"), "5");
    CHECK_STR(ALT(Q, "3/(2+√3)"), "0.8038475773");
}

int main(void)
{
    calc_settings_default(&S);
    to_fraction();
    arena_reset(arena_current());
    to_decimal();
    to_percent();
    arena_reset(arena_current());
    to_mixed();
    std_form();
    arena_reset(arena_current());
    surds();
    return test_finish();
}
