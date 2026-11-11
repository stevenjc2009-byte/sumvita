/* calc_run exact trig: every multiple of 30 and 45 from 0 to 360 (sin, cos,
 * tan), inverse trig of the exact values, radians, and the working step. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"

static CalcSettings S;
static CalcResult R;

static const char *A(const char *in)
{
    calc_run(ACT_EVAL, in, &S, &R);
    return R.ok ? R.answer : asprintf_a("ERR:%s", R.error);
}

#define ERR "ERR"
typedef struct { int deg; const char *sin, *cos, *tan; } Row;

static const Row TABLE[] = {
    {0, "0", "1", "0"},
    {30, "1/2", "√3/2", "√3/3"},
    {45, "√2/2", "√2/2", "1"},
    {60, "√3/2", "1/2", "√3"},
    {90, "1", "0", ERR},
    {120, "√3/2", "-1/2", "-√3"},
    {135, "√2/2", "-√2/2", "-1"},
    {150, "1/2", "-√3/2", "-√3/3"},
    {180, "0", "-1", "0"},
    {210, "-1/2", "-√3/2", "√3/3"},
    {225, "-√2/2", "-√2/2", "1"},
    {240, "-√3/2", "-1/2", "√3"},
    {270, "-1", "0", ERR},
    {300, "-√3/2", "1/2", "-√3"},
    {315, "-√2/2", "√2/2", "-1"},
    {330, "-1/2", "√3/2", "-√3/3"},
    {360, "0", "1", "0"},
};

static void check(const char *fn, int deg, const char *want)
{
    const char *got = A(asprintf_a("%s(%d)", fn, deg));
    if (strcmp(want, ERR) == 0) {
        CHECK(strncmp(got, "ERR:Maths error", 15) == 0, "%s(%d) should be a maths error, got %s", fn, deg, got);
    } else {
        t_total++;
        if (strcmp(got, want) != 0) {
            t_fail++;
            printf("FAIL %s(%d): got \"%s\" want \"%s\"\n", fn, deg, got, want);
        }
    }
}

static void table(void)
{
    for (size_t i = 0; i < sizeof TABLE / sizeof TABLE[0]; i++) {
        check("sin", TABLE[i].deg, TABLE[i].sin);
        check("cos", TABLE[i].deg, TABLE[i].cos);
        check("tan", TABLE[i].deg, TABLE[i].tan);
        arena_reset(arena_current());
    }
}

#define INV(s) "\xE2\x81\xBB\xC2\xB9"

static void inverse_and_more(void)
{
    CHECK_STR(A("sin" INV() "(1/2)"), "30");
    CHECK_STR(A("cos" INV() "(1/2)"), "60");
    CHECK_STR(A("tan" INV() "(1)"), "45");
    CHECK_STR(A("tan" INV() "(√3)"), "60");
    CHECK_STR(A("sin" INV() "(√2/2)"), "45");
    CHECK_STR(A("sin" INV() "(√3/2)"), "60");
    CHECK_STR(A("cos" INV() "(-1/2)"), "120");
    CHECK_STR(A("sin" INV() "(-1/2)"), "-30");
    CHECK_STR(A("cos" INV() "(0)"), "90");
    CHECK_STR(A("sin" INV() "(1)"), "90");
    CHECK_STR(A("tan" INV() "(-1)"), "-45");
    CHECK_STR(A("tan" INV() "(1/√3)"), "30");
    CHECK_STR(A("sin" INV() "(0.3)"), "17.45760312");
    CHECK_STR(A("2sin(30)"), "1");
    CHECK_STR(A("sin(30)+cos(60)"), "1");
    CHECK_STR(A("sin(45)^2"), "1/2");
    CHECK_STR(A("sin(390)"), "1/2");
    CHECK_STR(A("sin(-30)"), "-1/2");
    CHECK_STR(A("cos(-60)"), "1/2");
    CHECK_STR(A("tan(-45)"), "-1");
    CHECK_STR(A("4cos(60)"), "2");
    CHECK_STR(A("sin(20)"), "0.3420201433");
    calc_run(ACT_EVAL, "sin(150)", &S, &R);
    int found = 0;
    for (int i = 0; i < R.steps.n; i++) found |= strcmp(R.steps.v[i].math, "sin(150)=sin(30)=1/2") == 0;
    CHECK(found, "sin(150) working step");
    CHECK(strstr(A("tan(90)"), "tan 90") != NULL, "tan 90 message names the angle");
    S.degrees = false;
    CHECK_STR(A("sin(\xCF\x80/6)"), "1/2");
    CHECK_STR(A("cos(\xCF\x80)"), "-1");
    CHECK_STR(A("tan(\xCF\x80/4)"), "1");
    CHECK_STR(A("sin" INV() "(1/2)"), "\xCF\x80/6");
    CHECK_STR(A("sin(1)"), "0.8414709848");
    S.degrees = true;
}

int main(void)
{
    calc_settings_default(&S);
    table();
    inverse_and_more();
    return test_finish();
}
