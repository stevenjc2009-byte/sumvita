/* GCSE Geometry tools: Pythagoras, 2D areas, polygons, bearings, arcs and sectors. */
#include "tools_util.h"

static const char *un(const char *u) { return u ? u : ""; }

/* ---------- Pythagoras ---------- */

static const char *surd_alt(double a, double b, bool plus)
{
    if (fabs(a - round(a)) > 1e-9 || fabs(b - round(b)) > 1e-9) return "";
    const char *r = tu_surd((int64_t)llround(plus ? a * a + b * b : a * a - b * b));
    return r ? r : "";
}

RUN(run_pythag)
{
    tu_begin(o);
    double a, b;
    if (!tu_num(o, "Shorter side a", v[1], &a) || !tu_num(o, "Other side", v[2], &b)) return;
    if (a <= 0 || b <= 0) { tu_fail(o, "Sides must be positive"); return; }
    bool hyp = tu_choice("hypotenuse|shorter side", v[0]) == 0;
    double r;
    const char *alt;
    if (hyp) {
        ST("Pythagoras' theorem", "The square on the hypotenuse equals the sum of the squares on the other two sides", "c^2=a^2+b^2");
        ST("Substitute", "", "c^2=%s^2+%s^2", tu_w(a), tu_w(b));
        ST("Add", "", "c^2=%s", tu_w(a * a + b * b));
        r = sqrt(a * a + b * b);
        ST("Square root both sides", "", "c=√(%s)=%s", tu_w(a * a + b * b), tu_w(r));
        alt = surd_alt(a, b, true);
    } else {
        if (b <= a) { tu_fail(o, "The hypotenuse must be the longest side"); return; }
        ST("Pythagoras' theorem", "Rearrange to find a shorter side", "a^2=c^2-b^2");
        ST("Substitute", "c is the hypotenuse", "a^2=%s^2-%s^2", tu_w(b), tu_w(a));
        ST("Subtract", "", "a^2=%s", tu_w(b * b - a * a));
        r = sqrt(b * b - a * a);
        ST("Square root both sides", "", "a=√(%s)=%s", tu_w(b * b - a * a), tu_w(r));
        alt = surd_alt(b, a, false);
    }
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), un(v[3])), alt);
    tu_val(o, r);
}

RUN(run_pythag3d)
{
    tu_begin(o);
    double l, w, h;
    if (!tu_num(o, "Length", v[0], &l) || !tu_num(o, "Width", v[1], &w) || !tu_num(o, "Height", v[2], &h)) return;
    if (l <= 0 || w <= 0 || h <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    double d2 = l * l + w * w + h * h, r = sqrt(d2);
    ST("3D Pythagoras", "The space diagonal of a cuboid", "d^2=l^2+w^2+h^2");
    ST("Substitute", "", "d^2=%s^2+%s^2+%s^2", tu_w(l), tu_w(w), tu_w(h));
    ST("Add", "", "d^2=%s", tu_w(d2));
    ST("Square root", "", "d=√(%s)=%s", tu_w(d2), tu_w(r));
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), un(v[3])), "");
    tu_val(o, r);
}

/* ---------- areas ---------- */

static void area_out(CalcResult *o, const CalcSettings *s, double area, const char *unit)
{
    tu_ans(o, tu_u(tu_f(area, s, 3, -1), tu_unit_pow(unit, 2)), "");
    tu_val(o, area);
}

RUN(run_rect)
{
    tu_begin(o);
    double l, w;
    if (!tu_num(o, "Length", v[0], &l) || !tu_num(o, "Width", v[1], &w)) return;
    if (l <= 0 || w <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    ST("Area of a rectangle", "length × width", "A=%s×%s=%s", tu_w(l), tu_w(w), tu_w(l * w));
    ST("Perimeter", "Add all four sides: 2 × (length + width)", "P=2×(%s+%s)=%s", tu_w(l), tu_w(w), tu_w(2 * (l + w)));
    const char *u = un(v[2]);
    tu_ans(o, asprintf_a("\"Area\"=%s, \"Perimeter\"=%s", tu_u(tu_f(l * w, s, 3, -1), tu_unit_pow(u, 2)), tu_u(tu_f(2 * (l + w), s, 3, -1), u)), "");
    tu_val(o, l * w);
}

RUN(run_tri)
{
    tu_begin(o);
    double b, h;
    if (!tu_num(o, "Base", v[0], &b) || !tu_num(o, "Perpendicular height", v[1], &h)) return;
    if (b <= 0 || h <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    ST("Area of a triangle", "half × base × perpendicular height", "A=1/2×b×h");
    ST("Substitute", "", "A=1/2×%s×%s=%s", tu_w(b), tu_w(h), tu_w(b * h / 2));
    area_out(o, s, b * h / 2, un(v[2]));
}

RUN(run_para)
{
    tu_begin(o);
    double b, h;
    if (!tu_num(o, "Base", v[0], &b) || !tu_num(o, "Perpendicular height", v[1], &h)) return;
    if (b <= 0 || h <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    ST("Area of a parallelogram", "base × perpendicular height (not the slanted side)", "A=b×h");
    ST("Substitute", "", "A=%s×%s=%s", tu_w(b), tu_w(h), tu_w(b * h));
    area_out(o, s, b * h, un(v[2]));
}

RUN(run_trap)
{
    tu_begin(o);
    double a, b, h;
    if (!tu_num(o, "Parallel side a", v[0], &a) || !tu_num(o, "Parallel side b", v[1], &b) || !tu_num(o, "Perpendicular height", v[2], &h)) return;
    if (a <= 0 || b <= 0 || h <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    double r = (a + b) / 2 * h;
    ST("Area of a trapezium", "half × (sum of parallel sides) × height", "A=1/2×(a+b)×h");
    ST("Substitute", "", "A=1/2×(%s+%s)×%s=%s", tu_w(a), tu_w(b), tu_w(h), tu_w(r));
    area_out(o, s, r, un(v[3]));
}

RUN(run_circle)
{
    tu_begin(o);
    double x;
    if (!tu_num(o, "Radius or diameter", v[0], &x)) return;
    if (x <= 0) { tu_fail(o, "Must be positive"); return; }
    bool dia = tu_choice("radius|diameter", v[1]) == 1;
    double r = dia ? x / 2 : x;
    if (dia) ST("Radius is half the diameter", "", "r=%s/2=%s", tu_w(x), tu_w(r));
    ST("Area of a circle", "", "A=π×r^2=π×%s^2=%s", tu_w(r), tu_w(PI_D * r * r));
    ST("Circumference", "", "C=2×π×r=2×π×%s=%s", tu_w(r), tu_w(2 * PI_D * r));
    const char *u = un(v[2]), *u2 = tu_unit_pow(u, 2);
    const char *dec = asprintf_a("\"Area\"=%s, \"Circumference\"=%s", tu_u(tu_f(PI_D * r * r, s, 3, -1), u2), tu_u(tu_f(2 * PI_D * r, s, 3, -1), u));
    const char *pa = tu_pi(r * r), *pc = tu_pi(2 * r);
    tu_pair(o, s, dec, pa && pc ? asprintf_a("\"Area\"=%s, \"Circumference\"=%s", tu_u(pa, u2), tu_u(pc, u)) : NULL);
    tu_val(o, PI_D * r * r);
}

/* ---------- polygons, bearings, arcs ---------- */

RUN(run_polygon)
{
    tu_begin(o);
    double x, n, ext;
    if (!tu_num(o, "Value", v[1], &x)) return;
    int mode = tu_choice("number of sides|exterior angle|interior angle", v[0]);
    if (mode == 0) { n = x; ext = 360.0 / n; }
    else if (mode == 1) { ext = x; n = 360.0 / ext; }
    else { ext = 180.0 - x; n = ext > 0 ? 360.0 / ext : 0; }
    if (x <= 0 || n < 3 || fabs(n - round(n)) > 1e-6) { tu_fail(o, "That is not a regular polygon"); return; }
    n = round(n);
    ext = 360.0 / n;
    if (mode == 1) ST("Number of sides", "Exterior angles add up to 360°", "n=360/%s=%s", tu_w(x), tu_w(n));
    if (mode == 2) ST("Exterior angle", "Interior + exterior = 180° on a straight line", "180°-%s°=%s°", tu_w(x), tu_w(ext));
    if (mode == 2) ST("Number of sides", "Exterior angles add up to 360°", "n=360/%s=%s", tu_w(ext), tu_w(n));
    ST("Sum of interior angles", "(n - 2) × 180°", "(%s-2)×180=%s°", tu_w(n), tu_w((n - 2) * 180));
    ST("Each exterior angle (regular)", "360° ÷ n", "360/%s=%s°", tu_w(n), tu_w(ext));
    ST("Each interior angle (regular)", "180° - exterior angle", "180-%s=%s°", tu_w(ext), tu_w(180 - ext));
    tu_ans(o, asprintf_a("\"Sum\"=%s°, \"Interior\"=%s°, \"Exterior\"=%s°", tu_w((n - 2) * 180), tu_w(180 - ext), tu_w(ext)),
           mode ? asprintf_a("\"Sides\"=%s", tu_w(n)) : "");
    tu_val(o, (n - 2) * 180);
}

RUN(run_bearing)
{
    tu_begin(o);
    double b;
    if (!tu_num(o, "Bearing of B from A", v[0], &b)) return;
    if (b < 0 || b > 360) { tu_fail(o, "A bearing is between 0 and 360"); return; }
    if (b == 360) b = 0;
    double r = b < 180 ? b + 180 : b - 180;
    ST("Back bearing", b < 180 ? "Less than 180°: add 180°" : "180° or more: subtract 180°", "%s%s180=%s", tu_w(b), b < 180 ? "+" : "-", tu_w(r));
    bool whole = fabs(r - round(r)) < 1e-9;
    tu_ans(o, whole ? asprintf_a("%03d°", (int)llround(r)) : asprintf_a("%s°", tu_w(r)), "");
    tu_val(o, r);
}

RUN(run_arc)
{
    tu_begin(o);
    double r, th;
    if (!tu_num(o, "Radius", v[0], &r) || !tu_num(o, "Angle at the centre (°)", v[1], &th)) return;
    if (r <= 0 || th <= 0 || th > 360) { tu_fail(o, "Radius > 0 and angle from 0 to 360"); return; }
    double fr = th / 360, arc = fr * 2 * PI_D * r, area = fr * PI_D * r * r;
    ST("Fraction of the circle", "Angle ÷ 360", "%s/360", tu_w(th));
    ST("Arc length", "fraction × 2πr", "L=%s/360×2×π×%s=%s", tu_w(th), tu_w(r), tu_w(arc));
    ST("Sector area", "fraction × πr²", "A=%s/360×π×%s^2=%s", tu_w(th), tu_w(r), tu_w(area));
    ST("Sector perimeter", "Arc plus two radii", "P=%s+2×%s=%s", tu_w(arc), tu_w(r), tu_w(arc + 2 * r));
    const char *u = un(v[2]), *u2 = tu_unit_pow(u, 2);
    const char *dec = asprintf_a("\"Arc\"=%s, \"Area\"=%s, \"Perimeter\"=%s", tu_u(tu_f(arc, s, 3, -1), u), tu_u(tu_f(area, s, 3, -1), u2), tu_u(tu_f(arc + 2 * r, s, 3, -1), u));
    const char *pa = tu_pi(fr * 2 * r), *ps = tu_pi(fr * r * r);
    tu_pair(o, s, dec, pa && ps ? asprintf_a("\"Arc\"=%s, \"Area\"=%s", tu_u(pa, u), tu_u(ps, u2)) : NULL);
    tu_val(o, area);
}

static const Tool T[] = {
    TOOL("pythag", TOOL_CAT_GEOMETRY, "Pythagoras (2D)", "Find a hypotenuse or a shorter side", 4, run_pythag,
         F_CH("Find the", "hypotenuse|shorter side", "hypotenuse"), F_NUM("Shorter side a", "e.g. 3", ""),
         F_NUM("Other side (shorter b, or the hypotenuse)", "e.g. 4", ""), F_UNIT),
    TOOL("pythag3d", TOOL_CAT_GEOMETRY, "Pythagoras (3D)", "Space diagonal of a cuboid", 4, run_pythag3d,
         F_NUM("Length", "e.g. 8", ""), F_NUM("Width", "e.g. 5", ""), F_NUM("Height", "e.g. 4", ""), F_UNIT),
    TOOL("area_rect", TOOL_CAT_GEOMETRY, "Rectangle", "Area and perimeter of a rectangle", 3, run_rect,
         F_NUM("Length", "e.g. 8", ""), F_NUM("Width", "e.g. 5", ""), F_UNIT),
    TOOL("area_tri", TOOL_CAT_GEOMETRY, "Triangle area", "Half base times height", 3, run_tri,
         F_NUM("Base", "e.g. 10", ""), F_NUM("Perpendicular height", "e.g. 6", ""), F_UNIT),
    TOOL("area_para", TOOL_CAT_GEOMETRY, "Parallelogram area", "Base times perpendicular height", 3, run_para,
         F_NUM("Base", "e.g. 9", ""), F_NUM("Perpendicular height", "e.g. 4", ""), F_UNIT),
    TOOL("area_trap", TOOL_CAT_GEOMETRY, "Trapezium area", "Half the sum of parallel sides times height", 4, run_trap,
         F_NUM("Parallel side a", "e.g. 6", ""), F_NUM("Parallel side b", "e.g. 10", ""), F_NUM("Perpendicular height", "e.g. 4", ""), F_UNIT),
    TOOL("area_circle", TOOL_CAT_GEOMETRY, "Circle", "Area and circumference (exact π form too)", 3, run_circle,
         F_NUM("Radius or diameter", "e.g. 5", ""), F_CH("It is the", "radius|diameter", "radius"), F_UNIT),
    TOOL("polygon", TOOL_CAT_GEOMETRY, "Angles in polygons", "Interior and exterior angles of a polygon", 2, run_polygon,
         F_CH("Start from", "number of sides|exterior angle|interior angle", "number of sides"), F_NUM("Value", "e.g. 8", "")),
    TOOL("bearing", TOOL_CAT_GEOMETRY, "Back bearing", "Bearing of A from B, given B from A", 1, run_bearing,
         F_NUM("Bearing of B from A (°)", "e.g. 065", "")),
    TOOL("arc_sector", TOOL_CAT_GEOMETRY, "Arc length and sector area", "Arc, sector area and sector perimeter", 3, run_arc,
         F_NUM("Radius", "e.g. 6", ""), F_NUM("Angle at the centre (°)", "e.g. 75", ""), F_UNIT),
};

const Tool *tools_geometry(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
