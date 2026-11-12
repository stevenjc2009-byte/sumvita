/* GCSE Trigonometry tools: SOHCAHTOA, sine and cosine rules, area, exact values. */
#include "tools_util.h"

static const char *un(const char *u) { return u ? u : ""; }
static double rad(double deg) { return deg * PI_D / 180.0; }
static double deg(double r) { return r * 180.0 / PI_D; }

enum { HYP = 0, OPP, ADJ };

/* a triangle angle: strictly between 0° and 180°; label names it in the message */
static bool angle_ok(CalcResult *o, const char *label, double a)
{
    if (a <= 0 || a >= 180) { tu_fail(o, "%s is %s°: an angle in a triangle is between 0° and 180°", label, tu_w(a)); return false; }
    return true;
}

/* two angles of one triangle must leave something for the third */
static bool angles_fit(CalcResult *o, double A, double B)
{
    if (A + B >= 180 - 1e-9) {
        tu_fail(o, "Angles A and B add up to %s°: that is %s 180°, so there is no triangle", tu_w(A + B), A + B > 180 + 1e-9 ? "more than" : "exactly");
        return false;
    }
    return true;
}

RUN(run_soh_side)
{
    tu_begin(o);
    double th, k;
    if (!tu_num(o, "Angle (°)", v[0], &th) || !tu_num(o, "Known side", v[1], &k)) return;
    int known = tu_choice("hypotenuse|opposite|adjacent", v[2]);
    int find = tu_choice("opposite|adjacent|hypotenuse", v[3]);
    find = find == 0 ? OPP : find == 1 ? ADJ : HYP;
    if (th <= 0 || th >= 90 || k <= 0) { tu_fail(o, "Angle must be between 0° and 90°, side positive"); return; }
    if (known == find) { tu_fail(o, "The known side and the side to find must be different"); return; }
    int pair = (1 << known) | (1 << find);
    const char *fn, *form;
    int numer;
    double ratio;
    if (pair == ((1 << OPP) | (1 << HYP))) { fn = "sin"; form = "sin θ=O/H"; numer = OPP; ratio = sin(rad(th)); }
    else if (pair == ((1 << ADJ) | (1 << HYP))) { fn = "cos"; form = "cos θ=A/H"; numer = ADJ; ratio = cos(rad(th)); }
    else { fn = "tan"; form = "tan θ=O/A"; numer = OPP; ratio = tan(rad(th)); }
    const char *nm[3] = { "hypotenuse", "opposite", "adjacent" };
    tu_text(o, "Label the sides", asprintf_a("Known side is the %s; the side to find (x) is the %s.", nm[known], nm[find]));
    ST("Choose the ratio", "SOH CAH TOA: the ratio that uses these two sides", "%s", form);
    double r;
    if (find == numer) {
        ST("Substitute", "", "%s(%s°)=x/%s", fn, tu_w(th), tu_w(k));
        ST("Rearrange", "Multiply both sides by the known side", "x=%s×%s(%s°)", tu_w(k), fn, tu_w(th));
        r = k * ratio;
    } else {
        ST("Substitute", "", "%s(%s°)=%s/x", fn, tu_w(th), tu_w(k));
        ST("Rearrange", "Swap x and the ratio", "x=%s/%s(%s°)", tu_w(k), fn, tu_w(th));
        r = k / ratio;
    }
    ST("Calculate", "", "x=%s", tu_w(r));
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), un(v[4])), "");
    tu_val(o, r);
}

RUN(run_soh_angle)
{
    tu_begin(o);
    double a, b;
    if (!tu_num(o, "First side named", v[1], &a) || !tu_num(o, "Second side named", v[2], &b)) return;
    if (a <= 0 || b <= 0) { tu_fail(o, "Sides must be positive"); return; }
    int m = tu_choice("opp & hyp|adj & hyp|opp & adj", v[0]);
    double q = a / b, th;
    const char *fn = m == 0 ? "sin" : m == 1 ? "cos" : "tan";
    if (m < 2 && q >= 1) { tu_fail(o, "The hypotenuse must be the longest side"); return; }
    ST("Choose the ratio", "SOH CAH TOA", "%s θ=%s", fn, m == 0 ? "O/H" : m == 1 ? "A/H" : "O/A");
    ST("Substitute", "", "%s θ=%s/%s", fn, tu_w(a), tu_w(b));
    th = deg(m == 0 ? asin(q) : m == 1 ? acos(q) : atan(q));
    ST("Use the inverse function", "", "θ=%s^-1(%s/%s)=%s°", fn, tu_w(a), tu_w(b), tu_w(th));
    tu_ans(o, asprintf_a("%s°", tu_f(th, s, 3, 1)), "");
    tu_val(o, th);
}

RUN(run_sine_side)
{
    tu_begin(o);
    double A, a, B;
    if (!tu_num(o, "Angle A (°)", v[0], &A) || !tu_num(o, "Side a (opposite A)", v[1], &a) || !tu_num(o, "Angle B (°)", v[2], &B)) return;
    if (!tu_pos(o, "Side a", a) || !angle_ok(o, "Angle A", A) || !angle_ok(o, "Angle B", B) || !angles_fit(o, A, B)) return;
    double r = a * sin(rad(B)) / sin(rad(A));
    ST("Sine rule", "Each side over the sine of its opposite angle", "a/sin A=b/sin B");
    ST("Substitute", "", "%s/sin(%s°)=b/sin(%s°)", tu_w(a), tu_w(A), tu_w(B));
    ST("Rearrange", "Multiply both sides by sin B", "b=%s×sin(%s°)/sin(%s°)", tu_w(a), tu_w(B), tu_w(A));
    ST("Calculate", "", "b=%s", tu_w(r));
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), un(v[3])), "");
    tu_val(o, r);
}

RUN(run_sine_angle)
{
    tu_begin(o);
    double a, A, b;
    if (!tu_num(o, "Side a", v[0], &a) || !tu_num(o, "Angle A (°) opposite a", v[1], &A) || !tu_num(o, "Side b", v[2], &b)) return;
    if (!tu_pos(o, "Side a", a) || !tu_pos(o, "Side b", b) || !angle_ok(o, "Angle A", A)) return;
    double q = b * sin(rad(A)) / a;
    ST("Sine rule (angles on top)", "", "sin A/a=sin B/b");
    ST("Substitute", "", "sin(%s°)/%s=sin B/%s", tu_w(A), tu_w(a), tu_w(b));
    ST("Rearrange", "", "sin B=%s×sin(%s°)/%s=%s", tu_w(b), tu_w(A), tu_w(a), tu_w(q));
    if (q > 1) { tu_fail(o, "No such triangle: sin B would be more than 1"); return; }
    double B = deg(asin(q));
    ST("Inverse sine", "", "B=sin^-1(%s)=%s°", tu_w(q), tu_w(B));
    if (!angles_fit(o, A, B)) return;
    const char *alt = "";
    if (A + (180 - B) < 180 - 1e-9 && fabs(B - 90) > 1e-9) {
        alt = asprintf_a("\"or obtuse\" %s°", tu_f(180 - B, s, 3, 1));
        tu_text(o, "Second possible angle", asprintf_a("An obtuse angle with the same sine: 180° - %s° = %s°", tu_w(B), tu_w(180 - B)));
    }
    tu_ans(o, asprintf_a("%s°", tu_f(B, s, 3, 1)), alt);
    tu_val(o, B);
}

RUN(run_cosine_side)
{
    tu_begin(o);
    double a, b, C;
    if (!tu_num(o, "Side a", v[0], &a) || !tu_num(o, "Side b", v[1], &b) || !tu_num(o, "Angle C (°) between them", v[2], &C)) return;
    if (!tu_pos(o, "Side a", a) || !tu_pos(o, "Side b", b) || !angle_ok(o, "Angle C", C)) return;
    double c2 = a * a + b * b - 2 * a * b * cos(rad(C)), r = sqrt(c2);
    ST("Cosine rule", "Two sides and the angle between them", "c^2=a^2+b^2-2ab cos C");
    ST("Substitute", "", "c^2=%s^2+%s^2-2×%s×%s×cos(%s°)", tu_w(a), tu_w(b), tu_w(a), tu_w(b), tu_w(C));
    ST("Calculate c²", "", "c^2=%s", tu_w(c2));
    ST("Square root", "", "c=√(%s)=%s", tu_w(c2), tu_w(r));
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), un(v[3])), "");
    tu_val(o, r);
}

RUN(run_cosine_angle)
{
    tu_begin(o);
    double a, b, c;
    if (!tu_num(o, "Side a", v[0], &a) || !tu_num(o, "Side b", v[1], &b) || !tu_num(o, "Side c (opposite the angle)", v[2], &c)) return;
    if (a <= 0 || b <= 0 || c <= 0 || a + b <= c || a + c <= b || b + c <= a) { tu_fail(o, "These sides cannot make a triangle"); return; }
    double q = (a * a + b * b - c * c) / (2 * a * b), C = deg(acos(q));
    ST("Cosine rule rearranged", "To find an angle from three sides", "cos C=(a^2+b^2-c^2)/(2ab)");
    ST("Substitute", "", "cos C=(%s^2+%s^2-%s^2)/(2×%s×%s)", tu_w(a), tu_w(b), tu_w(c), tu_w(a), tu_w(b));
    ST("Calculate", "", "cos C=%s", tu_w(q));
    ST("Inverse cosine", "", "C=cos^-1(%s)=%s°", tu_w(q), tu_w(C));
    tu_ans(o, asprintf_a("%s°", tu_f(C, s, 3, 1)), "");
    tu_val(o, C);
}

RUN(run_area_sin)
{
    tu_begin(o);
    double a, b, C;
    if (!tu_num(o, "Side a", v[0], &a) || !tu_num(o, "Side b", v[1], &b) || !tu_num(o, "Angle C (°) between them", v[2], &C)) return;
    if (!tu_pos(o, "Side a", a) || !tu_pos(o, "Side b", b) || !angle_ok(o, "Angle C", C)) return;
    double r = 0.5 * a * b * sin(rad(C));
    ST("Area of a triangle", "Two sides and the angle between them", "A=1/2×a×b×sin C");
    ST("Substitute", "", "A=1/2×%s×%s×sin(%s°)=%s", tu_w(a), tu_w(b), tu_w(C), tu_w(r));
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), tu_unit_pow(un(v[3]), 2)), "");
    tu_val(o, r);
}

RUN(run_exact)
{
    tu_begin(o);
    static const char *sn[5] = { "0", "1/2", "√(2)/2", "√(3)/2", "1" };
    static const char *cs[5] = { "1", "√(3)/2", "√(2)/2", "1/2", "0" };
    static const char *tn[5] = { "0", "√(3)/3", "1", "√(3)", "\"undefined\"" };
    int i = tu_choice("0|30|45|60|90", v[0]);
    if (i == 0 && (!v[0] || strcmp(v[0], "0") != 0)) {
        if (tu_blank(v[0])) tu_fail(o, "Choose an angle: 0°, 30°, 45°, 60° or 90°");
        else tu_fail(o, "%s° is not in the exact values table: the angles are 0°, 30°, 45°, 60° and 90°", v[0]);
        return;
    }
    double ang = i == 0 ? 0 : i == 1 ? 30 : i == 2 ? 45 : i == 3 ? 60 : 90;
    ST("Exact values table", "Learn these: 0°, 30°, 45°, 60°, 90°", "%s°", tu_w(ang));
    ST("sin", "", "sin %s°=%s", tu_w(ang), sn[i]);
    ST("cos", "", "cos %s°=%s", tu_w(ang), cs[i]);
    ST("tan", "tan = sin ÷ cos", "tan %s°=%s", tu_w(ang), tn[i]);
    tu_ans(o, asprintf_a("sin %s°=%s, cos %s°=%s, tan %s°=%s", tu_w(ang), sn[i], tu_w(ang), cs[i], tu_w(ang), tn[i]),
           asprintf_a("%s, %s, %s", tu_sf(sin(rad(ang)), 4), tu_sf(cos(rad(ang)), 4), i == 4 ? "\"undefined\"" : tu_sf(tan(rad(ang)), 4)));
    tu_val(o, sin(rad(ang)));
}

static const Tool T[] = {
    TOOL("soh_side", TOOL_CAT_GEOMETRY, "SOHCAHTOA: find a side", "Right-angled triangle: find a side from an angle and a side", 5, run_soh_side,
         F_NUM("Angle θ (°)", "e.g. 35", ""), F_NUM("Known side", "e.g. 12", ""),
         F_CH("Known side is the", "hypotenuse|opposite|adjacent", "hypotenuse"),
         F_CH("Side to find", "opposite|adjacent|hypotenuse", "opposite"), F_UNIT),
    TOOL("soh_angle", TOOL_CAT_GEOMETRY, "SOHCAHTOA: find an angle", "Right-angled triangle: find an angle from two sides", 3, run_soh_angle,
         F_CH("The two sides you know", "opp & hyp|adj & hyp|opp & adj", "opp & hyp"),
         F_NUM("First side named", "e.g. 5", ""), F_NUM("Second side named", "e.g. 8", "")),
    TOOL("sine_side", TOOL_CAT_GEOMETRY, "Sine rule: find a side", "Given angle A, its opposite side a, and angle B", 4, run_sine_side,
         F_NUM("Angle A (°)", "e.g. 40", ""), F_NUM("Side a (opposite A)", "e.g. 9", ""), F_NUM("Angle B (°)", "e.g. 65", ""), F_UNIT),
    TOOL("sine_angle", TOOL_CAT_GEOMETRY, "Sine rule: find an angle", "Given side a, its opposite angle A, and side b", 3, run_sine_angle,
         F_NUM("Side a", "e.g. 9", ""), F_NUM("Angle A (°) opposite a", "e.g. 40", ""), F_NUM("Side b", "e.g. 12", "")),
    TOOL("cosine_side", TOOL_CAT_GEOMETRY, "Cosine rule: find a side", "Two sides and the angle between them", 4, run_cosine_side,
         F_NUM("Side a", "e.g. 7", ""), F_NUM("Side b", "e.g. 10", ""), F_NUM("Angle C (°) between them", "e.g. 50", ""), F_UNIT),
    TOOL("cosine_angle", TOOL_CAT_GEOMETRY, "Cosine rule: find an angle", "All three sides known", 3, run_cosine_angle,
         F_NUM("Side a", "e.g. 7", ""), F_NUM("Side b", "e.g. 9", ""), F_NUM("Side c (opposite the angle)", "e.g. 12", "")),
    TOOL("area_sin", TOOL_CAT_GEOMETRY, "Area = ½ab sin C", "Triangle area from two sides and the angle between", 4, run_area_sin,
         F_NUM("Side a", "e.g. 8", ""), F_NUM("Side b", "e.g. 11", ""), F_NUM("Angle C (°) between them", "e.g. 40", ""), F_UNIT),
    TOOL("exact_trig", TOOL_CAT_GEOMETRY, "Exact trig values", "sin, cos and tan of 0, 30, 45, 60, 90 degrees", 1, run_exact,
         F_CH("Angle (°)", "0|30|45|60|90", "30")),
};

const Tool *tools_trig(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
