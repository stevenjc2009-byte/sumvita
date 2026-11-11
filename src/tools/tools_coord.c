/* GCSE Algebra tools: coordinate geometry (gradient, midpoint, distance, lines). */
#include "tools_util.h"

static const char *pt(Rat x, Rat y) { return asprintf_a("(%s, %s)", tu_rs(x), tu_rs(y)); }
static const char *fr(Rat r) { return rat_is_neg(r) ? asprintf_a("(%s)", tu_rs(r)) : tu_rs(r); }

static bool get4(CalcResult *o, const char *const *v, Rat *x1, Rat *y1, Rat *x2, Rat *y2)
{
    return tu_rat(o, "x1", v[0], x1) && tu_rat(o, "y1", v[1], y1) && tu_rat(o, "x2", v[2], x2) && tu_rat(o, "y2", v[3], y2);
}

static Rat sub(Rat a, Rat b) { Rat r = a; rat_sub(a, b, &r); return r; }
static Rat mul(Rat a, Rat b) { Rat r = a; rat_mul(a, b, &r); return r; }

static const char *line_eq(Rat m, Rat c)
{
    double co[2] = { rat_to_double(m), rat_to_double(c) };
    return asprintf_a("y=%s", tu_poly(co, 1, "x"));
}

RUN(run_gradient)
{
    tu_begin(o);
    Rat x1, y1, x2, y2, m;
    if (!get4(o, v, &x1, &y1, &x2, &y2)) return;
    Rat dx = sub(x2, x1), dy = sub(y2, y1);
    ST("Gradient formula", "change in y ÷ change in x", "m=(y2-y1)/(x2-x1)");
    ST("Substitute", "", "m=(%s-%s)/(%s-%s)", fr(y2), fr(y1), fr(x2), fr(x1));
    if (rat_is_zero(dx)) { tu_text(o, "Vertical line", "The change in x is 0, so the gradient is undefined"); tu_ans(o, "undefined", ""); return; }
    ST("Subtract", "", "m=%s/%s", tu_rs(dy), tu_rs(dx));
    rat_div(dy, dx, &m);
    ST("Simplify", "", "m=%s", tu_rs(m));
    char d[64];
    rat_to_decimal_str(m, 6, d, sizeof d);
    tu_ans(o, tu_rs(m), astrdup(d));
    tu_val(o, rat_to_double(m));
}

RUN(run_midpoint)
{
    tu_begin(o);
    Rat x1, y1, x2, y2, mx, my, two = rat_int(2);
    if (!get4(o, v, &x1, &y1, &x2, &y2)) return;
    Rat sx = x1, sy = y1;
    rat_add(x1, x2, &sx);
    rat_add(y1, y2, &sy);
    rat_div(sx, two, &mx);
    rat_div(sy, two, &my);
    ST("Midpoint formula", "Average the x values and the y values", "((x1+x2)/2, (y1+y2)/2)");
    ST("Substitute", "", "((%s+%s)/2, (%s+%s)/2)", fr(x1), fr(x2), fr(y1), fr(y2));
    ST("Calculate", "", "%s", pt(mx, my));
    tu_ans(o, pt(mx, my), "");
}

RUN(run_distance)
{
    tu_begin(o);
    Rat x1, y1, x2, y2;
    if (!get4(o, v, &x1, &y1, &x2, &y2)) return;
    double dx = rat_to_double(sub(x2, x1)), dy = rat_to_double(sub(y2, y1)), d2 = dx * dx + dy * dy, d = sqrt(d2);
    ST("Distance formula", "Pythagoras on the change in x and y", "d=√((x2-x1)^2+(y2-y1)^2)");
    ST("Substitute", "", "d=√((%s)^2+(%s)^2)", tu_w(dx), tu_w(dy));
    ST("Add", "", "d=√(%s)=%s", tu_w(d2), tu_w(d));
    const char *alt = "";
    int64_t r;
    if (fabs(d2 - round(d2)) < 1e-9 && d2 > 0 && !i64_isqrt_exact((int64_t)llround(d2), &r)) alt = asprintf_a("√(%lld)", (long long)llround(d2));
    tu_pair(o, s, tu_f(d, s, 3, -1), *alt ? alt : NULL);
    tu_val(o, d);
}

RUN(run_line2)
{
    tu_begin(o);
    Rat x1, y1, x2, y2, m;
    if (!get4(o, v, &x1, &y1, &x2, &y2)) return;
    Rat dx = sub(x2, x1), dy = sub(y2, y1);
    if (rat_is_zero(dx)) {
        if (rat_is_zero(dy)) { tu_fail(o, "The two points are the same"); return; }
        ST("Vertical line", "Both points have the same x", "x=%s", tu_rs(x1));
        tu_ans(o, asprintf_a("x=%s", tu_rs(x1)), "");
        return;
    }
    rat_div(dy, dx, &m);
    ST("Gradient", "change in y ÷ change in x", "m=%s/%s=%s", tu_rs(dy), tu_rs(dx), tu_rs(m));
    Rat c = sub(y1, mul(m, x1));
    ST("Find c", "Substitute one point into y = mx + c", "%s=%s×%s+c", tu_rs(y1), fr(m), fr(x1));
    ST("Solve for c", "", "c=%s", tu_rs(c));
    const char *eq = line_eq(m, c);
    ST("Equation of the line", "", "%s", eq);
    tu_ans(o, eq, "");
    tu_val(o, rat_to_double(m));
}

RUN(run_par_perp)
{
    tu_begin(o);
    Rat m, x1, y1, m2;
    if (!tu_rat(o, "Gradient of the given line", v[1], &m) || !tu_rat(o, "Point x", v[2], &x1) || !tu_rat(o, "Point y", v[3], &y1)) return;
    bool par = tu_choice("parallel|perpendicular", v[0]) == 0;
    if (par) {
        m2 = m;
        ST("Parallel lines", "Same gradient", "m=%s", tu_rs(m2));
    } else {
        if (rat_is_zero(m)) {
            ST("Perpendicular to a horizontal line", "The new line is vertical", "x=%s", tu_rs(x1));
            tu_ans(o, asprintf_a("x=%s", tu_rs(x1)), "");
            return;
        }
        rat_inv(m, &m2);
        m2 = rat_neg(m2);
        ST("Perpendicular lines", "Gradients multiply to -1: flip and change sign", "m=-1/%s=%s", fr(m), tu_rs(m2));
    }
    Rat c = sub(y1, mul(m2, x1));
    ST("Find c", "Substitute the point into y = mx + c", "%s=%s×%s+c", tu_rs(y1), fr(m2), fr(x1));
    ST("Solve for c", "", "c=%s", tu_rs(c));
    const char *eq = line_eq(m2, c);
    ST("Equation of the line", "", "%s", eq);
    tu_ans(o, eq, "");
    tu_val(o, rat_to_double(m2));
}

#define PTS F_NUM("x1", "e.g. 1", ""), F_NUM("y1", "e.g. 2", ""), F_NUM("x2", "e.g. 5", ""), F_NUM("y2", "e.g. 10", "")

static const Tool T[] = {
    TOOL("gradient", TOOL_CAT_ALGEBRA, "Gradient between two points", "m = change in y ÷ change in x", 4, run_gradient, PTS),
    TOOL("midpoint", TOOL_CAT_ALGEBRA, "Midpoint of a line segment", "Halfway between two points", 4, run_midpoint, PTS),
    TOOL("distance", TOOL_CAT_ALGEBRA, "Distance between two points", "Length of a line segment", 4, run_distance, PTS),
    TOOL("line2", TOOL_CAT_ALGEBRA, "Line through two points", "Find y = mx + c", 4, run_line2, PTS),
    TOOL("par_perp", TOOL_CAT_ALGEBRA, "Parallel or perpendicular line", "Line through a point, parallel or perpendicular to a given gradient", 4, run_par_perp,
         F_CH("Line is", "parallel|perpendicular", "perpendicular"), F_NUM("Gradient of the given line", "e.g. 2 or -3/4", ""),
         F_NUM("Through point x", "e.g. 4", ""), F_NUM("Through point y", "e.g. 1", "")),
};

const Tool *tools_coord(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
