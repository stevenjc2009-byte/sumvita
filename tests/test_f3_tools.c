/* Regression tests for the GCSE tool fixes (F3). Each case failed before the fix.
 * RUN fills the fields as the user would; ok() checks text in answer / alt and that
 * every line parses; err() checks the wording of the error. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"
#include "engine/expr.h"
#include "tools/tools.h"
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
#define RUN(...) run(__VA_ARGS__, NULL)

static void parses(const char *id, const char *line)
{
    char err[96];
    int pos;
    if (!line || !line[0]) return;
    CHECK(ex_parse(line, err, sizeof err, &pos) != NULL, "%s: \"%s\" does not parse: %s", id, line, err);
}

static void ok(const char *id, const char *a, const char *b)
{
    CHECK(g_o.ok, "%s: failed: %s", id, g_o.error);
    if (!g_o.ok) return;
    CHECK(!a[0] || strstr(g_o.answer, a), "%s: answer \"%s\" lacks \"%s\"", id, g_o.answer, a);
    CHECK(!b[0] || strstr(g_o.answer_alt, b), "%s: alt \"%s\" lacks \"%s\"", id, g_o.answer_alt, b);
    parses(id, g_o.answer);
    parses(id, g_o.answer_alt);
    for (int i = 0; i < g_o.steps.n; i++) parses(id, g_o.steps.v[i].math);
}

static bool has_step(const char *needle)
{
    for (int i = 0; i < g_o.steps.n; i++)
        if (strstr(g_o.steps.v[i].math, needle) || strstr(g_o.steps.v[i].note, needle) ||
            strstr(g_o.steps.v[i].title, needle)) return true;
    return false;
}

static void err(const char *id, const char *want)
{
    CHECK(!g_o.ok && g_o.error[0], "%s: should fail (answer \"%s\")", id, g_o.answer);
    CHECK(!want[0] || strstr(g_o.error, want), "%s: error \"%s\" lacks \"%s\"", id, g_o.error, want);
}

static int sum_list(const char *s)
{
    int t = 0;
    for (const char *p = s; *p; p++)
        if (*p >= '0' && *p <= '9') {
            t += atoi(p);
            while (p[1] >= '0' && p[1] <= '9') p++;
        }
    return t;
}

static void titled(const char *id)
{
    for (int i = 0; i < g_o.steps.n; i++) CHECK(g_o.steps.v[i].title && g_o.steps.v[i].title[0], "%s: step %d has no title", id, i + 1);
}

static void tests(void)
{
    /* 1 */
    RUN("conv_hours", "decimal hours to h and min", "0.999"); ok("conv_hours", "1 \"h\" 0 \"min\"", "");
    RUN("conv_hours", "decimal hours to h and min", "2.75"); ok("conv_hours", "2 \"h\" 45 \"min\"", "");
    /* 2 */
    RUN("prob_cond", "1/10", "1/4"); ok("prob_cond", "2/5", "");
    CHECK(has_step("(1/10)÷(1/4)=2/5"), "prob_cond working line");
    /* 3, 4 */
    RUN("stratified", "35,65", "10"); ok("stratified", "", "");
    CHECK(sum_list(g_o.answer) == 10, "stratified 35,65 total %d (%s)", sum_list(g_o.answer), g_o.answer);
    RUN("stratified", "120,80,200", "40"); ok("stratified", "12", "");
    RUN("stratified", "120,80,200", "500"); err("stratified", "larger than the population");
    /* 5, 6 */
    RUN("sine_side", "40", "-9", "65", "cm"); err("sine_side", "must be positive");
    RUN("sine_side", "100", "5", "80", "cm"); err("sine_side", "no triangle");
    RUN("sine_angle", "9", "120", "12"); err("sine_angle", "");
    /* 7 */
    RUN("distance", "1", "2", "5", "10"); ok("distance", "8.94", "4√5");
    RUN("distance", "0", "0", "3", "3"); ok("distance", "", "3√2");
    RUN("pythag", "hypotenuse", "4", "8", "cm"); ok("pythag", "8.94", "4√5");
    /* 8 */
    RUN("soh_side", "52", "8", "adjacent", "hypotenuse", "cm"); ok("soh_side", "", "");
    /* 9 */
    RUN("pct_adjust", "-5", "10", "increase"); err("pct_adjust", "negative");
    RUN("simple_int", "-800", "4", "3"); err("simple_int", "negative");
    RUN("vat", "-5", "20", "add"); err("vat", "negative");
    RUN("compound", "-5", "3", "2", "growth"); err("compound", "negative");
    /* 10 */
    RUN("freq_density", "frequency", "10", "30", "-1"); err("freq_density", "negative");
    /* 11 */
    RUN("stat_grouped", "0,5,10", "10,20,30", "1,2,3"); err("stat_grouped", "");
    RUN("stat_grouped", "0,10,20", "10,20,30", "1,2,3"); ok("stat_grouped", "", "");
    /* 12 */
    RUN("exact_trig", "50"); err("exact_trig", "0°, 30°, 45°, 60° and 90°");
    /* 13 */
    RUN("conv_length", "-2", "m", "cm"); err("conv_length", "negative");
    RUN("conv_capacity", "-1", "litre", "ml"); err("conv_capacity", "negative");
    RUN("conv_area", "-4", "m²", "cm²"); err("conv_area", "negative");
    RUN("conv_speed", "-5", "km/h", "m/s"); err("conv_speed", "negative");
    RUN("conv_time", "-1", "hour", "minute"); err("conv_time", "negative");
    RUN("conv_density", "-2", "g/cm³", "kg/m³"); err("conv_density", "negative");
    RUN("conv_temp", "-40", "°C", "°F"); ok("conv_temp", "-40", "");
    /* 14 */
    RUN("frac_of", "5/6", "100"); ok("frac_of", "83.3", "250/3");
    RUN("frac_of", "3/4", "100"); ok("frac_of", "75", "");
    RUN("ratio_share", "100", "1,2"); ok("ratio_share", "33.33", "100/3");
    RUN("ratio_share", "60", "2,3"); ok("ratio_share", "24", "");
    RUN("prob_expected", "1/6", "100"); ok("prob_expected", "16.7", "50/3");
    RUN("prob_expected", "0.2", "50"); ok("prob_expected", "10", "");
    /* 15 */
    RUN("capture", "30", "50", "60"); err("capture", "can't be more than the second catch");
    RUN("capture", "30", "50", "-1"); err("capture", "must be positive");
    /* 16 */
    RUN("compound", "5000", "-150", "2", "growth"); err("compound", "Rate");
    RUN("compound", "5000", "100", "2", "decay"); err("compound", "Rate");
    RUN("cosine_side", "7", "10", "200", "cm"); err("cosine_side", "Angle");
    RUN("area_sin", "8", "-11", "40", "cm"); err("area_sin", "must be positive");
    /* 17 */
    RUN("nth_quad", "1,2,3,4"); ok("nth_quad", "n", "linear");
    CHECK(has_step("linear"), "nth_quad: says the sequence is linear");
    RUN("nth_quad", "4,9,16,25,36"); ok("nth_quad", "n", "");
    /* 18 */
    RUN("midpoint", "0", "0", "5", "7"); ok("midpoint", "5/2", "(2.5, 3.5)");
    RUN("midpoint", "1", "2", "5", "10"); ok("midpoint", "3", "");
    /* cosmetic */
    RUN("stat_list", "5,8,9,-3,10"); ok("stat_list", "", "");
    CHECK(has_step("(-3+5+8+9+10)/5=29/5=5.8"), "stat_list mean shows the sum");
    RUN("tree_with", "3", "8", "one of each"); ok("tree_with", "", ""); titled("tree_with");
    RUN("tree_without", "3", "8", "one of each"); ok("tree_without", "", ""); titled("tree_without");
    RUN("conv_speed", "72", "km/h", "m/s"); ok("conv_speed", "20", "");
    CHECK(has_step("÷3.6"), "conv_speed uses ÷3.6");
    RUN("polygon", "exterior angle", "36"); ok("polygon", "", "\"Sides\"=10");
    RUN("bounds_calc", "12", "1", "a÷b", "4", "1"); ok("bounds_calc", "3.5714", "");
    CHECK(!strstr(g_o.answer, "3.5714286"), "bounds_calc a÷b shown to 7 d.p.");
}

int main(void)
{
    g_set.dp = -1;
    g_set.sf = 0;
    g_set.degrees = true;
    tests();
    return test_finish();
}
