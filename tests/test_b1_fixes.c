/* Regression tests for the B1 fixes (bounds, pct, VAT, rounding, big stats, formatting, conversions).
 * Each case failed on v1.1.0. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"
#include "tools/tools.h"
#include "tools/tools_util.h"
#include <stdarg.h>
#include <stdlib.h>

static CalcSettings g_set;
static CalcResult g_o;

static void run(const char *id, ...)
{
    const Tool *t = tools_find(id);
    const char *vals[TOOL_MAX_FIELDS];
    for (int i = 0; i < TOOL_MAX_FIELDS; i++) vals[i] = "";
    memset(&g_o, 0, sizeof g_o);
    if (!t) { CHECK(0, "tool %s missing", id); return; }
    va_list ap;
    va_start(ap, id);
    for (int i = 0; i < t->nfields; i++) {
        const char *v = va_arg(ap, const char *);
        if (!v) break;
        vals[i] = v;
    }
    va_end(ap);
    arena_reset(arena_current());
    t->run(vals, &g_set, &g_o);
}
#define RT(...) run(__VA_ARGS__, NULL)

static void ans(const char *id, const char *want)
{
    CHECK(g_o.ok, "%s: failed: %s", id, g_o.error);
    if (g_o.ok) CHECK(strstr(g_o.answer, want), "%s: answer \"%s\" lacks \"%s\"", id, g_o.answer, want);
}

static void exact(const char *id, const char *want)
{
    CHECK(g_o.ok, "%s: failed: %s", id, g_o.error);
    if (g_o.ok) CHECK(strcmp(g_o.answer, want) == 0, "%s: answer \"%s\" is not \"%s\"", id, g_o.answer, want);
}

static void err(const char *id, const char *want)
{
    CHECK(!g_o.ok && g_o.error[0], "%s: should fail (answer \"%s\")", id, g_o.answer);
    CHECK(strstr(g_o.error, want), "%s: error \"%s\" lacks \"%s\"", id, g_o.error, want);
}

static bool has_step(const char *needle)
{
    for (int i = 0; i < g_o.steps.n; i++)
        if (strstr(g_o.steps.v[i].math, needle) || strstr(g_o.steps.v[i].note, needle) ||
            strstr(g_o.steps.v[i].title, needle)) return true;
    return false;
}

static CalcResult g_c;
static void eng(CalcAction a, const char *in)
{
    memset(&g_c, 0, sizeof g_c);
    arena_reset(arena_current());
    calc_run(a, in, &g_set, &g_c);
}

static void eng_is(const char *label, const char *want)
{
    CHECK(g_c.ok && g_c.answer && strcmp(g_c.answer, want) == 0, "%s -> \"%s\" (want \"%s\")", label,
          g_c.ok ? g_c.answer : g_c.error, want);
}

static void tests(void)
{
    /* 1 bounds with negatives */
    RT("bounds_calc", "-3", "1", "a×b", "2", "1");
    ans("bounds", "\"Upper bound\"=-3.75, \"Lower bound\"=-8.75");
    CHECK(has_step("corner"), "bounds x: working names the corners");
    RT("bounds_calc", "-3", "1", "a÷b", "2", "1");
    ans("bounds", "\"Upper bound\"=-1, \"Lower bound\"=-2.3333");
    RT("bounds_calc", "3", "1", "a÷b", "0", "1");
    err("bounds", "b could be 0");
    RT("bounds_calc", "4", "1", "a×b", "-2", "1");   /* a in [3.5,4.5], b in [-2.5,-1.5] */
    ans("bounds", "\"Upper bound\"=-5.25, \"Lower bound\"=-11.25");
    RT("bounds_calc", "3", "1", "a×b", "2", "1");
    ans("bounds", "\"Upper bound\"=8.75, \"Lower bound\"=3.75");
    /* 2 pct */
    RT("pct_adjust", "50", "150", "decrease"); err("pct_adjust", "A decrease can't be more than 100%");
    RT("pct_adjust", "50", "100", "decrease"); exact("pct_adjust", "0");
    RT("pct_adjust", "50", "20", "decrease"); exact("pct_adjust", "40");
    /* 3 vat */
    RT("vat", "60", "-100", "remove"); err("vat", "-100%");
    RT("vat", "60", "-150", "remove"); err("vat", "-100%");
    RT("vat", "120", "20", "remove"); exact("vat", "100.00");
    /* 4 round */
    RT("round", "9.96", "sf", "2"); exact("round 9.96 2sf", "10");
    RT("round", "99.96", "sf", "3"); exact("round 99.96 3sf", "100");
    RT("round", "0.0996", "sf", "2"); exact("round 0.0996 2sf", "0.10");
    RT("round", "9.94", "sf", "2"); exact("round 9.94 2sf", "9.9");
    RT("round", "3.14159", "sf", "2"); exact("round 3.14159 2sf", "3.1");
    CHECK(has_step("1 decimal place ("), "singular decimal place");
    CHECK(!has_step("1 decimal places"), "no '1 decimal places'");
    /* 5 big stats */
    RT("stat_list", "1234567890123,1234567890124");
    ans("stat_list", "\"Mean\"=1234567890123.5, \"Median\"=1234567890123.5");
    RT("stat_list", "1234567890123,1234567890124,1234567890125");
    ans("stat_list", "\"Mean\"=1234567890124, \"Median\"=1234567890124");
    RT("stat_freq", "1,2", "1234567890123,5");
    CHECK(has_step("1234567890128"), "stat_freq total frequency kept");
    RT("stat_list", "5,8,9,-3,10"); ans("stat_list", "\"Mean\"=5.8");
    /* 6 formatting */
    CHECK(strlen(tu_dp(1e100, 2)) > 100, "tu_dp 1e100 not truncated");
    CHECK(strcmp(tu_dp(NAN, 2), "0") != 0, "tu_dp NaN not 0");
    CHECK(strcmp(tu_dp(INFINITY, 2), "too big") == 0, "tu_dp inf too big");
    CHECK(strcmp(tu_sf(INFINITY, 3), "too big") == 0, "tu_sf inf too big");
    CHECK(strcmp(tu_sf(NAN, 3), "too big") == 0, "tu_sf nan too big");
    CHECK(strcmp(tu_f(INFINITY, &g_set, 4, -1), "too big") == 0, "tu_f inf too big");
    CHECK(tu_pi(1e30) == NULL, "tu_pi huge is NULL (no int64 overflow)");
    CHECK(tu_pi(-1e300) == NULL, "tu_pi huge negative is NULL");
    CHECK(tu_pi(3) && strcmp(tu_pi(3), "3\xCF\x80") == 0, "tu_pi 3");
    /* 7 engine */
    eng(ACT_TO_MIXED, "-1/2"); eng_is("mixed -1/2", "-1/2");
    eng(ACT_TO_MIXED, "-7/2"); eng_is("mixed -7/2", "-3_1/2");
    eng(ACT_TO_MIXED, "1/2"); eng_is("mixed 1/2", "1/2");
    eng(ACT_STD_FORM, "1*10^-400"); eng_is("stdform 1e-400", "1\xC3\x97" "10^-400");
    eng(ACT_STD_FORM, "25*10^-400"); eng_is("stdform 25e-400", "2.5\xC3\x97" "10^-399");
    eng(ACT_STD_FORM, "3*10^-300"); eng_is("stdform 3e-300", "3\xC3\x97" "10^-300");
}

int main(void)
{
    g_set.dp = -1;
    g_set.sf = 0;
    g_set.degrees = true;
    tests();
    return test_finish();
}
