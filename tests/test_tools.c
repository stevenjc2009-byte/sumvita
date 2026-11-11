/* GCSE toolkit tests. One representative case per tool, checked against a
 * value worked by hand (source in the comment). Each case fills the fields as
 * the user would, runs the tool, and checks that the expected text appears in
 * the answer or its alternative form, and that the working is not empty. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"
#include "engine/expr.h"
#include "tools/tools.h"
#include "ui/note_text.h"
#include <stdarg.h>
#include <stdlib.h>

static CalcSettings g_set;
#define T(...) T_(__VA_ARGS__, NULL)
#define E(...) E_(__VA_ARGS__, NULL)

/* text=value lines and the rest must parse ("HCF"=6, "Area"=12 "cm²", ...) and print back */
static void rt_line(const char *id, const char *line)
{
    if (!line || !line[0]) return;
    char err[96];
    int pos;
    Expr *e = ex_parse(line, err, sizeof err, &pos);
    CHECK(e != NULL, "%s: \"%s\" does not parse: %s @%d", id, line, err, pos);
    if (!e) return;
    const char *once = ex_print(e);
    Expr *again = ex_parse(once, err, sizeof err, &pos);
    CHECK(again && strcmp(ex_print(again), once) == 0, "%s: \"%s\" does not print back stably", id, line);
}

/* T(id, want1, want2, want3, "field1", "field2", ..., NULL). Wants may be "". */
static void T_(const char *id, const char *w1, const char *w2, const char *w3, ...)
{
    const Tool *t = tools_find(id);
    if (!t) { CHECK(0, "tool %s not found", id); return; }
    const char *vals[TOOL_MAX_FIELDS];
    for (int i = 0; i < TOOL_MAX_FIELDS; i++) vals[i] = "";
    va_list ap;
    va_start(ap, w3);
    for (int i = 0; i < t->nfields; i++) {
        const char *v = va_arg(ap, const char *);
        if (!v) break;
        vals[i] = v;
    }
    va_end(ap);
    arena_reset(arena_current());
    CalcResult o;
    memset(&o, 0, sizeof o);
    t->run(vals, &g_set, &o);
    if (!o.ok) { CHECK(0, "%s failed: %s", id, o.error); return; }
    char hay[600];
    snprintf(hay, sizeof hay, "%s | %s", o.answer, o.answer_alt);
    const char *w[3] = { w1, w2, w3 };
    for (int i = 0; i < 3; i++)
        CHECK(!w[i][0] || strstr(hay, w[i]), "%s: want \"%s\" in \"%s\"", id, w[i], hay);
    CHECK(o.steps.n > 0, "%s: no working", id);
    /* every line the UI will lay out is display syntax: it parses and prints back */
    rt_line(id, o.answer);
    rt_line(id, o.answer_alt);
    for (int i = 0; i < o.steps.n; i++) rt_line(id, o.steps.v[i].math);
    /* titles and notes as the UI shows them (note_text): no ASCII maths left */
    for (int i = 0; i < o.steps.n; i++)
        for (int k = 0; k < 2; k++) {
            char *nt = note_text(k ? o.steps.v[i].note : o.steps.v[i].title);
            CHECK(nt && !strchr(nt, '^') && !strchr(nt, '*') && !strstr(nt, "sqrt("), "%s: step %d \"%s\"", id, i, nt);
            free(nt);
        }
}

static void E_(const char *id, ...)   /* must fail with an error message */
{
    const Tool *t = tools_find(id);
    if (!t) { CHECK(0, "tool %s not found", id); return; }
    const char *vals[TOOL_MAX_FIELDS];
    for (int i = 0; i < TOOL_MAX_FIELDS; i++) vals[i] = "";
    va_list ap;
    va_start(ap, id);
    for (int i = 0; i < t->nfields; i++) {
        const char *v = va_arg(ap, const char *);
        if (!v) break;
        vals[i] = v;
    }
    va_end(ap);
    arena_reset(arena_current());
    CalcResult o;
    memset(&o, 0, sizeof o);
    t->run(vals, &g_set, &o);
    CHECK(!o.ok && o.error[0], "%s: bad input should give an error", id);
}

static void table_checks(void)
{
    int n;
    const Tool *all = tools_all(&n);
    CHECK(n >= 60, "only %d tools", n);
    for (int i = 0; i < n; i++) {
        CHECK(all[i].run && all[i].nfields > 0 && all[i].nfields <= TOOL_MAX_FIELDS, "tool %s malformed", all[i].id);
        for (int j = i + 1; j < n; j++) CHECK(strcmp(all[i].id, all[j].id) != 0, "duplicate id %s", all[i].id);
        CHECK(tools_find(all[i].id) == &all[i], "find %s", all[i].id);
    }
    const char *cats[16];
    int nc = tools_categories(cats, 16);
    CHECK(nc == 7, "categories = %d", nc);
    CHECK(tools_find("no_such_tool") == NULL, "find unknown");
}

static void number_tests(void)
{
    T("pct_of", "36", "", "", "15", "240");                               /* 0.15 x 240 */
    T("pct_change", "25", "increase", "", "80", "100");                   /* 20/80 */
    T("pct_reverse", "75", "", "", "90", "20", "increase");               /* 90 / 1.2 */
    T("pct_adjust", "220", "", "", "250", "12", "decrease");              /* 250 x 0.88 */
    T("compound", "5737.62", "", "", "5000", "3.5", "4", "growth");       /* 5000 x 1.035^4 */
    T("simple_int", "96", "", "", "800", "4", "3");                       /* 800 x 0.04 x 3 */
    T("vat", "72", "", "", "60", "20", "add");
    T("vat", "60", "", "", "72", "20", "remove");                         /* 72 / 1.2 */
    T("hcf_lcm", "6", "1260", "", "84", "90");                            /* 84=2^2.3.7, 90=2.3^2.5 */
    T("bounds", "35", "45", "", "40", "10");                              /* 35 <= x < 45 */
    T("bounds_calc", "18.7425", "17.7625", "", "7.3", "0.1", "a×b", "2.5", "0.1");   /* 7.35 x 2.55, 7.25 x 2.45 */
    T("round", "3.14", "", "", "3.14159", "dp", "2");
    T("round", "0.0046", "", "", "0.004567", "sf", "2");
    T("estimate", "300", "", "", "48.7", "3.12", "0.52");                 /* 50 x 3 / 0.5 */
    T("frac_arith", "19/12", "", "", "3/4", "+", "5/6");                  /* 9/12 + 10/12 */
    T("frac_of", "45", "", "", "3/8", "120");                             /* 120/8 x 3 */
    E("pct_of", "abc", "240");
}

static void ratio_tests(void)
{
    T("ratio_simplify", "2:3:5", "", "", "12, 18, 30");                   /* divide by 6 */
    T("ratio_share", "24", "36", "", "60", "2, 3");                       /* 60/5 = 12 */
    T("ratio_combine", "8:12:15", "", "", "2, 3", "4, 5");                /* 2:3 x4, 4:5 x3 */
    T("ratio_known", "24", "40", "16", "3, 5, 2", "2", "40");             /* 40/5 = 8 per part */
    T("best_buy", "2", "", "", "400", "1.80", "750", "3.15");             /* 0.45/100 vs 0.42/100 */
    T("prop_direct", "33.3", "", "", "x^2", "3", "12", "y", "5");         /* k = 4/3, y = 4/3 x 25 */
    T("prop_inverse", "3", "", "", "x", "4", "6", "y", "8");              /* k = 24 */
    T("speed", "60", "", "", "150", "2.5", "", "km, h, km/h");
    T("density", "2.7", "", "", "540", "200", "", "g, cm³, g/cm³");
    T("pressure", "1500", "", "", "600", "0.4", "", "N, m², N/m²");
    T("scale", "9", "27", "", "3", "length");                             /* 3^2, 3^3 */
}

static void geometry_tests(void)
{
    T("pythag", "5", "", "", "hypotenuse", "3", "4", "cm");
    T("pythag", "12", "", "", "shorter side", "5", "13", "cm");           /* 169-25 = 144 */
    T("pythag3d", "10.2", "", "", "8", "5", "4", "cm");                   /* sqrt105 */
    T("area_rect", "40", "26", "", "8", "5", "cm");
    T("area_tri", "30", "", "", "10", "6", "cm");
    T("area_para", "36", "", "", "9", "4", "cm");
    T("area_trap", "32", "", "", "6", "10", "4", "cm");                   /* (6+10)/2 x 4 */
    T("area_circle", "78.5", "25π", "", "5", "radius", "cm");
    T("polygon", "1080", "135", "45", "number of sides", "8");            /* (8-2)x180 */
    T("bearing", "245", "", "", "065");                                   /* 65 + 180 */
    T("arc_sector", "7.85", "23.6", "", "6", "75", "cm");                 /* 75/360 x 2pi6, x pi36 */

    T("vol_prism", "240", "", "", "24", "10", "cm");
    T("vol_cyl", "452", "144π", "", "4", "9", "cm");                      /* pi x 16 x 9 */
    T("vol_cone", "66", "21π", "", "3", "7", "cm");                     /* 1/3 pi 9 7 */
    T("vol_sphere", "905", "288π", "", "6", "sphere", "cm");              /* 4/3 pi 216 */
    T("vol_sphere", "18π", "", "", "3", "hemisphere", "cm");              /* 2/3 pi 27 */
    T("vol_pyr", "72", "", "", "6", "4", "9", "cm");                      /* 1/3 x 24 x 9 */
    T("vol_frustum", "528", "168π", "", "6", "3", "8", "cm");             /* 1/3 pi 8 (36+18+9) */
    T("cuboid", "120", "158", "", "8", "5", "3", "cm");                   /* 2(40+24+15) */
    T("surf_cyl", "104π", "", "", "4", "9", "cm");                        /* 2pi4(9+4) */
    T("surf_cone", "24π", "", "", "3", "4", "cm");                        /* l=5: pi3(3+5) */
    T("surf_sphere", "100π", "", "", "5", "sphere", "cm");
    T("surf_sphere", "75π", "", "", "5", "hemisphere", "cm");             /* 3 pi r^2 */

    T("soh_side", "6.88", "", "", "35", "12", "hypotenuse", "opposite", "cm");  /* 12 sin35 */
    T("soh_angle", "38.7", "", "", "opp & hyp", "5", "8");                /* sin^-1(5/8) */
    T("sine_side", "12.7", "", "", "40", "9", "65", "cm");                /* 9 sin65/sin40 */
    T("sine_angle", "59.0", "121.0", "", "9", "40", "12");                /* sin^-1(12 sin40/9) */
    T("cosine_side", "7.68", "", "", "7", "10", "50", "cm");              /* sqrt(149-140cos50) */
    T("cosine_angle", "96.4", "", "", "7", "9", "12");                    /* (49+81-144)/126 */
    T("area_sin", "28.3", "", "", "8", "11", "40", "cm");                 /* 44 sin40 */
    T("exact_trig", "1/2", "", "", "30");
    T("exact_trig", "undefined", "", "", "90");
}

static void prob_tests(void)
{
    T("prob_not", "13/20", "", "", "0.35");
    T("prob_combined", "3/20", "", "", "0.3", "0.5", "independent: A and B");
    T("prob_combined", "13/20", "", "", "0.3", "0.5", "independent: A or B");   /* .8-.15 */
    T("prob_combined", "4/5", "", "", "0.3", "0.5", "mutually exclusive: A or B");
    T("prob_expected", "30", "", "", "0.15", "200");
    T("prob_relative", "3/10", "", "", "18", "60");
    T("tree_with", "9/64", "", "", "3", "8", "two of type A");
    T("tree_with", "15/32", "", "", "3", "8", "one of each");              /* 2 x 3/8 x 5/8 */
    T("tree_without", "3/28", "", "", "3", "8", "two of type A");          /* 3/8 x 2/7 */
    T("tree_without", "9/14", "", "", "3", "8", "at least one of type A"); /* 1 - 5/8 x 4/7 */
    T("prob_cond", "2/5", "", "", "1/10", "1/4");
    T("prob_venn", "7/10", "3/10", "", "0.5", "0.4", "0.2");               /* .9-.2, 1-.7 */
    T("counting", "120", "", "", "nCr", "10", "3");
    T("counting", "720", "", "", "nPr", "10", "3");
    T("counting", "120", "", "", "n!", "5", "");
    {   /* every working line of n! is an equation, never "5 != 120" (not equal) */
        const char *vals[TOOL_MAX_FIELDS] = {"n!", "5", "", "", "", "", "", ""};
        CalcResult o;
        memset(&o, 0, sizeof o);
        tools_find("counting")->run(vals, &g_set, &o);
        for (int i = 0; i < o.steps.n; i++) {
            Expr *e = o.steps.v[i].math[0] ? ex_parse(o.steps.v[i].math, NULL, 0, NULL) : NULL;
            CHECK(!e || e->kind != E_REL || e->op == R_EQ, "counting step \"%s\" is not an equation", o.steps.v[i].math);
        }
    }
}

static void stats_tests(void)
{
    T("stat_list", "7.8", "", "", "4, 7, 7, 9, 12");                       /* 39/5 */
    T("stat_freq", "2.25", "", "", "1, 2, 3, 4", "5, 8, 4, 3");            /* 45/20 */
    T("stat_grouped", "16.5", "", "", "0, 10, 20", "10, 20, 30", "4, 9, 7");   /* 330/20 */
    T("stat_quartiles", "7", "", "", "3, 5, 6, 8, 9, 12, 14");             /* IQR 12-5 */
    T("freq_density", "1.2", "", "", "frequency density", "10", "30", "24");
    T("freq_density", "24", "", "", "frequency", "10", "30", "1.2");
    T("pie_angles", "72", "108", "36", "12, 18, 6, 24");                   /* 360/60 = 6 per item */
    T("stratified", "12", "8", "20", "120, 80, 200", "40");                /* 40/400 = 0.1 */
    T("capture", "250", "", "", "30", "50", "6");                          /* 30 x 50 / 6 */
    T("moving_avg", "12.67", "14.67", "16.33", "12, 15, 11, 18, 20", "3");
}

static void convert_tests(void)
{
    T("conv_length", "250", "", "", "2.5", "m", "cm");
    T("conv_mass", "2500", "", "", "2.5", "kg", "g");
    T("conv_capacity", "2500", "", "", "2.5", "litre", "ml");
    T("conv_area", "25000", "", "", "2.5", "m²", "cm²");                   /* x 10000 */
    T("conv_volume", "2500000", "", "", "2.5", "m³", "cm³");               /* x 10^6 */
    T("conv_speed", "20", "", "", "72", "km/h", "m/s");
    T("conv_time", "150", "", "", "2.5", "hour", "minute");
    T("conv_density", "2700", "", "", "2.7", "g/cm³", "kg/m³");
    T("conv_temp", "212", "", "", "100", "°C", "°F");                      /* 100 x 9/5 + 32 */
    T("conv_currency", "292.5", "", "", "250", "1.17", "£ to foreign");
    T("conv_hours", "2", "45", "", "decimal hours to h and min", "2.75", "");
    T("conv_hours", "2.75", "", "", "h and min to decimal hours", "2", "45");
}

static void algebra_tests(void)
{
    T("nth_linear", "3n+2", "", "", "5, 8, 11, 14");
    T("nth_quad", "n^2+2n+1", "", "", "4, 9, 16, 25, 36");
    T("geometric", "3×2^(n-1)", "48", "", "3, 6, 12, 24");
    T("iteration", "1.8", "", "", "(3x+1)^(1/3)", "1", "5");                /* settles near 1.32 */
    T("trapezium", "26", "", "", "x^2+1", "0", "4", "4");
    T("gradient", "2", "", "", "1", "2", "5", "10");                       /* 8/4 */
    T("midpoint", "3", "6", "", "1", "2", "5", "10");
    T("distance", "8.94", "", "", "1", "2", "5", "10");                    /* sqrt80 */
    T("line2", "2x", "", "", "1", "2", "5", "10");                         /* y = 2x */
    T("par_perp", "-(1/2)x+3", "", "", "perpendicular", "2", "4", "1");         /* m=-1/2 */
}

int main(void)
{
    g_set.dp = -1;
    g_set.sf = 0;
    g_set.degrees = true;
    table_checks();
    number_tests();
    ratio_tests();
    geometry_tests();
    prob_tests();
    stats_tests();
    convert_tests();
    algebra_tests();
    return test_finish();
}
