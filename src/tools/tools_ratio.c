/* GCSE Ratio & Proportion tools. Ratios are printed as quoted text ("3:5"). */
#include "tools_util.h"

/* Rats -> smallest whole numbers in the same ratio. */
static bool ratio_ints(CalcResult *o, const Rat *r, int n, int64_t *out)
{
    int64_t L = 1;
    for (int i = 0; i < n; i++) {
        if (rat_is_neg(r[i]) || rat_is_zero(r[i])) { tu_fail(o, "Ratio parts must be positive"); return false; }
        if (!i64_lcm(L, r[i].d, &L)) { tu_fail(o, "Numbers too large"); return false; }
    }
    int64_t g = 0;
    for (int i = 0; i < n; i++) {
        out[i] = r[i].n * (L / r[i].d);
        g = i64_gcd(g, out[i]);
    }
    for (int i = 0; i < n; i++) out[i] /= g;
    return true;
}

static const char *rq(const int64_t *p, int n) { return asprintf_a("\"%s\"", tu_ratio_str(p, n)); }

RUN(run_ratio_simplify)
{
    tu_begin(o);
    Rat r[16];
    int n;
    if (!tu_ratlist(o, "Ratio", v[0], r, 16, &n)) return;
    if (n < 2) { tu_fail(o, "Give at least two numbers"); return; }
    int64_t raw[16], p[16];
    bool whole = true;
    for (int i = 0; i < n; i++) whole = whole && r[i].d == 1;
    if (!ratio_ints(o, r, n, p)) return;
    if (!whole) {
        int64_t L = 1;
        for (int i = 0; i < n; i++) i64_lcm(L, r[i].d, &L);
        for (int i = 0; i < n; i++) raw[i] = r[i].n * (L / r[i].d);
        ST(asprintf_a("Multiply every part by %lld", (long long)L), "Whole numbers: use the LCM of the denominators", "%s", rq(raw, n));
    }
    ST("Divide every part by their HCF", "", "%s", rq(p, n));
    tu_ans(o, rq(p, n), "");
}

RUN(run_ratio_share)
{
    tu_begin(o);
    double amt;
    Rat r[16];
    int n;
    if (!tu_num(o, "Amount", v[0], &amt) || !tu_ratlist(o, "Ratio", v[1], r, 16, &n) || !tu_nonneg(o, "Amount", amt)) return;
    int64_t p[16], tot = 0;
    if (!ratio_ints(o, r, n, p)) return;
    for (int i = 0; i < n; i++) tot += p[i];
    double one = amt / (double)tot;
    {
        char sum[160] = "";
        size_t sl = 0;
        for (int i = 0; i < n && sl < sizeof sum - 24; i++) sl += (size_t)snprintf(sum + sl, sizeof sum - sl, "%s%lld", i ? "+" : "", (long long)p[i]);
        ST("Add the parts of the ratio", "", "%s=%lld", sum, (long long)tot);
    }
    ST("Find the value of one part", "Divide the amount by the total parts", "%s÷%lld=%s", tu_w(amt), (long long)tot, tu_w(one));
    /* exact shares as fractions when the amount is rational; money-style 2 d.p. if any share does not end */
    Rat ra, ex[16];
    bool exact = rat_from_double(amt, 1000000, &ra), ends = true;
    for (int i = 0; i < n && exact; i++) {
        exact = rat_mul(ra, RAT(p[i], tot), &ex[i]);
        ends = ends && exact && rat_terminates(ex[i]);
    }
    char buf[256] = "", fx[256] = "";
    size_t len = 0, fl = 0;
    for (int i = 0; i < n; i++) {
        double sh = one * (double)p[i];
        ST(asprintf_a("Share %d", i + 1), "Multiply by the number of parts", "%lld×%s=%s", (long long)p[i], tu_w(one), tu_w(sh));
        len += (size_t)snprintf(buf + len, sizeof buf - len, "%s%s", i ? ", " : "", ends || !exact ? tu_f(sh, s, 8, -1) : tu_f(sh, s, 3, 2));
        if (exact) fl += (size_t)snprintf(fx + fl, sizeof fx - fl, "%s%s", i ? ", " : "", tu_rs(ex[i]));
    }
    if (exact && !ends) tu_pair(o, s, astrdup(buf), astrdup(fx));
    else tu_ans(o, astrdup(buf), "");
}

RUN(run_ratio_combine)
{
    tu_begin(o);
    Rat a[4], b[4];
    int na, nb;
    if (!tu_ratlist(o, "First ratio a:b", v[0], a, 4, &na) || !tu_ratlist(o, "Second ratio b:c", v[1], b, 4, &nb)) return;
    if (na != 2 || nb != 2) { tu_fail(o, "Each ratio needs exactly two numbers"); return; }
    int64_t p[2], q[2], L;
    if (!ratio_ints(o, a, 2, p) || !ratio_ints(o, b, 2, q)) return;
    if (!i64_lcm(p[1], q[0], &L)) { tu_fail(o, "Numbers too large"); return; }
    int64_t fa = L / p[1], fb = L / q[0];
    ST("Make the common part (b) equal", "Use the LCM of the two b values", "LCM(%lld,%lld)=%lld", (long long)p[1], (long long)q[0], (long long)L);
    int64_t first[2] = { p[0] * fa, L }, second[2] = { L, q[1] * fb };
    ST("Scale the first ratio", "", "%s", rq(first, 2));
    ST("Scale the second ratio", "", "%s", rq(second, 2));
    int64_t res[3] = { first[0], L, second[1] }, g = i64_gcd(i64_gcd(res[0], res[1]), res[2]);
    for (int i = 0; i < 3; i++) res[i] /= g;
    ST("Combine and simplify", "", "%s", rq(res, 3));
    tu_ans(o, rq(res, 3), "");
}

RUN(run_ratio_known)
{
    tu_begin(o);
    Rat r[16];
    int n;
    int64_t k;
    double known;
    if (!tu_ratlist(o, "Ratio", v[0], r, 16, &n) || !tu_int(o, "Part number", v[1], &k) || !tu_num(o, "Known amount", v[2], &known) || !tu_nonneg(o, "Known amount", known)) return;
    if (k < 1 || k > n) { tu_fail(o, "Part number must be 1 to %d", n); return; }
    int64_t p[16], tot = 0;
    if (!ratio_ints(o, r, n, p)) return;
    for (int i = 0; i < n; i++) tot += p[i];
    double one = known / (double)p[k - 1];
    ST("Find one part", "Divide the known amount by its number of parts", "%s÷%lld=%s", tu_w(known), (long long)p[k - 1], tu_w(one));
    char buf[256] = "";
    size_t len = 0;
    for (int i = 0; i < n; i++) {
        double x = one * (double)p[i];
        ST(asprintf_a("Part %d", i + 1), "Multiply one part by its number of parts", "%lld×%s=%s", (long long)p[i], tu_w(one), tu_w(x));
        len += (size_t)snprintf(buf + len, sizeof buf - len, "%s%s", i ? ", " : "", tu_f(x, s, 8, -1));
    }
    tu_ans(o, astrdup(buf), asprintf_a("\"Total %s\"", tu_f(one * (double)tot, s, 8, -1)));
    tu_val(o, one * (double)tot);
}

RUN(run_best_buy)
{
    tu_begin(o);
    double sz[3], pr[3];
    int n = 0;
    const char *lbl[3][2] = { { "Size 1", "Price 1" }, { "Size 2", "Price 2" }, { "Size 3", "Price 3" } };
    for (int i = 0; i < 3; i++) {
        if (i == 2 && tu_blank(v[4]) && tu_blank(v[5])) break;
        if (!tu_num(o, lbl[i][0], v[2 * i], &sz[i]) || !tu_num(o, lbl[i][1], v[2 * i + 1], &pr[i])) return;
        if (sz[i] <= 0 || pr[i] <= 0) { tu_fail(o, "Sizes and prices must be positive"); return; }
        n++;
    }
    int best = 0;
    for (int i = 0; i < n; i++) {
        double u = pr[i] / sz[i];
        ST(asprintf_a("Deal %d: cost per unit", i + 1), "Divide the price by the amount", "%s/%s=%s", tu_w(pr[i]), tu_w(sz[i]), tu_w(u));
        if (u < pr[best] / sz[best]) best = i;
    }
    tu_text(o, "Compare", asprintf_a("The lowest cost per unit is the best buy: deal %d", best + 1));
    tu_ans(o, asprintf_a("\"Deal %d is the best buy\"", best + 1), asprintf_a("\"%s per unit\"", tu_sf(pr[best] / sz[best], 4)));
    tu_val(o, best + 1);
}

/* ---------- direct / inverse proportion ---------- */

static const char *PROP_CH = "x|x^2|x^3|√x";
static double pf(int kind, double x) { return kind == 0 ? x : kind == 1 ? x * x : kind == 2 ? x * x * x : sqrt(x); }
static double pf_inv(int kind, double y) { return kind == 0 ? y : kind == 1 ? sqrt(y) : kind == 2 ? cbrt(y) : y * y; }
static const char *pname(int kind) { return kind == 0 ? "x" : kind == 1 ? "x^2" : kind == 2 ? "x^3" : "√(x)"; }

static const char *ks(double k)
{
    Rat r;
    if (fabs(k - round(k)) < 1e-9) return tu_w(k);
    if (rat_from_double(k, 1000, &r) && fabs(rat_to_double(r) - k) < 1e-9 * fabs(k)) return asprintf_a("(%lld/%lld)", (long long)r.n, (long long)r.d);
    return tu_w(k);
}

static void prop_run(const char *const *v, const CalcSettings *s, CalcResult *o, bool inverse)
{
    tu_begin(o);
    double x1, y1, g;
    if (!tu_num(o, "Known x", v[1], &x1) || !tu_num(o, "Known y", v[2], &y1) || !tu_num(o, "Given value", v[4], &g)) return;
    int kind = tu_choice(PROP_CH, v[0]);
    bool findy = tu_choice("y|x", v[3]) == 0;
    double f1 = pf(kind, x1);
    if (x1 < 0 && kind == 3) { tu_fail(o, "x must not be negative"); return; }
    if (f1 == 0 || (inverse && y1 == 0)) { tu_fail(o, "Known values cannot be 0"); return; }
    const char *nm = pname(kind);
    ST("Write the proportion as an equation", inverse ? "y is inversely proportional to the chosen power of x" : "y is directly proportional to the chosen power of x",
       inverse ? "y=k/%s" : "y=k×%s", nm);
    double k = inverse ? y1 * f1 : y1 / f1;
    if (inverse) ST("Find k", "Substitute the known x and y", "k=%s×%s=%s", tu_w(y1), tu_w(f1), tu_w(k));
    else ST("Find k", "Substitute the known x and y", "k=%s/%s=%s", tu_w(y1), tu_w(f1), tu_w(k));
    ST("Write the formula", "", inverse ? "y=%s/%s" : "y=%s×%s", ks(k), nm);
    double ans;
    if (findy) {
        double f2 = pf(kind, g);
        if (inverse && f2 == 0) { tu_fail(o, "x cannot be 0"); return; }
        ans = inverse ? k / f2 : k * f2;
        ST("Substitute x", "", "y=%s", tu_w(ans));
    } else {
        if (g == 0 || k / g < 0 || (!inverse && g / k < 0)) { tu_fail(o, "No solution for this value"); return; }
        double f2 = inverse ? k / g : g / k;
        ans = pf_inv(kind, f2);
        ST("Substitute y and rearrange", "", inverse ? "%s=%s/%s" : "%s=%s×%s", tu_w(g), ks(k), nm);
        ST("Solve for x", "", "x=%s", tu_w(ans));
    }
    tu_ans(o, tu_f(ans, s, 4, -1), "");
    tu_val(o, ans);
}

RUN(run_prop_direct) { prop_run(v, s, o, false); }
RUN(run_prop_inverse) { prop_run(v, s, o, true); }

/* ---------- compound measures: q3 = q1 / q2 ---------- */

static void tri_run(const char *const *v, CalcResult *o, const char *n1, const char *n2, const char *n3,
                    const char *l1, const char *l2, const char *l3)
{
    tu_begin(o);
    bool b1 = tu_blank(v[0]), b2 = tu_blank(v[1]), b3 = tu_blank(v[2]);
    if (b1 + b2 + b3 != 1) { tu_fail(o, "Leave exactly one box empty: the one to find"); return; }
    double q1 = 0, q2 = 0, q3 = 0, ans;
    if ((!b1 && !tu_num(o, n1, v[0], &q1)) || (!b2 && !tu_num(o, n2, v[1], &q2)) || (!b3 && !tu_num(o, n3, v[2], &q3))) return;
    if ((!b1 && !tu_nonneg(o, n1, q1)) || (!b2 && !tu_nonneg(o, n2, q2)) || (!b3 && !tu_nonneg(o, n3, q3))) return;
    char units[80];
    snprintf(units, sizeof units, "%s", v[3] ? v[3] : "");
    char *u[3] = { "", "", "" };
    int k = 0;
    for (char *t = strtok(units, ", "); t && k < 3; t = strtok(NULL, ", ")) u[k++] = t;
    ST("Formula", asprintf_a("%s = %s ÷ %s", n3, n1, n2), "%s=%s/%s", l3, l1, l2);
    const char *unit;
    if (b3) {
        if (q2 == 0) { tu_fail(o, "%s cannot be 0", n2); return; }
        ans = q1 / q2; unit = u[2];
        ST("Substitute", "", "%s=%s/%s", l3, tu_w(q1), tu_w(q2));
    } else if (b1) {
        ans = q3 * q2; unit = u[0];
        ST("Rearrange", "Multiply both sides", "%s=%s×%s", l1, l3, l2);
        ST("Substitute", "", "%s=%s×%s", l1, tu_w(q3), tu_w(q2));
    } else {
        if (q3 == 0) { tu_fail(o, "%s cannot be 0", n3); return; }
        ans = q1 / q3; unit = u[1];
        ST("Rearrange", "", "%s=%s/%s", l2, l1, l3);
        ST("Substitute", "", "%s=%s/%s", l2, tu_w(q1), tu_w(q3));
    }
    ST("Answer", "", "%s", tu_w(ans));
    tu_ans(o, tu_u(tu_sf(ans, 4), unit), "");
    tu_val(o, ans);
}

RUN(run_speed) { tri_run(v, o, "Distance", "Time", "Speed", "d", "t", "s"); }
RUN(run_density) { tri_run(v, o, "Mass", "Volume", "Density", "m", "V", "d"); }
RUN(run_pressure) { tri_run(v, o, "Force", "Area", "Pressure", "F", "A", "P"); }

/* ---------- scale factors ---------- */

RUN(run_scale)
{
    tu_begin(o);
    double k, orig = 0;
    if (!tu_num(o, "Scale factor", v[0], &k)) return;
    bool have = !tu_blank(v[2]);
    if (have && (!tu_num(o, "Original value", v[2], &orig) || !tu_nonneg(o, "Original value", orig))) return;
    if (k <= 0) { tu_fail(o, "Scale factor must be positive"); return; }
    int kind = tu_choice("length|area|volume", v[1]);
    double len = kind == 0 ? k : kind == 1 ? sqrt(k) : cbrt(k);
    ST("Length scale factor", kind == 0 ? "Given" : kind == 1 ? "Area scale factor = (length sf)², so take the square root" : "Volume scale factor = (length sf)³, so take the cube root",
       "%s", tu_w(len));
    ST("Area scale factor", "Square the length scale factor", "%s^2=%s", tu_w(len), tu_w(len * len));
    ST("Volume scale factor", "Cube the length scale factor", "%s^3=%s", tu_w(len), tu_w(len * len * len));
    double f = kind == 0 ? len : kind == 1 ? len * len : len * len * len;
    if (have) ST("New value", "Original × scale factor of the same type", "%s×%s=%s", tu_w(orig), tu_w(f), tu_w(orig * f));
    tu_ans(o, asprintf_a("\"Length scale factor\"=%s, \"Area scale factor\"=%s, \"Volume scale factor\"=%s", tu_f(len, s, 6, -1), tu_f(len * len, s, 6, -1), tu_f(len * len * len, s, 6, -1)),
           have ? tu_f(orig * f, s, 6, -1) : "");
    tu_val(o, have ? orig * f : f);
}

static const Tool T[] = {
    TOOL("ratio_simplify", TOOL_CAT_RATIO, "Simplify a ratio", "Write a ratio in its simplest form", 1, run_ratio_simplify,
         F_LIST("Ratio parts", "e.g. 12, 18, 30", "")),
    TOOL("ratio_share", TOOL_CAT_RATIO, "Share in a ratio", "Divide an amount in a given ratio", 2, run_ratio_share,
         F_NUM("Amount", "e.g. 60", ""), F_LIST("Ratio", "e.g. 2, 3", "")),
    TOOL("ratio_combine", TOOL_CAT_RATIO, "Combine ratios", "From a:b and b:c find a:b:c", 2, run_ratio_combine,
         F_LIST("First ratio a:b", "e.g. 2, 3", ""), F_LIST("Second ratio b:c", "e.g. 4, 5", "")),
    TOOL("ratio_known", TOOL_CAT_RATIO, "Ratio with one amount known", "Find the other parts when one part is given", 3, run_ratio_known,
         F_LIST("Ratio", "e.g. 3, 5, 2", ""), F_NUM("Which part is known", "e.g. 2", ""), F_NUM("Known amount", "e.g. 40", "")),
    TOOL("best_buy", TOOL_CAT_RATIO, "Best buy", "Compare the cost per unit of two or three deals", 6, run_best_buy,
         F_NUM("Size 1", "e.g. 400", ""), F_NUM("Price 1", "e.g. 1.80", ""), F_NUM("Size 2", "e.g. 750", ""), F_NUM("Price 2", "e.g. 3.15", ""),
         F_OPT("Size 3 (optional)", "e.g. 1000"), F_OPT("Price 3 (optional)", "e.g. 4.00")),
    TOOL("prop_direct", TOOL_CAT_RATIO, "Direct proportion", "y proportional to x, x², x³ or √x: find k, then y or x", 5, run_prop_direct,
         F_CH("y is directly proportional to", "x|x^2|x^3|√x", "x"), F_NUM("Known x", "e.g. 3", ""), F_NUM("Known y", "e.g. 12", ""),
         F_CH("Find", "y|x", "y"), F_NUM("Given value (of the other)", "e.g. 5", "")),
    TOOL("prop_inverse", TOOL_CAT_RATIO, "Inverse proportion", "y proportional to 1/x, 1/x² ...: find k, then y or x", 5, run_prop_inverse,
         F_CH("y is inversely proportional to", "x|x^2|x^3|√x", "x"), F_NUM("Known x", "e.g. 4", ""), F_NUM("Known y", "e.g. 6", ""),
         F_CH("Find", "y|x", "y"), F_NUM("Given value (of the other)", "e.g. 8", "")),
    TOOL("speed", TOOL_CAT_RATIO, "Speed, distance, time", "Leave the one you want to find empty", 4, run_speed,
         F_OPT("Distance", "e.g. 150"), F_OPT("Time", "e.g. 2.5"), F_OPT("Speed", "leave empty to find"),
         F_CH("Units", "km, h, km/h|m, s, m/s|miles, h, mph", "km, h, km/h")),
    TOOL("density", TOOL_CAT_RATIO, "Density, mass, volume", "Leave the one you want to find empty", 4, run_density,
         F_OPT("Mass", "e.g. 540"), F_OPT("Volume", "e.g. 200"), F_OPT("Density", "leave empty to find"),
         F_CH("Units", "g, cm³, g/cm³|kg, m³, kg/m³", "g, cm³, g/cm³")),
    TOOL("pressure", TOOL_CAT_RATIO, "Pressure, force, area", "Leave the one you want to find empty", 4, run_pressure,
         F_OPT("Force", "e.g. 600"), F_OPT("Area", "e.g. 0.4"), F_OPT("Pressure", "leave empty to find"),
         F_CH("Units", "N, m², N/m²|N, cm², N/cm²", "N, m², N/m²")),
    TOOL("scale", TOOL_CAT_RATIO, "Scale factors: length, area, volume", "Convert between length, area and volume scale factors", 3, run_scale,
         F_NUM("Scale factor", "e.g. 3", ""), F_CH("The scale factor is for", "length|area|volume", "length"),
         F_OPT("Original value (optional)", "e.g. 12")),
};

const Tool *tools_ratio(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
