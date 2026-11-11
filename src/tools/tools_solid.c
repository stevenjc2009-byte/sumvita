/* GCSE Geometry tools: volumes and surface areas of solids. */
#include "tools_util.h"

static const char *un(const char *u) { return u ? u : ""; }

/* decimal answer with the exact π form as the alternative. val = pc × π. */
static void out_pi(CalcResult *o, const CalcSettings *s, const char *lab, double val, double pc, const char *unit)
{
    const char *d = asprintf_a("\"%s\"=%s", lab, tu_u(tu_f(val, s, 3, -1), unit));
    const char *p = tu_pi(pc);
    tu_pair(o, s, d, p ? asprintf_a("\"%s\"=%s", lab, tu_u(p, unit)) : NULL);
    tu_val(o, val);
}

static bool pos2(CalcResult *o, double a, double b)
{
    if (a <= 0 || b <= 0) { tu_fail(o, "Lengths must be positive"); return false; }
    return true;
}

RUN(run_vol_prism)
{
    tu_begin(o);
    double a, l;
    if (!tu_num(o, "Cross-section area", v[0], &a) || !tu_num(o, "Length", v[1], &l) || !pos2(o, a, l)) return;
    ST("Volume of a prism", "area of cross-section × length", "V=A×l");
    ST("Substitute", "", "V=%s×%s=%s", tu_w(a), tu_w(l), tu_w(a * l));
    tu_ans(o, tu_u(tu_f(a * l, s, 3, -1), tu_unit_pow(un(v[2]), 3)), "");
    tu_val(o, a * l);
}

RUN(run_vol_cyl)
{
    tu_begin(o);
    double r, h;
    if (!tu_num(o, "Radius", v[0], &r) || !tu_num(o, "Height", v[1], &h) || !pos2(o, r, h)) return;
    ST("Volume of a cylinder", "circle area × height", "V=π×r^2×h");
    ST("Substitute", "", "V=π×%s^2×%s=%s", tu_w(r), tu_w(h), tu_w(PI_D * r * r * h));
    out_pi(o, s, "Volume", PI_D * r * r * h, r * r * h, tu_unit_pow(un(v[2]), 3));
}

RUN(run_vol_cone)
{
    tu_begin(o);
    double r, h;
    if (!tu_num(o, "Radius", v[0], &r) || !tu_num(o, "Perpendicular height", v[1], &h) || !pos2(o, r, h)) return;
    ST("Volume of a cone", "one third × base area × height", "V=1/3×π×r^2×h");
    ST("Substitute", "", "V=1/3×π×%s^2×%s=%s", tu_w(r), tu_w(h), tu_w(PI_D * r * r * h / 3));
    out_pi(o, s, "Volume", PI_D * r * r * h / 3, r * r * h / 3, tu_unit_pow(un(v[2]), 3));
}

RUN(run_vol_sphere)
{
    tu_begin(o);
    double r;
    if (!tu_num(o, "Radius", v[0], &r)) return;
    if (r <= 0) { tu_fail(o, "Radius must be positive"); return; }
    bool hemi = tu_choice("sphere|hemisphere", v[1]) == 1;
    double k = hemi ? 2.0 / 3.0 : 4.0 / 3.0;
    ST(hemi ? "Volume of a hemisphere" : "Volume of a sphere", hemi ? "half of a sphere" : "", hemi ? "V=2/3×π×r^3" : "V=4/3×π×r^3");
    ST("Substitute", "", "V=%s×π×%s^3=%s", hemi ? "2/3" : "4/3", tu_w(r), tu_w(k * PI_D * r * r * r));
    out_pi(o, s, "Volume", k * PI_D * r * r * r, k * r * r * r, tu_unit_pow(un(v[2]), 3));
}

RUN(run_vol_pyr)
{
    tu_begin(o);
    double l, w, h;
    if (!tu_num(o, "Base length", v[0], &l) || !tu_num(o, "Base width", v[1], &w) || !tu_num(o, "Perpendicular height", v[2], &h)) return;
    if (l <= 0 || w <= 0 || h <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    double r = l * w * h / 3;
    ST("Volume of a pyramid", "one third × base area × height", "V=1/3×l×w×h");
    ST("Substitute", "", "V=1/3×%s×%s×%s=%s", tu_w(l), tu_w(w), tu_w(h), tu_w(r));
    tu_ans(o, tu_u(tu_f(r, s, 3, -1), tu_unit_pow(un(v[3]), 3)), "");
    tu_val(o, r);
}

RUN(run_vol_frustum)
{
    tu_begin(o);
    double R, r, h;
    if (!tu_num(o, "Bottom radius R", v[0], &R) || !tu_num(o, "Top radius r", v[1], &r) || !tu_num(o, "Height of frustum", v[2], &h)) return;
    if (R <= 0 || r <= 0 || h <= 0 || r >= R) { tu_fail(o, "Need R > r > 0 and h > 0"); return; }
    double H = h * R / (R - r), small = H - h;
    double vb = PI_D * R * R * H / 3, vs = PI_D * r * r * small / 3;
    ST("Complete the cone", "A frustum is a big cone with a small cone removed", "H/%s=(H-%s)/%s", tu_w(R), tu_w(h), tu_w(r));
    ST("Height of the full cone (similar triangles)", "", "H=%s×%s/(%s-%s)=%s", tu_w(h), tu_w(R), tu_w(R), tu_w(r), tu_w(H));
    ST("Volume of the big cone", "", "V=1/3×π×%s^2×%s=%s", tu_w(R), tu_w(H), tu_w(vb));
    ST("Volume of the small cone", "Height H - h", "V=1/3×π×%s^2×%s=%s", tu_w(r), tu_w(small), tu_w(vs));
    ST("Subtract", "", "%s-%s=%s", tu_w(vb), tu_w(vs), tu_w(vb - vs));
    out_pi(o, s, "Volume", vb - vs, (R * R * H - r * r * small) / 3, tu_unit_pow(un(v[3]), 3));
}

RUN(run_cuboid)
{
    tu_begin(o);
    double l, w, h;
    if (!tu_num(o, "Length", v[0], &l) || !tu_num(o, "Width", v[1], &w) || !tu_num(o, "Height", v[2], &h)) return;
    if (l <= 0 || w <= 0 || h <= 0) { tu_fail(o, "Lengths must be positive"); return; }
    double vol = l * w * h, sa = 2 * (l * w + l * h + w * h);
    ST("Volume of a cuboid", "", "V=l×w×h=%s×%s×%s=%s", tu_w(l), tu_w(w), tu_w(h), tu_w(vol));
    ST("Surface area", "Three pairs of faces", "S=2(lw+lh+wh)=2(%s+%s+%s)=%s", tu_w(l * w), tu_w(l * h), tu_w(w * h), tu_w(sa));
    const char *u = un(v[3]);
    tu_ans(o, asprintf_a("\"Volume\"=%s, \"Surface area\"=%s", tu_u(tu_f(vol, s, 3, -1), tu_unit_pow(u, 3)), tu_u(tu_f(sa, s, 3, -1), tu_unit_pow(u, 2))), "");
    tu_val(o, vol);
}

RUN(run_surf_cyl)
{
    tu_begin(o);
    double r, h;
    if (!tu_num(o, "Radius", v[0], &r) || !tu_num(o, "Height", v[1], &h) || !pos2(o, r, h)) return;
    double cur = 2 * PI_D * r * h, ends = 2 * PI_D * r * r;
    ST("Curved surface", "A rectangle: circumference × height", "2×π×r×h=2×π×%s×%s=%s", tu_w(r), tu_w(h), tu_w(cur));
    ST("Two circular ends", "", "2×π×r^2=2×π×%s^2=%s", tu_w(r), tu_w(ends));
    ST("Total surface area", "", "%s+%s=%s", tu_w(cur), tu_w(ends), tu_w(cur + ends));
    out_pi(o, s, "Surface area", cur + ends, 2 * r * h + 2 * r * r, tu_unit_pow(un(v[2]), 2));
}

RUN(run_surf_cone)
{
    tu_begin(o);
    double r, h;
    if (!tu_num(o, "Radius", v[0], &r) || !tu_num(o, "Perpendicular height", v[1], &h) || !pos2(o, r, h)) return;
    double l = sqrt(r * r + h * h), cur = PI_D * r * l, base = PI_D * r * r;
    ST("Slant height", "Pythagoras", "l=√(%s^2+%s^2)=%s", tu_w(r), tu_w(h), tu_w(l));
    ST("Curved surface", "π × r × l", "π×%s×%s=%s", tu_w(r), tu_w(l), tu_w(cur));
    ST("Circular base", "π × r²", "π×%s^2=%s", tu_w(r), tu_w(base));
    ST("Total surface area", "", "%s+%s=%s", tu_w(cur), tu_w(base), tu_w(cur + base));
    out_pi(o, s, "Surface area", cur + base, r * l + r * r, tu_unit_pow(un(v[2]), 2));
}

RUN(run_surf_sphere)
{
    tu_begin(o);
    double r;
    if (!tu_num(o, "Radius", v[0], &r)) return;
    if (r <= 0) { tu_fail(o, "Radius must be positive"); return; }
    bool hemi = tu_choice("sphere|hemisphere", v[1]) == 1;
    if (hemi) {
        ST("Curved surface", "Half of 4πr²", "2×π×%s^2=%s", tu_w(r), tu_w(2 * PI_D * r * r));
        ST("Flat circular face", "", "π×%s^2=%s", tu_w(r), tu_w(PI_D * r * r));
        ST("Total surface area", "", "3×π×r^2=%s", tu_w(3 * PI_D * r * r));
    } else {
        ST("Surface area of a sphere", "", "S=4×π×r^2=4×π×%s^2=%s", tu_w(r), tu_w(4 * PI_D * r * r));
    }
    double k = hemi ? 3 : 4;
    out_pi(o, s, "Surface area", k * PI_D * r * r, k * r * r, tu_unit_pow(un(v[2]), 2));
}

static const Tool T[] = {
    TOOL("vol_prism", TOOL_CAT_GEOMETRY, "Volume of a prism", "Cross-section area times length", 3, run_vol_prism,
         F_NUM("Cross-section area", "e.g. 24", ""), F_NUM("Length", "e.g. 10", ""), F_UNIT),
    TOOL("vol_cyl", TOOL_CAT_GEOMETRY, "Volume of a cylinder", "π r² h", 3, run_vol_cyl,
         F_NUM("Radius", "e.g. 4", ""), F_NUM("Height", "e.g. 9", ""), F_UNIT),
    TOOL("vol_cone", TOOL_CAT_GEOMETRY, "Volume of a cone", "⅓ π r² h", 3, run_vol_cone,
         F_NUM("Radius", "e.g. 3", ""), F_NUM("Perpendicular height", "e.g. 7", ""), F_UNIT),
    TOOL("vol_sphere", TOOL_CAT_GEOMETRY, "Volume of a sphere / hemisphere", "4/3 π r³", 3, run_vol_sphere,
         F_NUM("Radius", "e.g. 6", ""), F_CH("Shape", "sphere|hemisphere", "sphere"), F_UNIT),
    TOOL("vol_pyr", TOOL_CAT_GEOMETRY, "Volume of a pyramid", "⅓ × base area × height", 4, run_vol_pyr,
         F_NUM("Base length", "e.g. 6", ""), F_NUM("Base width", "e.g. 4", ""), F_NUM("Perpendicular height", "e.g. 9", ""), F_UNIT),
    TOOL("vol_frustum", TOOL_CAT_GEOMETRY, "Volume of a frustum", "Big cone minus small cone", 4, run_vol_frustum,
         F_NUM("Bottom radius R", "e.g. 6", ""), F_NUM("Top radius r", "e.g. 3", ""), F_NUM("Height of frustum", "e.g. 8", ""), F_UNIT),
    TOOL("cuboid", TOOL_CAT_GEOMETRY, "Cuboid volume and surface area", "Volume and total surface area", 4, run_cuboid,
         F_NUM("Length", "e.g. 8", ""), F_NUM("Width", "e.g. 5", ""), F_NUM("Height", "e.g. 3", ""), F_UNIT),
    TOOL("surf_cyl", TOOL_CAT_GEOMETRY, "Surface area of a cylinder", "Curved surface plus two ends", 3, run_surf_cyl,
         F_NUM("Radius", "e.g. 4", ""), F_NUM("Height", "e.g. 9", ""), F_UNIT),
    TOOL("surf_cone", TOOL_CAT_GEOMETRY, "Surface area of a cone", "Slant height, curved surface and base", 3, run_surf_cone,
         F_NUM("Radius", "e.g. 3", ""), F_NUM("Perpendicular height", "e.g. 4", ""), F_UNIT),
    TOOL("surf_sphere", TOOL_CAT_GEOMETRY, "Surface area of a sphere / hemisphere", "4 π r²", 3, run_surf_sphere,
         F_NUM("Radius", "e.g. 5", ""), F_CH("Shape", "sphere|hemisphere", "sphere"), F_UNIT),
};

const Tool *tools_solid(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
