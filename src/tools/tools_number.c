/* GCSE Number tools: percentages, interest, VAT, HCF/LCM, bounds, rounding, fractions. */
#include "tools_util.h"

/* ---------- percentages ---------- */

RUN(run_pct_of)
{
    tu_begin(o);
    double p, a;
    if (!tu_num(o, "Percentage", v[0], &p) || !tu_num(o, "Amount", v[1], &a) || !tu_nonneg(o, "Amount", a)) return;
    double r = p / 100.0 * a;
    ST("Write the percentage as a fraction", "Per cent means out of 100", "%s%%=%s/100", tu_w(p), tu_w(p));
    ST("Multiply by the amount", "", "%s/100×%s", tu_w(p), tu_w(a));
    ST("Calculate", "", "%s", tu_w(r));
    tu_ans(o, tu_f(r, s, 8, -1), "");
    tu_val(o, r);
}

RUN(run_pct_change)
{
    tu_begin(o);
    double a, b;
    if (!tu_num(o, "Original value", v[0], &a) || !tu_num(o, "New value", v[1], &b)) return;
    if (a == 0) { tu_fail(o, "Original value cannot be 0"); return; }
    double ch = b - a, pc = ch / a * 100.0;
    ST("Find the change", "New value - original value", "%s-%s=%s", tu_w(b), tu_w(a), tu_w(ch));
    ST("Percentage change", "change ÷ original × 100", "(%s/%s)×100=%s%%", tu_w(ch), tu_w(a), tu_w(pc));
    tu_ans(o, asprintf_a("%s%% \"%s\"", tu_f(fabs(pc), s, 3, -1), pc >= 0 ? "increase" : "decrease"), "");
    tu_val(o, pc);
}

RUN(run_pct_reverse)
{
    tu_begin(o);
    double after, p;
    if (!tu_num(o, "Price after the change", v[0], &after) || !tu_num(o, "Percentage change", v[1], &p) ||
        !tu_nonneg(o, "Price after the change", after)) return;
    bool up = tu_choice("increase|decrease", v[2]) == 0;
    double m = up ? 1 + p / 100 : 1 - p / 100;
    if (m <= 0) { tu_fail(o, "Multiplier must be positive"); return; }
    double orig = after / m;
    ST("Find the multiplier", up ? "Increase: 1 + p/100" : "Decrease: 1 - p/100", "100%%%s%s%%=%s%%", up ? "+" : "-", tu_w(p), tu_w(m * 100));
    ST("Write the multiplier", "", "m=%s", tu_w(m));
    ST("Reverse: divide by the multiplier", "original × multiplier = new value", "x=%s/%s=%s", tu_w(after), tu_w(m), tu_w(orig));
    tu_ans(o, tu_f(orig, s, 8, -1), "");
    tu_val(o, orig);
}

RUN(run_pct_adjust)
{
    tu_begin(o);
    double a, p;
    if (!tu_num(o, "Amount", v[0], &a) || !tu_num(o, "Percentage", v[1], &p) || !tu_nonneg(o, "Amount", a)) return;
    bool up = tu_choice("increase|decrease", v[2]) == 0;
    double m = up ? 1 + p / 100 : 1 - p / 100;
    double r = a * m;
    ST("Find the multiplier", up ? "Increase: 1 + p/100" : "Decrease: 1 - p/100", "m=1%s%s/100=%s", up ? "+" : "-", tu_w(p), tu_w(m));
    ST("Multiply the amount by the multiplier", "", "%s×%s=%s", tu_w(a), tu_w(m), tu_w(r));
    tu_ans(o, tu_f(r, s, 8, -1), asprintf_a("\"Change of %s\"", tu_w(r - a)));
    tu_val(o, r);
}

/* ---------- interest, VAT ---------- */

RUN(run_compound)
{
    tu_begin(o);
    double pr, rate, yrs;
    if (!tu_num(o, "Starting amount", v[0], &pr) || !tu_num(o, "Rate % per year", v[1], &rate) ||
        !tu_num(o, "Years", v[2], &yrs) || !tu_nonneg(o, "Starting amount", pr) || !tu_nonneg(o, "Years", yrs)) return;
    bool up = tu_choice("growth|decay", v[3]) == 0;
    double m = up ? 1 + rate / 100 : 1 - rate / 100;
    if (m <= 0) {
        tu_fail(o, up ? "Rate %s%% is too low: growth needs a rate above -100%%" : "Rate %s%% is too high: decay needs a rate below 100%%", tu_w(rate));
        return;
    }
    double a = pr * pow(m, yrs);
    ST("Formula", up ? "Compound interest / growth" : "Depreciation / decay", "A=P×(1%s r/100)^n", up ? "+" : "-");
    ST("Find the multiplier", "", "1%s%s/100=%s", up ? "+" : "-", tu_w(rate), tu_w(m));
    ST("Substitute", "P = starting amount, n = number of years", "A=%s×%s^%s", tu_w(pr), tu_w(m), tu_w(yrs));
    ST("Calculate", "", "A=%s", tu_w(a));
    ST("Change", up ? "Interest earned = A - P" : "Loss in value = P - A", "%s", tu_dp(fabs(a - pr), 2));
    tu_ans(o, tu_f(a, s, 8, 2), asprintf_a("\"Change of %s\"", tu_dp(a - pr, 2)));
    tu_val(o, a);
}

RUN(run_simple_int)
{
    tu_begin(o);
    double pr, rate, yrs;
    if (!tu_num(o, "Amount", v[0], &pr) || !tu_num(o, "Rate % per year", v[1], &rate) ||
        !tu_num(o, "Years", v[2], &yrs) || !tu_nonneg(o, "Amount", pr) || !tu_nonneg(o, "Years", yrs)) return;
    double i = pr * rate * yrs / 100.0;
    ST("Formula", "Simple interest is the same every year", "I=(P×R×T)/100");
    ST("Substitute", "", "I=(%s×%s×%s)/100", tu_w(pr), tu_w(rate), tu_w(yrs));
    ST("Interest", "", "I=%s", tu_dp(i, 2));
    ST("Total", "Add the interest to the amount", "%s+%s=%s", tu_w(pr), tu_dp(i, 2), tu_dp(pr + i, 2));
    tu_ans(o, tu_f(i, s, 8, 2), asprintf_a("\"Total %s\"", tu_dp(pr + i, 2)));
    tu_val(o, i);
}

RUN(run_vat)
{
    tu_begin(o);
    double pr, rate;
    if (!tu_num(o, "Price", v[0], &pr) || !tu_num(o, "VAT rate %", v[1], &rate) || !tu_nonneg(o, "Price", pr)) return;
    double m = 1 + rate / 100;
    if (tu_choice("add|remove", v[2]) == 0) {
        double r = pr * m;
        ST("Multiplier for adding VAT", "", "1+%s/100=%s", tu_w(rate), tu_w(m));
        ST("Price including VAT", "", "%s×%s=%s", tu_w(pr), tu_w(m), tu_dp(r, 2));
        tu_ans(o, tu_f(r, s, 8, 2), asprintf_a("\"VAT %s\"", tu_dp(r - pr, 2)));
        tu_val(o, r);
    } else {
        double r = pr / m;
        ST("Price including VAT is", "", "%s%% of the price before VAT", tu_w(m * 100));
        ST("Divide by the multiplier", "", "%s/%s=%s", tu_w(pr), tu_w(m), tu_dp(r, 2));
        tu_ans(o, tu_f(r, s, 8, 2), asprintf_a("\"VAT %s\"", tu_dp(pr - r, 2)));
        tu_val(o, r);
    }
}

/* ---------- HCF and LCM ---------- */

typedef struct { int64_t p[16]; int e[16]; int k; } Fac;

static void fac_of(int64_t n, Fac *f) { f->k = n >= 2 ? i64_factorize(n, f->p, f->e, 16) : 0; }

static const char *fac_str(const Fac *f)
{
    char b[200];
    size_t len = 0;
    if (!f->k) return "1";
    b[0] = 0;
    for (int i = 0; i < f->k && len < sizeof b - 30; i++) {
        if (f->e[i] > 1) len += (size_t)snprintf(b + len, sizeof b - len, "%s%lld^%d", i ? "×" : "", (long long)f->p[i], f->e[i]);
        else len += (size_t)snprintf(b + len, sizeof b - len, "%s%lld", i ? "×" : "", (long long)f->p[i]);
    }
    return astrdup(b);
}

static int fac_exp(const Fac *f, int64_t p)
{
    for (int i = 0; i < f->k; i++) if (f->p[i] == p) return f->e[i];
    return 0;
}

static void add_copies(char *list, size_t cap, int64_t p, int copies)
{
    for (int i = 0; i < copies; i++) {
        size_t l = strlen(list);
        snprintf(list + l, cap - l, "%s%lld", l ? ", " : "", (long long)p);
    }
}

static const char *times_form(const char *list)   /* "2, 3" -> "2×3" */
{
    if (!*list) return "1";
    char *c = astrdup(list);
    char *out = amalloc(strlen(c) * 3 + 4), *w = out;
    for (char *r = c; *r; r++) {
        if (*r == ',' && r[1] == ' ') { w += sprintf(w, "×"); r++; }
        else *w++ = *r;
    }
    *w = 0;
    return out;
}

RUN(run_hcf_lcm)
{
    tu_begin(o);
    int64_t a, b;
    if (!tu_int(o, "First number", v[0], &a) || !tu_int(o, "Second number", v[1], &b)) return;
    if (a < 1 || b < 1 || a > 1000000000000LL || b > 1000000000000LL) { tu_fail(o, "Use whole numbers from 1 to 10^12"); return; }
    Fac fa, fb;
    fac_of(a, &fa);
    fac_of(b, &fb);
    ST("Prime factors of the first number", "", "%lld=%s", (long long)a, fac_str(&fa));
    ST("Prime factors of the second number", "", "%lld=%s", (long long)b, fac_str(&fb));
    char both[160] = "", onlya[160] = "", onlyb[160] = "";
    int64_t primes[32];
    int np = 0;
    for (int i = 0; i < fa.k; i++) primes[np++] = fa.p[i];
    for (int i = 0; i < fb.k; i++) if (!fac_exp(&fa, fb.p[i])) primes[np++] = fb.p[i];
    for (int i = 0; i < np; i++)
        for (int j = i + 1; j < np; j++) if (primes[j] < primes[i]) { int64_t t = primes[i]; primes[i] = primes[j]; primes[j] = t; }
    for (int i = 0; i < np; i++) {
        int ea = fac_exp(&fa, primes[i]), eb = fac_exp(&fb, primes[i]), m = ea < eb ? ea : eb;
        add_copies(both, sizeof both, primes[i], m);
        add_copies(onlya, sizeof onlya, primes[i], ea - m);
        add_copies(onlyb, sizeof onlyb, primes[i], eb - m);
    }
    tu_text(o, "Venn diagram", asprintf_a("In both circles: %s.  Only %lld: %s.  Only %lld: %s.",
            *both ? both : "nothing", (long long)a, *onlya ? onlya : "nothing", (long long)b, *onlyb ? onlyb : "nothing"));
    int64_t g = i64_gcd(a, b), l;
    if (!i64_lcm(a, b, &l)) { tu_fail(o, "LCM too large"); return; }
    ST("HCF: multiply the numbers in both circles", "", "\"HCF\"=%s=%lld", times_form(both), (long long)g);
    char all[500];
    snprintf(all, sizeof all, "%s%s%s%s%s", both, *both && *onlya ? ", " : "", onlya,
             (*both || *onlya) && *onlyb ? ", " : "", onlyb);
    ST("LCM: multiply every number in the diagram", "", "\"LCM\"=%s=%lld", times_form(all), (long long)l);
    tu_ans(o, asprintf_a("\"HCF\"=%lld, \"LCM\"=%lld", (long long)g, (long long)l), "");
    tu_val(o, (double)g);
}

/* ---------- bounds ---------- */

RUN(run_bounds)
{
    tu_begin(o);
    double x, acc;
    if (!tu_num(o, "Measurement", v[0], &x) || !tu_num(o, "Rounded to the nearest", v[1], &acc)) return;
    if (acc <= 0) { tu_fail(o, "Accuracy must be positive"); return; }
    double h = acc / 2, lb = x - h, ub = x + h;
    ST("Half the degree of accuracy", "Rounded to the nearest unit u, the true value is within u/2", "%s/2=%s", tu_w(acc), tu_w(h));
    ST("Lower bound", "Subtract", "%s-%s=%s", tu_w(x), tu_w(h), tu_w(lb));
    ST("Upper bound", "Add", "%s+%s=%s", tu_w(x), tu_w(h), tu_w(ub));
    tu_ans(o, asprintf_a("%s≤x<%s", tu_w(lb), tu_w(ub)), asprintf_a("\"Lower bound\"=%s, \"Upper bound\"=%s", tu_w(lb), tu_w(ub)));
    tu_val(o, ub);
}

RUN(run_bounds_calc)
{
    tu_begin(o);
    double a, da, b, db;
    if (!tu_num(o, "a", v[0], &a) || !tu_num(o, "a rounded to nearest", v[1], &da) ||
        !tu_num(o, "b", v[3], &b) || !tu_num(o, "b rounded to nearest", v[4], &db)) return;
    if (da <= 0 || db <= 0) { tu_fail(o, "Accuracy must be positive"); return; }
    double al = a - da / 2, au = a + da / 2, bl = b - db / 2, bu = b + db / 2, lo, hi;
    int op = tu_choice("a+b|a-b|a×b|a÷b", v[2]);
    ST("Bounds of a", "", "%s≤a<%s", tu_w(al), tu_w(au));
    ST("Bounds of b", "", "%s≤b<%s", tu_w(bl), tu_w(bu));
    switch (op) {
    case 0: lo = al + bl; hi = au + bu;
        ST("Upper bound: use both upper bounds", "", "%s+%s=%s", tu_w(au), tu_w(bu), tu_w(hi));
        ST("Lower bound: use both lower bounds", "", "%s+%s=%s", tu_w(al), tu_w(bl), tu_w(lo)); break;
    case 1: lo = al - bu; hi = au - bl;
        ST("Upper bound: biggest a, smallest b", "", "%s-%s=%s", tu_w(au), tu_w(bl), tu_w(hi));
        ST("Lower bound: smallest a, biggest b", "", "%s-%s=%s", tu_w(al), tu_w(bu), tu_w(lo)); break;
    case 2: lo = al * bl; hi = au * bu;
        ST("Upper bound: both upper bounds", "", "%s×%s=%s", tu_w(au), tu_w(bu), tu_w(hi));
        ST("Lower bound: both lower bounds", "", "%s×%s=%s", tu_w(al), tu_w(bl), tu_w(lo)); break;
    default:
        if (bl <= 0) { tu_fail(o, "b could be 0 or negative"); return; }
        lo = al / bu; hi = au / bl;
        ST("Upper bound: biggest a, smallest b", "", "%s/%s=%s", tu_w(au), tu_w(bl), tu_w(hi));
        ST("Lower bound: smallest a, biggest b", "", "%s/%s=%s", tu_w(al), tu_w(bu), tu_w(lo)); break;
    }
    int bsf = op == 3 ? 5 : 8;   /* a÷b rarely ends: 5 s.f. */
    tu_ans(o, asprintf_a("\"Upper bound\"=%s, \"Lower bound\"=%s", tu_f(hi, s, bsf, -1), tu_f(lo, s, bsf, -1)), "");
    tu_val(o, hi);
}

/* ---------- rounding and estimation ---------- */

RUN(run_round)
{
    tu_begin(o);
    double x;
    int64_t n;
    if (!tu_num(o, "Number", v[0], &x) || !tu_int(o, "Places", v[2], &n)) return;
    bool sf = tu_choice("dp|sf", v[1]) == 1;
    if (n < (sf ? 1 : 0) || n > 12) { tu_fail(o, "Places out of range"); return; }
    if (x == 0) { tu_ans(o, "0", ""); tu_val(o, 0); tu_text(o, "Zero", "Zero stays zero"); return; }
    int dp = sf ? (int)n - 1 - (int)floor(log10(fabs(x))) : (int)n;
    double ax = fabs(x);
    double scaled = dp >= 0 ? ax * pow(10, dp + 1) : ax / pow(10, -dp - 1);
    int digit = (int)fmod(floor(scaled + 1e-7), 10.0);
    if (sf) tu_text(o, "Count significant figures", asprintf_a("%s to %lld s.f. means %d decimal places (start at the first non-zero digit)", tu_w(x), (long long)n, dp > 0 ? dp : 0));
    ST("Look at the next digit", "The digit after the last one you keep", "digit=%d", digit);
    ST(digit >= 5 ? "5 or more: round up" : "Less than 5: round down", "", "%s", "");
    double r = dp >= 0 ? round(x * pow(10, dp) + copysign(1e-9, x)) / pow(10, dp)
                       : round(x / pow(10, -dp)) * pow(10, -dp);
    const char *res = tu_dp(r, dp > 0 ? dp : 0);
    tu_ans(o, res, asprintf_a("\"to %lld %s\"", (long long)n, sf ? "s.f." : "d.p."));
    tu_val(o, r);
}

RUN(run_estimate)
{
    tu_begin(o);
    double a, b, c;
    if (!tu_num(o, "a", v[0], &a) || !tu_num(o, "b", v[1], &b) || !tu_num(o, "c", v[2], &c)) return;
    double ra = tu_rsf(a, 1), rb = tu_rsf(b, 1), rc = tu_rsf(c, 1);
    if (rc == 0) { tu_fail(o, "c rounds to 0"); return; }
    ST(asprintf_a("%s rounds to", tu_w(a)), "1 significant figure", "%s", tu_w(ra));
    ST(asprintf_a("%s rounds to", tu_w(b)), "1 significant figure", "%s", tu_w(rb));
    ST(asprintf_a("%s rounds to", tu_w(c)), "1 significant figure", "%s", tu_w(rc));
    double r = ra * rb / rc;
    ST("Calculate with the rounded numbers", "", "(%s×%s)/%s=%s", tu_w(ra), tu_w(rb), tu_w(rc), tu_w(r));
    tu_ans(o, tu_f(r, s, 8, -1), "");
    tu_val(o, r);
}

/* ---------- fractions ---------- */

static const char *fr(Rat r) { return rat_is_neg(r) ? asprintf_a("(%s)", tu_rs(r)) : tu_rs(r); }

RUN(run_frac_arith)
{
    tu_begin(o);
    Rat a, b, r;
    if (!tu_rat(o, "First fraction", v[0], &a) || !tu_rat(o, "Second fraction", v[2], &b)) return;
    int op = tu_choice("+|-|×|÷", v[1]);
    ST("Write as fractions", "Change mixed numbers to improper fractions", "%s %s %s", fr(a), op == 0 ? "+" : op == 1 ? "-" : op == 2 ? "×" : "÷", fr(b));
    bool ok = true;
    if (op < 2) {
        int64_t l;
        if (!i64_lcm(a.d, b.d, &l)) { tu_fail(o, "Numbers too large"); return; }
        int64_t na = a.n * (l / a.d), nb = b.n * (l / b.d);
        ST("Common denominator", "Make both denominators the same", "%lld/%lld %s %lld/%lld", (long long)na, (long long)l, op ? "-" : "+", (long long)nb, (long long)l);
        ok = op ? rat_sub(a, b, &r) : rat_add(a, b, &r);
        ST("Add or subtract the numerators", "", "%lld/%lld", (long long)(op ? na - nb : na + nb), (long long)l);
    } else if (op == 2) {
        ok = rat_mul(a, b, &r);
        ST("Multiply numerators and denominators", "", "(%lld×%lld)/(%lld×%lld)", (long long)a.n, (long long)b.n, (long long)a.d, (long long)b.d);
    } else {
        if (rat_is_zero(b)) { tu_fail(o, "Cannot divide by 0"); return; }
        Rat ib;
        ok = rat_inv(b, &ib) && rat_mul(a, ib, &r);
        ST("Keep, change, flip", "Multiply by the reciprocal", "%s×%s", fr(a), fr(ib));
    }
    if (!ok) { tu_fail(o, "Numbers too large"); return; }
    ST("Simplify", "Divide top and bottom by the HCF", "%s", tu_rs(r));
    char dec[64];
    rat_to_decimal_str(r, 8, dec, sizeof dec);
    tu_ans(o, tu_rs(r), astrdup(dec));
    tu_val(o, rat_to_double(r));
}

RUN(run_frac_of)
{
    tu_begin(o);
    Rat f;
    double amt;
    if (!tu_rat(o, "Fraction", v[0], &f) || !tu_num(o, "Amount", v[1], &amt) || !tu_nonneg(o, "Amount", amt)) return;
    double part = amt / (double)f.d, r = part * (double)f.n;
    ST("Divide by the denominator", "", "%s÷%lld=%s", tu_w(amt), (long long)f.d, tu_w(part));
    ST("Multiply by the numerator", "", "%s×%lld=%s", tu_w(part), (long long)f.n, tu_w(r));
    Rat ra, exact;
    if (rat_from_double(amt, 1000000, &ra) && rat_mul(f, ra, &exact)) tu_rat_ans(o, s, exact, 3, -1);
    else { tu_ans(o, tu_f(r, s, 3, -1), ""); tu_val(o, r); }
}

static const Tool T[] = {
    TOOL("pct_of", TOOL_CAT_NUMBER, "Percentage of an amount", "Find x% of an amount", 2, run_pct_of,
         F_NUM("Percentage", "e.g. 15", ""), F_NUM("Amount", "e.g. 240", "")),
    TOOL("pct_change", TOOL_CAT_NUMBER, "Percentage change", "Percentage increase or decrease between two values", 2, run_pct_change,
         F_NUM("Original value", "e.g. 80", ""), F_NUM("New value", "e.g. 100", "")),
    TOOL("pct_reverse", TOOL_CAT_NUMBER, "Reverse percentage", "Find the original amount before a percentage change", 3, run_pct_reverse,
         F_NUM("Price after the change", "e.g. 90", ""), F_NUM("Percentage change", "e.g. 20", ""),
         F_CH("The change was an", "increase|decrease", "increase")),
    TOOL("pct_adjust", TOOL_CAT_NUMBER, "Increase or decrease by %", "Multiplier method for a percentage change", 3, run_pct_adjust,
         F_NUM("Amount", "e.g. 250", ""), F_NUM("Percentage", "e.g. 12", ""), F_CH("Change", "increase|decrease", "increase")),
    TOOL("compound", TOOL_CAT_NUMBER, "Compound interest / growth / decay", "Repeated percentage change over several years", 4, run_compound,
         F_NUM("Starting amount", "e.g. 5000", ""), F_NUM("Rate % per year", "e.g. 3.5", ""), F_NUM("Years", "e.g. 4", ""),
         F_CH("Type", "growth|decay", "growth")),
    TOOL("simple_int", TOOL_CAT_NUMBER, "Simple interest", "Interest and total with simple interest", 3, run_simple_int,
         F_NUM("Amount", "e.g. 800", ""), F_NUM("Rate % per year", "e.g. 4", ""), F_NUM("Years", "e.g. 3", "")),
    TOOL("vat", TOOL_CAT_NUMBER, "VAT", "Add VAT to a price, or take it off", 3, run_vat,
         F_NUM("Price", "e.g. 60", ""), F_NUM("VAT rate %", "e.g. 20", "20"), F_CH("Do", "add|remove", "add")),
    TOOL("hcf_lcm", TOOL_CAT_NUMBER, "HCF and LCM", "Prime factors, Venn diagram, HCF and LCM", 2, run_hcf_lcm,
         F_NUM("First number", "e.g. 84", ""), F_NUM("Second number", "e.g. 90", "")),
    TOOL("bounds", TOOL_CAT_NUMBER, "Bounds and error interval", "Lower and upper bound of a rounded measurement", 2, run_bounds,
         F_NUM("Measurement", "e.g. 40", ""), F_NUM("Rounded to the nearest", "e.g. 10 or 0.1", "")),
    TOOL("bounds_calc", TOOL_CAT_NUMBER, "Bounds of a calculation", "Upper and lower bound of a+b, a-b, a×b or a÷b", 5, run_bounds_calc,
         F_NUM("a", "e.g. 7.3", ""), F_NUM("a rounded to nearest", "e.g. 0.1", ""),
         F_CH("Calculate", "a+b|a-b|a×b|a÷b", "a+b"),
         F_NUM("b", "e.g. 2.5", ""), F_NUM("b rounded to nearest", "e.g. 0.1", "")),
    TOOL("round", TOOL_CAT_NUMBER, "Rounding", "Round to decimal places or significant figures", 3, run_round,
         F_NUM("Number", "e.g. 3.14159", ""), F_CH("Round to", "dp|sf", "dp"), F_NUM("How many", "e.g. 2", "2")),
    TOOL("estimate", TOOL_CAT_NUMBER, "Estimate a×b÷c", "Estimate by rounding to 1 significant figure", 3, run_estimate,
         F_NUM("a", "e.g. 48.7", ""), F_NUM("b", "e.g. 3.12", ""), F_NUM("c (divide by)", "e.g. 0.52", "")),
    TOOL("frac_arith", TOOL_CAT_NUMBER, "Fraction arithmetic", "Add, subtract, multiply or divide fractions", 3, run_frac_arith,
         F_NUM("First fraction", "e.g. 3/4 or 1.5", ""), F_CH("Operation", "+|-|×|÷", "+"), F_NUM("Second fraction", "e.g. 5/6", "")),
    TOOL("frac_of", TOOL_CAT_NUMBER, "Fraction of an amount", "Find a fraction of a quantity", 2, run_frac_of,
         F_NUM("Fraction", "e.g. 3/8", ""), F_NUM("Amount", "e.g. 120", "")),
};

const Tool *tools_number(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
