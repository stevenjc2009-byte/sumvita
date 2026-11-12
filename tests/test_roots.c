/* Solutions -> equation (src/engine/roots_eq.c): "x=-2 and x=-3" on =, Solve or
 * Expand gives x^2+5x+6=0 with GCSE working; bare "and" / "or" between two
 * relations is a list separator (parse_lex.c); everything else unchanged. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"

static CalcSettings S;
static CalcResult R;

static const char *A(CalcAction a, const char *in)
{
    arena_reset(arena_current());
    calc_settings_default(&S);
    calc_run(a, in, &S, &R);
    return R.ok ? R.answer : asprintf_a("ERR:%s", R.error);
}

/* every step as "title [math]" joined by " | " (depth shown as leading '>') */
static const char *steps_text(void)
{
    const char *s = "";
    for (int i = 0; i < R.steps.n; i++) {
        const Step *st = &R.steps.v[i];
        s = asprintf_a("%s%s%.*s%s [%s]", s, i ? " | " : "", st->depth, ">>>>", st->title, st->math);
    }
    return s;
}

#define AUTO(in, want) CHECK_STR(A(ACT_AUTO, in), want)
#define ALT(in, want) do { A(ACT_AUTO, in); CHECK_STR(R.answer_alt, want); } while (0)

static void answers(void)
{
    AUTO("x=-2 and x=-3", "x^2+5x+6=0");
    AUTO("x= -2 and x= -3", "x^2+5x+6=0");
    AUTO("X= -2 and x= -3", "x^2+5x+6=0");
    AUTO("x=-2, x=-3", "x^2+5x+6=0");
    AUTO("x=-2 or x=-3", "x^2+5x+6=0");
    AUTO("x = - 2 and x = - 3", "x^2+5x+6=0");
    AUTO("x=2 \"or\" x=3", "x^2-5x+6=0");
    AUTO("-2=x and -3=x", "x^2+5x+6=0");
    AUTO("X=2 and X=3", "X^2-5X+6=0");
    AUTO("x=2, X=3", "x^2-5x+6=0");
    AUTO("x=4 and x=4", "x^2-8x+16=0");
    AUTO("x=1/2 and x=-3", "2x^2+5x-3=0");
    AUTO("x=-1/2, x=2/3", "6x^2-x-2=0");
    AUTO("x=1.5 and x=2", "2x^2-7x+6=0");
    AUTO("x=0 and x=5", "x^2-5x=0");
    AUTO("x=1, x=2, x=3", "x^3-6x^2+11x-6=0");
    AUTO("x=2 and x=3 and x=-1", "x^3-4x^2+x+6=0");
    AUTO("x=\xC2\xB1" "3", "x^2-9=0");
    AUTO("x=3\xC2\xB1\xE2\x88\x9A" "5", "x^2-6x+4=0");
    AUTO("x=3+\xE2\x88\x9A" "5 and x=3\xE2\x88\x92\xE2\x88\x9A" "5", "x^2-6x+4=0");
    AUTO("x=(1\xC2\xB1\xE2\x88\x9A" "5)/2", "x^2-x-1=0");
    AUTO("x=2\xE2\x88\x9A" "3 and x=-2\xE2\x88\x9A" "3", "x^2-12=0");
    AUTO("x=1 and x=3+\xE2\x88\x9A" "5 and x=3-\xE2\x88\x9A" "5", "x^3-7x^2+10x-4=0");
    AUTO("\xCE\xB8=1 and \xCE\xB8=2", "\xCE\xB8^2-3\xCE\xB8+2=0");
    ALT("x=-2 and x=-3", "(x+2)(x+3)=0");
    ALT("x=4 and x=4", "(x-4)^2=0");
    ALT("x=1/2 and x=-3", "(2x-1)(x+3)=0");
    ALT("x=0 and x=5", "x(x-5)=0");
    ALT("x=5 and x=0", "x(x-5)=0");
    ALT("x=3\xC2\xB1\xE2\x88\x9A" "5", "(x-3-\xE2\x88\x9A" "5)(x-3+\xE2\x88\x9A" "5)=0");
    /* Solve and Expand on the same input do the same job */
    CHECK_STR(A(ACT_SOLVE, "x=-2 and x=-3"), "x^2+5x+6=0");
    CHECK_STR(A(ACT_EXPAND, "x=-2 and x=-3"), "x^2+5x+6=0");
    CHECK_STR(A(ACT_SOLVE, "x=1/2, x=-3"), "2x^2+5x-3=0");
}

static void working(void)
{
    A(ACT_AUTO, "X= -2 and x= -3");
    CHECK_STR(steps_text(),
              "Each solution gives a bracket [x+2=0, x+3=0] | Multiply the brackets [(x+2)(x+3)=0] | "
              "Multiply out the brackets [x^2+3x+2x+6=0] | Collect like terms [x^2+5x+6=0] | "
              "So the equation is [x^2+5x+6=0]");
    A(ACT_AUTO, "x=1/2 and x=-3");
    CHECK_STR(steps_text(),
              "Each solution gives a bracket [x-1/2=0, x+3=0] | Multiply by 2 so there are no fractions [2x-1=0] | "
              "Multiply the brackets [(2x-1)(x+3)=0] | Multiply out the brackets [2x^2+6x-x-3=0] | "
              "Collect like terms [2x^2+5x-3=0] | So the equation is [2x^2+5x-3=0]");
    A(ACT_AUTO, "x=0 and x=5");
    CHECK_STR(steps_text(),
              "Each solution gives a bracket [x=0, x-5=0] | Multiply the brackets [x(x-5)=0] | "
              "Multiply each term in the bracket by x [x^2-5x=0] | So the equation is [x^2-5x=0]");
    A(ACT_AUTO, "x=3\xC2\xB1\xE2\x88\x9A" "5");
    CHECK_STR(steps_text(),
              "Each solution gives a bracket [x-3-\xE2\x88\x9A" "5=0, x-3+\xE2\x88\x9A" "5=0] | "
              "Multiply the brackets [(x-3-\xE2\x88\x9A" "5)(x-3+\xE2\x88\x9A" "5)=0] | "
              "Use the difference of two squares [(x-3)^2-(\xE2\x88\x9A" "5)^2=0] | Square the surd [(x-3)^2-5=0] | "
              "Multiply out each bracket [x^2-6x+9-5=0] | >Expand (x-3)^2 [x^2-6x+9] | "
              ">>Write the power as repeated brackets [(x-3)(x-3)] | >>Multiply out the brackets [x^2-3x-3x+9] | "
              ">>Collect like terms [x^2-6x+9] | Collect like terms [x^2-6x+4=0] | So the equation is [x^2-6x+4=0]");
    A(ACT_AUTO, "x=2\xE2\x88\x9A" "3 and x=-2\xE2\x88\x9A" "3");
    CHECK_STR(steps_text(),
              "Each solution gives a bracket [x-2\xE2\x88\x9A" "3=0, x+2\xE2\x88\x9A" "3=0] | "
              "Multiply the brackets [(x-2\xE2\x88\x9A" "3)(x+2\xE2\x88\x9A" "3)=0] | "
              "Use the difference of two squares [x^2-(2\xE2\x88\x9A" "3)^2=0] | Square the surd [x^2-12=0] | "
              "So the equation is [x^2-12=0]");
}

static void errors(void)
{
    AUTO("x=\xE2\x88\x9A" "2 and x=3", "ERR:x=\xE2\x88\x9A" "2 needs its partner x=-\xE2\x88\x9A" "2: surd solutions come in pairs");
    AUTO("x=\xCF\x80 and x=2", "ERR:Each solution must be a number, a fraction or a surd");
    AUTO("x=\xE2\x88\x9A" "2+\xE2\x88\x9A" "3 and x=1", "ERR:Each solution can only have one surd in it");
    AUTO("x=2, x=3\xC2\xB1" "1\xC2\xB1" "1", "ERR:Use \xC2\xB1 only once in each solution");
    AUTO("x=1, x=2, x=3, x=4, x=5, x=6, x=7, x=8, x=9", "ERR:Too many solutions: 8 at most");
}

static void unchanged(void)
{
    AUTO("x=2", "x=2");
    AUTO("x=-2", "x=-2");
    AUTO("2x+3y=12, x-y=1", "x=3, y=2");
    AUTO("2x+3y=12 and x-y=1", "x=3, y=2");
    AUTO("x=2, y=3", "x=2, y=3");
    AUTO("x=y+1, x=3", "x=3, y=2");
    AUTO("x=y+1 and x=3", "x=3, y=2");             /* now read like its comma form */
    AUTO("-3<2x+1<=7", "-2<x\xE2\x89\xA4" "3");
    AUTO("x^2+5x+6=0", "x=-3 \"or\" x=-2");
    AUTO("2x+1=7", "x=3");
    CHECK_STR(A(ACT_EXPAND, "(x+2)(x+3)"), "x^2+5x+6");
    CHECK_STR(A(ACT_FACTOR, "x^2+5x+6"), "(x+2)(x+3)");
    /* "and" / "or" stay letters unless they sit between two relations */
    AUTO("2a+n", "2a+n");
    AUTO("and", "adn");
    AUTO("a=and", "ERR:Which letter should I solve for?");
    A(ACT_AUTO, "x=or+1, y=2");
    CHECK_STR(R.input_echo, "x=or+1, y=2");
    A(ACT_AUTO, "x= -2 and x= -3");
    CHECK_STR(R.input_echo, "x=-2, x=-3");
    /* glued to the next letter (typed on the pad with no spaces): still a separator */
    AUTO("x=-2andx=-3", "x^2+5x+6=0");
    AUTO("x=-2orx=-3", "x^2+5x+6=0");
    AUTO("X=-2and x=-3", "x^2+5x+6=0");
    AUTO("2x+3y=12andx-y=1", "x=3, y=2");
    A(ACT_AUTO, "x=-2andx=-3");
    CHECK_STR(R.input_echo, "x=-2, x=-3");
    AUTO("andx", "adnx");                            /* no relation either side: letters */
    A(ACT_AUTO, "x=orx+1, y=2");
    CHECK_STR(R.input_echo, "x=orx+1, y=2");
}

int main(void)
{
    answers();
    working();
    errors();
    unchanged();
    return test_finish();
}
