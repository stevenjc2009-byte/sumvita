/* v1.2 input-handling tests (s2): command words, unicode/alt syntax, X/x, completesq equations,
 * contradictions, symbolic answers, step tidying, decimal alt, plain-English errors. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"
#include "engine/expr.h"
#include <stdlib.h>

static CalcSettings g_set;
static CalcResult g_o;

static void run(CalcAction a, const char *in)
{
    memset(&g_o, 0, sizeof g_o);
    arena_reset(arena_current());
    calc_run(a, in, &g_set, &g_o);
}

/* answer equals ans exactly */
#define ANS(act, in, ans) do { run(act, in); CHECK(g_o.ok, "%s: %s", in, g_o.error); \
    CHECK_STR(g_o.ok ? g_o.answer : "(failed)", ans); } while (0)
#define ALT(act, in, ans, alt) do { ANS(act, in, ans); CHECK_STR(g_o.answer_alt, alt); } while (0)
#define ERR(act, in, msg, pos) do { run(act, in); CHECK(!g_o.ok, "%s should fail", in); \
    CHECK_STR(g_o.error, msg); CHECK(g_o.error_pos == (pos), "%s: pos %d want %d", in, g_o.error_pos, (pos)); } while (0)
#define STEP(i, m) CHECK(g_o.steps.n > (i) && !strcmp(g_o.steps.v[i].math ? g_o.steps.v[i].math : "", m), \
    "step %d: got \"%s\"", i, g_o.steps.n > (i) && g_o.steps.v[i].math ? g_o.steps.v[i].math : "(none)")

static int steps_have(const char *needle)
{
    for (int i = 0; i < g_o.steps.n; i++)
        if (g_o.steps.v[i].math && strstr(g_o.steps.v[i].math, needle)) return 1;
    return 0;
}
static int title_have(const char *needle)
{
    for (int i = 0; i < g_o.steps.n; i++)
        if (g_o.steps.v[i].title && strstr(g_o.steps.v[i].title, needle)) return 1;
    return 0;
}

static void item1_commands(void)
{
    ANS(ACT_AUTO, "solve 2x+1=7", "x=3");
    ANS(ACT_AUTO, "Solve: 2x+1=7", "x=3");
    ANS(ACT_AUTO, "SOLVE 2x+1=7", "x=3");
    ANS(ACT_AUTO, "simplify 2x+3x", "5x");
    ANS(ACT_AUTO, "Simplify: 2x+3x", "5x");
    ANS(ACT_AUTO, "expand (x+1)(x+2)", "x^2+3x+2");
    ANS(ACT_AUTO, "factorise x^2+5x+6", "(x+2)(x+3)");
    ANS(ACT_AUTO, "factorize x^2-1", "(x+1)(x-1)");
    ANS(ACT_AUTO, "factor x^2-1", "(x+1)(x-1)");
    ANS(ACT_AUTO, "  solve   2x+1=7", "x=3");
    ANS(ACT_EVAL, "solve 2+3", "5");
    ANS(ACT_AUTO, "x²+2x+1", "x^2+2x+1");
    run(ACT_AUTO, "solving 2x=4");   /* a longer word is not a command */
    /* the letters stay in the answer (the tidy pass may sort them: 2/(gil nosv)) */
    CHECK(g_o.ok && strcmp(g_o.answer, "x=2") != 0 && strchr(g_o.answer, 'v'),
          "solving is not stripped: %s", g_o.answer);
    ERR(ACT_AUTO, "solve+", "Something is missing at the end", 6);
    ERR(ACT_EVAL, "solve 2+", "Something is missing at the end", 8);
}

static void item2_syntax(void)
{
    ANS(ACT_SOLVE, "x²-9=0", "x=±3");
    ANS(ACT_SOLVE, "x³=8", "x=2");
    ALT(ACT_EVAL, "½+¼", "3/4", "0.75");
    ALT(ACT_EVAL, "¾", "3/4", "0.75");
    ALT(ACT_EVAL, "¼", "1/4", "0.25");
    ALT(ACT_EVAL, "½", "1/2", "0.5");
    ALT(ACT_EVAL, "2½", "2_1/2", "2.5");
    ANS(ACT_EVAL, "5–3", "2");
    ANS(ACT_EVAL, "5—3", "2");
    ANS(ACT_EVAL, "–3+5", "2");
    ANS(ACT_SOLVE, "x**2=16", "x=±4");
    ANS(ACT_EVAL, "2**3", "8");
    ANS(ACT_EVAL, "3²", "9");
    ANS(ACT_EVAL, "2³+1", "9");
    ANS(ACT_SOLVE, "x–1=4", "x=5");
}

static void item3_X(void)
{
    ANS(ACT_SOLVE, "X+x=4", "x=2");
    ANS(ACT_SOLVE, "X+1=4", "X=3");
    ANS(ACT_SOLVE, "X^2=9", "X=±3");
    ANS(ACT_SIMPLIFY, "X+x", "2x");
    ANS(ACT_SIMPLIFY, "2X+3x", "5x");
    ANS(ACT_SIMPLIFY, "X+X", "2X");
    ANS(ACT_EXPAND, "(X+1)(x+2)", "x^2+3x+2");
}

static void item4_completesq(void)
{
    ANS(ACT_COMPLETE_SQ, "x^2+6x+5=0", "x=-5 \"or\" x=-1");
    ANS(ACT_COMPLETE_SQ, "x^2-4x-5=0", "x=-1 \"or\" x=5");
    CHECK(title_have("Add 2 to both sides"), "add step reads 'to both sides'");
    CHECK(!title_have("Add 2 from both sides"), "no 'Add .. from'");
    ANS(ACT_COMPLETE_SQ, "2x^2+8x+6=0", "x=-3 \"or\" x=-1");
    ANS(ACT_COMPLETE_SQ, "x^2+4x+4=0", "x=-2");
    ANS(ACT_COMPLETE_SQ, "x^2+2x+5=0", "\"no real solutions\"");
    ANS(ACT_COMPLETE_SQ, "x^2+6x+1=0", "x=-3±2√2");
    STEP(1, "(x+3)^2-9+1=0");
    STEP(2, "(x+3)^2-8=0");
    STEP(3, "(x+3)^2=8");
    STEP(4, "x+3=±√8");
    CHECK(title_have("Subtract 3 from both sides"), "subtract step");
    ANS(ACT_COMPLETE_SQ, "solve x^2+6x+5=0", "x=-5 \"or\" x=-1");
    run(ACT_COMPLETE_SQ, "x^2+6x+5");   /* expression form still works */
    CHECK(g_o.ok && strstr(g_o.answer, "(x+3)^2-4"), "expression form: %s", g_o.answer);
    ERR(ACT_COMPLETE_SQ, "x^2+1>3", "Type a quadratic like x^2+6x+5, or an equation like x^2+6x+5=0", -1);
}

static void item5_nosol(void)
{
    ANS(ACT_SOLVE, "x/(x+1)+1/x=1", "\"no solution\"");
    STEP(0, "x^2+x+1=x^2+x");
    STEP(1, "1=0");
    ANS(ACT_AUTO, "x/(x+1)+1/x=1", "\"no solution\"");
    ANS(ACT_SOLVE, "x+1=x+2", "\"no solution\"");
    CHECK(title_have("never true"), "contradiction step title");
    ANS(ACT_SOLVE, "2x+1=2x+1", "\"true for all values of x\"");
}

static void item6_symbolic(void)
{
    ANS(ACT_SOLVE, "x^2-a^2=0", "x=±a");
    ANS(ACT_SOLVE, "x^2+2ax+a^2=0", "x=-a");
    ANS(ACT_SOLVE, "(x+a)(x+b)=0", "x=-a \"or\" x=-b");
    STEP(0, "x+a=0 \"or\" x+b=0");
    ANS(ACT_SOLVE, "ax+b=0", "x=-b/a");
    ANS(ACT_AUTO, "ax+b=0", "x=-b/a");
    ANS(ACT_SOLVE, "(x-p)(x-q)=0", "x=p \"or\" x=q");
    ANS(ACT_SOLVE, "(x+2)(x-3)=0", "x=-2 \"or\" x=3");
    ANS(ACT_SOLVE, "x^2-9=0", "x=±3");
}

static void item7_steps(void)
{
    ANS(ACT_SOLVE, "(2x+1)/3=5", "x=7");
    STEP(0, "2x+1=15");
    CHECK(!title_have("Expand"), "no expand step for (2x+1)/3=5");
    ANS(ACT_SOLVE, "(5x+1)/2=(3x-1)/4", "x=-3/7");
    STEP(0, "2(5x+1)=3x-1");
    CHECK(!steps_have("(3x-1)"), "redundant brackets dropped");
    ANS(ACT_SOLVE, "(x+1)/2=3", "x=5");
    STEP(0, "x+1=6");
}

static void item8_decimal(void)
{
    ALT(ACT_SOLVE, "0.2x=1.5", "x=7.5", "x=15/2");
    ALT(ACT_SOLVE, "x^2=1.44", "x=±1.2", "x=±6/5");
    ALT(ACT_SOLVE, "x/0.5=3", "x=1.5", "x=3/2");
    ANS(ACT_SOLVE, "2x=3", "x=3/2");
    CHECK_STR(g_o.answer_alt, "");   /* no decimal typed: no alt */
    ANS(ACT_SOLVE, "x/2=3", "x=6");
}

static void item9_errors(void)
{
    ERR(ACT_EVAL, "2+", "Something is missing at the end", 2);
    ERR(ACT_EVAL, "(2+3", "A bracket is missing: add a )", 4);
    ERR(ACT_EVAL, "2+3)", "That symbol can't go there", 3);
    ERR(ACT_EVAL, "x$2", "Unknown symbol $", 1);
    ERR(ACT_EVAL, "()", "There is nothing inside the brackets", 1);
    ERR(ACT_EVAL, "|x", "The | is not closed: add another |", 2);
    ERR(ACT_EVAL, "(x]", "The brackets don't match: use ( with ) and [ with ]", 2);
    ERR(ACT_EVAL, "1.2.3", "A number can only have one decimal point", 3);
    ERR(ACT_EVAL, "2*/3", "That symbol can't go there", 2);
    ERR(ACT_EVAL, "", "Type something first", 0);
    ERR(ACT_EVAL, "sin(1,2)", "sin needs 1 value in the brackets", 0);
    ERR(ACT_EVAL, "root 2", "Put brackets after root, like root(...)", 5);
    ERR(ACT_EVAL, "\"abc", "A \" is missing to close the text", 0);
    ERR(ACT_EVAL, "recur(0.1(2)", "Type a recurring decimal like 0.1(27)", 12);
    ERR(ACT_EVAL, "a<b<c<d<e<f", "Use only one = or < sign between two sides", 9);
    static const char *bad[] = { "2+", "(2+3", "2+3)", "()" };
    for (int i = 0; i < 4; i++) {
        run(ACT_EVAL, bad[i]);
        CHECK(!strstr(g_o.error, "Unexpected") && !strstr(g_o.error, "Missing"), "jargon left: %s", g_o.error);
    }
}

static void item10_messages(void)
{
    ERR(ACT_SOLVE, "2x+3y=12 x-y=1", "Type two equations separated by a comma: 2x+3y=12, x-y=1", -1);
    ANS(ACT_SOLVE, "2x+3y=12; x-y=1", "x=3, y=2");
    ANS(ACT_SOLVE, "2x+3y=12, x-y=1", "x=3, y=2");
    ANS(ACT_SOLVE, "2x+3y=12;x-y=1", "x=3, y=2");
    run(ACT_SOLVE, "x>3 and x<7");
    CHECK(!g_o.ok && strstr(g_o.error, "3<x<7"), "range hint: %s", g_o.error);
    run(ACT_SOLVE, "x^2-9=0,x>0");
    CHECK(!g_o.ok && strstr(g_o.error, "x^2-9=0") && strstr(g_o.error, "x>0"), "mixed hint: %s", g_o.error);
    run(ACT_AUTO, "x>3 and x<7");
    CHECK(!g_o.ok && strstr(g_o.error, "3<x<7"), "auto range hint: %s", g_o.error);
    ANS(ACT_SOLVE, "x+1>3", "x>2");
}

int main(void)
{
    calc_settings_default(&g_set);
    g_set.dp = -1;
    item1_commands();
    item2_syntax();
    item3_X();
    item4_completesq();
    item5_nosol();
    item6_symbolic();
    item7_steps();
    item8_decimal();
    item9_errors();
    item10_messages();
    return test_finish();
}
