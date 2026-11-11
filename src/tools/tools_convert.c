/* GCSE Conversions: units, area, volume, speed, time, temperature, currency, density. */
#include "tools_util.h"

typedef struct { const char *name; double f; } Unit;   /* f = size of one unit in the base unit */

static const Unit LENGTH[] = { { "mm", 0.001 }, { "cm", 0.01 }, { "m", 1 }, { "km", 1000 }, { "inch", 0.0254 }, { "ft", 0.3048 }, { "yard", 0.9144 }, { "mile", 1609.344 } };
static const Unit MASS[] = { { "mg", 1e-6 }, { "g", 0.001 }, { "kg", 1 }, { "tonne", 1000 }, { "oz", 0.028349523 }, { "lb", 0.45359237 }, { "stone", 6.35029318 } };
static const Unit CAPACITY[] = { { "ml", 0.001 }, { "cl", 0.01 }, { "litre", 1 }, { "pint", 0.56826125 }, { "gallon", 4.54609 } };
static const Unit AREA[] = { { "mm²", 1e-6 }, { "cm²", 1e-4 }, { "m²", 1 }, { "km²", 1e6 }, { "hectare", 1e4 }, { "in²", 6.4516e-4 }, { "ft²", 0.09290304 } };
static const Unit VOLUME[] = { { "mm³", 1e-9 }, { "cm³", 1e-6 }, { "m³", 1 }, { "litre", 1e-3 }, { "ml", 1e-6 }, { "ft³", 0.0283168466 } };
static const Unit SPEED[] = { { "m/s", 1 }, { "km/h", 1.0 / 3.6 }, { "mph", 0.44704 }, { "knot", 0.514444444 } };
static const Unit TIME[] = { { "second", 1 }, { "minute", 60 }, { "hour", 3600 }, { "day", 86400 }, { "week", 604800 } };
static const Unit DENSITY[] = { { "g/cm³", 1000 }, { "kg/m³", 1 }, { "kg/litre", 1000 } };

#define NU(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int find_unit(const Unit *t, int n, const char *name)
{
    for (int i = 0; i < n; i++) if (name && strcmp(t[i].name, name) == 0) return i;
    return 0;
}

static void conv(const char *const *v, const CalcSettings *s, CalcResult *o, const Unit *t, int n)
{
    tu_begin(o);
    double x;
    if (!tu_num(o, "Value", v[0], &x)) return;
    const Unit *from = &t[find_unit(t, n, v[1])], *to = &t[find_unit(t, n, v[2])];
    double ratio = from->f / to->f, r = x * ratio;
    if (from == to) { tu_text(o, "Same units", "Nothing to convert"); }
    else {
        ST("Link the units", "How many of the new unit make one of the old unit", "%s", asprintf_a("1 \"%s\"=%s \"%s\"", from->name, tu_sf(ratio, 8), to->name));
        if (ratio >= 1) ST("Multiply", "Going to a smaller unit gives a bigger number", "%s×%s=%s", tu_w(x), tu_sf(ratio, 8), tu_w(r));
        else ST("Divide", "Going to a bigger unit gives a smaller number", "%s/%s=%s", tu_w(x), tu_sf(1 / ratio, 8), tu_w(r));
    }
    tu_ans(o, tu_u(tu_f(r, s, 6, -1), to->name), "");
    tu_val(o, r);
}

RUN(run_length) { conv(v, s, o, LENGTH, NU(LENGTH)); }
RUN(run_mass) { conv(v, s, o, MASS, NU(MASS)); }
RUN(run_capacity) { conv(v, s, o, CAPACITY, NU(CAPACITY)); }
RUN(run_area) { conv(v, s, o, AREA, NU(AREA)); }
RUN(run_volume) { conv(v, s, o, VOLUME, NU(VOLUME)); }
RUN(run_speed_conv) { conv(v, s, o, SPEED, NU(SPEED)); }
RUN(run_time) { conv(v, s, o, TIME, NU(TIME)); }
RUN(run_density_conv) { conv(v, s, o, DENSITY, NU(DENSITY)); }

RUN(run_temp)
{
    tu_begin(o);
    double x, c;
    if (!tu_num(o, "Temperature", v[0], &x)) return;
    int from = tu_choice("°C|°F|K", v[1]), to = tu_choice("°C|°F|K", v[2]);
    if (from == 0) c = x;
    else if (from == 1) { c = (x - 32) * 5 / 9; ST("Fahrenheit to Celsius", "", "C=(F-32)×5/9=(%s-32)×5/9=%s", tu_w(x), tu_w(c)); }
    else { c = x - 273.15; ST("Kelvin to Celsius", "", "C=K-273.15=%s", tu_w(c)); }
    if (c < -273.15 - 1e-9) { tu_fail(o, "Below absolute zero"); return; }
    double r = to == 0 ? c : to == 1 ? c * 9 / 5 + 32 : c + 273.15;
    if (to == 1) ST("Celsius to Fahrenheit", "", "F=C×9/5+32=%s×9/5+32=%s", tu_w(c), tu_w(r));
    if (to == 2) ST("Celsius to Kelvin", "", "K=C+273.15=%s", tu_w(r));
    if (from == to) tu_text(o, "Same units", "Nothing to convert");
    tu_ans(o, tu_u(tu_f(r, s, 5, -1), to == 0 ? "°C" : to == 1 ? "°F" : "K"), "");
    tu_val(o, r);
}

RUN(run_currency)
{
    tu_begin(o);
    double amt, rate;
    if (!tu_num(o, "Amount", v[0], &amt) || !tu_num(o, "Exchange rate (foreign per £1)", v[1], &rate)) return;
    if (rate <= 0) { tu_fail(o, "Rate must be positive"); return; }
    bool toforeign = tu_choice("£ to foreign|foreign to £", v[2]) == 0;
    ST("Exchange rate", "", "\"£\"1=%s", tu_w(rate));
    double r = toforeign ? amt * rate : amt / rate;
    if (toforeign) ST("Multiply by the rate", "Going to the foreign currency", "%s×%s=%s", tu_w(amt), tu_w(rate), tu_dp(r, 2));
    else ST("Divide by the rate", "Going back to pounds", "%s/%s=%s", tu_w(amt), tu_w(rate), tu_dp(r, 2));
    tu_ans(o, toforeign ? tu_f(r, s, 6, 2) : asprintf_a("\"£\"%s", tu_f(r, s, 6, 2)), "");
    tu_val(o, r);
}

RUN(run_hours)
{
    tu_begin(o);
    double x, mins = 0;
    if (!tu_num(o, "Hours", v[1], &x)) return;
    bool to_hm = tu_choice("decimal hours to h and min|h and min to decimal hours", v[0]) == 0;
    if (!to_hm && !tu_blank(v[2]) && !tu_num(o, "Minutes", v[2], &mins)) return;
    if (x < 0 || mins < 0) { tu_fail(o, "Time cannot be negative"); return; }
    if (to_hm) {
        double h = floor(x + 1e-9), m = (x - h) * 60;
        ST("Whole hours", "", "%s", tu_w(h));
        ST("Minutes", "Multiply the decimal part by 60", "%s×60=%s", tu_w(x - h), tu_w(m));
        tu_ans(o, asprintf_a("%s \"h\" %s \"min\"", tu_w(h), tu_w(tu_rsf(m, 6))), "");
        tu_val(o, x);
    } else {
        double r = x + mins / 60;
        ST("Minutes to hours", "Divide by 60", "%s/60=%s", tu_w(mins), tu_w(mins / 60));
        ST("Add the hours", "", "%s+%s=%s", tu_w(x), tu_w(mins / 60), tu_w(r));
        tu_ans(o, tu_u(tu_f(r, s, 6, -1), "hours"), "");
        tu_val(o, r);
    }
}

#define CONV_TOOL(id_, name_, desc_, run_, choices_, from_, to_) \
    TOOL(id_, TOOL_CAT_CONVERT, name_, desc_, 3, run_, F_NUM("Value", "e.g. 2.5", ""), \
         F_CH("From", choices_, from_), F_CH("To", choices_, to_))

static const Tool T[] = {
    CONV_TOOL("conv_length", "Length", "mm, cm, m, km, inches, feet, miles", run_length, "mm|cm|m|km|inch|ft|yard|mile", "m", "cm"),
    CONV_TOOL("conv_mass", "Mass", "mg, g, kg, tonnes, ounces, pounds, stones", run_mass, "mg|g|kg|tonne|oz|lb|stone", "kg", "g"),
    CONV_TOOL("conv_capacity", "Capacity", "ml, cl, litres, pints, gallons", run_capacity, "ml|cl|litre|pint|gallon", "litre", "ml"),
    CONV_TOOL("conv_area", "Area", "mm², cm², m², km², hectares", run_area, "mm²|cm²|m²|km²|hectare|in²|ft²", "m²", "cm²"),
    CONV_TOOL("conv_volume", "Volume", "mm³, cm³, m³, litres", run_volume, "mm³|cm³|m³|litre|ml|ft³", "m³", "cm³"),
    CONV_TOOL("conv_speed", "Speed", "m/s, km/h, mph, knots", run_speed_conv, "m/s|km/h|mph|knot", "km/h", "m/s"),
    CONV_TOOL("conv_time", "Time", "seconds, minutes, hours, days, weeks", run_time, "second|minute|hour|day|week", "hour", "minute"),
    CONV_TOOL("conv_density", "Density", "g/cm³, kg/m³, kg/litre", run_density_conv, "g/cm³|kg/m³|kg/litre", "g/cm³", "kg/m³"),
    CONV_TOOL("conv_temp", "Temperature", "°C, °F, kelvin", run_temp, "°C|°F|K", "°C", "°F"),
    TOOL("conv_currency", TOOL_CAT_CONVERT, "Currency", "Convert using an exchange rate", 3, run_currency,
         F_NUM("Amount", "e.g. 250", ""), F_NUM("Exchange rate (foreign per £1)", "e.g. 1.17", ""),
         F_CH("Convert", "£ to foreign|foreign to £", "£ to foreign")),
    TOOL("conv_hours", TOOL_CAT_CONVERT, "Hours and minutes", "Decimal hours to hours and minutes, and back", 3, run_hours,
         F_CH("Convert", "decimal hours to h and min|h and min to decimal hours", "decimal hours to h and min"),
         F_NUM("Hours", "e.g. 2.75", ""), F_OPT("Minutes (for h and min to decimal)", "e.g. 45")),
};

const Tool *tools_convert(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
