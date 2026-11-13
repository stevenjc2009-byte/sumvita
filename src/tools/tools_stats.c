/* GCSE Statistics tools. */
#include "tools_util.h"

static void sort_d(double *a, int n)
{
    for (int i = 1; i < n; i++) {
        double x = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; j--; }
        a[j + 1] = x;
    }
}

/* mean or similar in the answer: tool default s.f. unless that would round whole digits off */
static const char *ansv(double x, const CalcSettings *s)
{
    if (fabs(x) >= 1000 && s->dp < 0 && s->sf <= 0) return tu_wx(x);
    return tu_f(x, s, 4, -1);
}

static double median_of(const double *a, int n)   /* sorted */
{
    return n % 2 ? a[n / 2] : (a[n / 2 - 1] + a[n / 2]) / 2;
}

static const char *dl(const double *a, int n, const CalcSettings *s, bool answer)
{
    char b[400];
    size_t len = 0;
    b[0] = 0;
    for (int i = 0; i < n && len < sizeof b - 40; i++)
        len += (size_t)snprintf(b + len, sizeof b - len, "%s%s", i ? ", " : "", answer ? ansv(a[i], s) : tu_wx(a[i]));
    return astrdup(b);
}

/* modes of a list of (value, weight) pairs, sorted by value; "no mode" if all weights equal */
static const char *modes(const double *x, const double *w, int n)
{
    double best = 0;
    for (int i = 0; i < n; i++) if (w[i] > best) best = w[i];
    int cnt = 0;
    for (int i = 0; i < n; i++) if (w[i] == best) cnt++;
    if (cnt == n && n > 1) return "none";
    char b[200];
    size_t len = 0;
    b[0] = 0;
    for (int i = 0; i < n && len < sizeof b - 30; i++)
        if (w[i] == best) len += (size_t)snprintf(b + len, sizeof b - len, "%s%s", len ? " and " : "", tu_wx(x[i]));
    return astrdup(b);
}

/* 5+8-3+9 for the working; just the total when the list is too long to show */
static const char *sum_expr(const double *a, int n)
{
    char b[400];
    size_t len = 0;
    b[0] = 0;
    for (int i = 0; i < n; i++) {
        const char *w = tu_wx(a[i]);
        len += (size_t)snprintf(b + len, sizeof b - len, "%s%s", i && a[i] >= 0 ? "+" : "", w);
        if (len >= sizeof b - 20) { double t = 0; for (int k = 0; k < n; k++) t += a[k]; return tu_wx(t); }
    }
    return astrdup(b);
}

RUN(run_list_stats)
{
    tu_begin(o);
    double a[TU_MAXLIST];
    int n;
    if (!tu_list(o, "Data", v[0], a, TU_MAXLIST, &n)) return;
    sort_d(a, n);
    double sum = 0;
    for (int i = 0; i < n; i++) sum += a[i];
    double mean = sum / n, med = median_of(a, n), rng = a[n - 1] - a[0];
    double w[TU_MAXLIST], xs[TU_MAXLIST];
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (k && xs[k - 1] == a[i]) w[k - 1]++;
        else { xs[k] = a[i]; w[k] = 1; k++; }
    }
    const char *mo = modes(xs, w, k);
    ST("Put the numbers in order", "", "%s", dl(a, n, s, false));
    ST("Mean", "Add up all the values, divide by how many", "(%s)/%d=%s/%d=%s", sum_expr(a, n), n, tu_wx(sum), n, tu_wx(mean));
    ST("Median", n % 2 ? "The middle value" : "Halfway between the two middle values", "%s", tu_wx(med));
    tu_text(o, "Mode", asprintf_a("The most common value: %s", mo));
    ST("Range", "Biggest - smallest", "%s-%s=%s", tu_wx(a[n - 1]), tu_wx(a[0]), tu_wx(rng));
    tu_ans(o, asprintf_a("\"Mean\"=%s, \"Median\"=%s, \"Range\"=%s", ansv(mean, s), tu_wx(med), tu_wx(rng)),
           asprintf_a("\"Mode\"=%s", mo));
    tu_val(o, mean);
}

RUN(run_freq_table)
{
    tu_begin(o);
    double x[TU_MAXLIST], f[TU_MAXLIST];
    int n, nf;
    if (!tu_list(o, "Values", v[0], x, TU_MAXLIST, &n) || !tu_list(o, "Frequencies", v[1], f, TU_MAXLIST, &nf)) return;
    if (n != nf) { tu_fail(o, "Values and frequencies need the same count"); return; }
    for (int i = 0; i < n; i++) if (f[i] < 0 || fabs(f[i] - round(f[i])) > 1e-9) { tu_fail(o, "Frequencies are whole numbers"); return; }
    for (int i = 1; i < n; i++)   /* sort by value */
        for (int j = i; j > 0 && x[j - 1] > x[j]; j--) {
            double t = x[j]; x[j] = x[j - 1]; x[j - 1] = t;
            t = f[j]; f[j] = f[j - 1]; f[j - 1] = t;
        }
    double N = 0, fx = 0, fxs[TU_MAXLIST];
    for (int i = 0; i < n; i++) { N += f[i]; fxs[i] = f[i] * x[i]; fx += fxs[i]; }
    if (N <= 0) { tu_fail(o, "Total frequency is 0"); return; }
    double mean = fx / N;
    ST("Total frequency", "Add the frequencies", "%s", tu_wx(N));
    ST("Multiply value × frequency", "", "%s", dl(fxs, n, s, false));
    ST("Mean", "Total of (value × frequency) ÷ total frequency", "%s/%s=%s", tu_wx(fx), tu_wx(N), tu_wx(mean));
    double pos = (N + 1) / 2, cum = 0, med = x[n - 1];
    int lo = (int)floor(pos), hi = (int)ceil(pos);
    double vlo = x[n - 1], vhi = x[n - 1];
    bool glo = false, ghi = false;
    for (int i = 0; i < n; i++) {
        cum += f[i];
        if (!glo && cum >= lo) { vlo = x[i]; glo = true; }
        if (!ghi && cum >= hi) { vhi = x[i]; ghi = true; }
    }
    med = (vlo + vhi) / 2;
    ST("Median", asprintf_a("The value in position (N+1)/2 = %s", tu_wx(pos)), "%s", tu_wx(med));
    const char *mo = modes(x, f, n);
    tu_text(o, "Mode", asprintf_a("The value with the highest frequency: %s", mo));
    ST("Range", "", "%s-%s=%s", tu_wx(x[n - 1]), tu_wx(x[0]), tu_wx(x[n - 1] - x[0]));
    tu_ans(o, asprintf_a("\"Mean\"=%s, \"Median\"=%s, \"Range\"=%s", ansv(mean, s), tu_wx(med), tu_wx(x[n - 1] - x[0])),
           asprintf_a("\"Mode\"=%s", mo));
    tu_val(o, mean);
}

RUN(run_grouped)
{
    tu_begin(o);
    double lo[TU_MAXLIST], up[TU_MAXLIST], f[TU_MAXLIST], mid[TU_MAXLIST], fm[TU_MAXLIST];
    int n, n2, n3;
    if (!tu_list(o, "Lower bounds", v[0], lo, TU_MAXLIST, &n) || !tu_list(o, "Upper bounds", v[1], up, TU_MAXLIST, &n2) ||
        !tu_list(o, "Frequencies", v[2], f, TU_MAXLIST, &n3)) return;
    if (n != n2 || n != n3) { tu_fail(o, "Give the same number of lower bounds, upper bounds and frequencies"); return; }
    double N = 0, tot = 0;
    int modal = 0;
    for (int i = 0; i < n; i++) {
        if (up[i] <= lo[i]) { tu_fail(o, "Class %d: the upper bound %s must be above the lower bound %s", i + 1, tu_w(up[i]), tu_w(lo[i])); return; }
        if (f[i] < 0) { tu_fail(o, "Class %d: the frequency cannot be negative", i + 1); return; }
        if (i && lo[i] < up[i - 1]) {
            tu_fail(o, "Class %d starts at %s but class %d ends at %s: classes must be in order and not overlap", i + 1, tu_w(lo[i]), i, tu_w(up[i - 1]));
            return;
        }
        mid[i] = (lo[i] + up[i]) / 2;
        fm[i] = mid[i] * f[i];
        N += f[i]; tot += fm[i];
        if (f[i] > f[modal]) modal = i;
    }
    if (N <= 0) { tu_fail(o, "Total frequency is 0"); return; }
    double mean = tot / N, pos = (N + 1) / 2, cum = 0;
    int med = n - 1;
    for (int i = 0; i < n; i++) { cum += f[i]; if (cum >= pos) { med = i; break; } }
    ST("Midpoint of each class", "", "%s", dl(mid, n, s, false));
    ST("Midpoint × frequency", "", "%s", dl(fm, n, s, false));
    ST("Estimated mean", "Total of (midpoint × frequency) ÷ total frequency", "%s/%s=%s", tu_w(tot), tu_w(N), tu_w(mean));
    ST("Modal class", "The class with the highest frequency", "%s≤x<%s", tu_w(lo[modal]), tu_w(up[modal]));
    ST("Median class", asprintf_a("Contains the value in position (N+1)/2 = %s", tu_w(pos)), "%s≤x<%s", tu_w(lo[med]), tu_w(up[med]));
    tu_ans(o, tu_f(mean, s, 4, -1), asprintf_a("\"Modal class\" %s≤x<%s", tu_w(lo[modal]), tu_w(up[modal])));
    tu_val(o, mean);
}

RUN(run_quartiles)
{
    tu_begin(o);
    double a[TU_MAXLIST];
    int n;
    if (!tu_list(o, "Data", v[0], a, TU_MAXLIST, &n)) return;
    if (n < 3) { tu_fail(o, "Need at least 3 values"); return; }
    sort_d(a, n);
    int h = n / 2;
    double q2 = median_of(a, n), q1 = median_of(a, h), q3 = median_of(a + (n - h), h), iqr = q3 - q1;
    ST("Put the numbers in order", "", "%s", dl(a, n, s, false));
    ST("Median (Q2)", "The middle value", "%s", tu_w(q2));
    ST("Lower quartile (Q1)", n % 2 ? "Median of the lower half (leave out the middle value)" : "Median of the lower half", "%s", tu_w(q1));
    ST("Upper quartile (Q3)", n % 2 ? "Median of the upper half (leave out the middle value)" : "Median of the upper half", "%s", tu_w(q3));
    ST("Interquartile range", "Q3 - Q1", "%s-%s=%s", tu_w(q3), tu_w(q1), tu_w(iqr));
    tu_ans(o, asprintf_a("\"Q1\"=%s, \"Q2\"=%s, \"Q3\"=%s, \"IQR\"=%s", tu_w(q1), tu_w(q2), tu_w(q3), tu_w(iqr)),
           asprintf_a("\"Min\"=%s, \"Max\"=%s", tu_w(a[0]), tu_w(a[n - 1])));
    tu_val(o, iqr);
}

RUN(run_freq_density)
{
    tu_begin(o);
    double lo, up, x;
    if (!tu_num(o, "Class lower bound", v[1], &lo) || !tu_num(o, "Class upper bound", v[2], &up) || !tu_num(o, "Value", v[3], &x)) return;
    if (up <= lo) { tu_fail(o, "Upper bound must be above lower bound"); return; }
    bool fd = tu_choice("frequency density|frequency", v[0]) == 0;
    if (!tu_nonneg(o, fd ? "Frequency" : "Frequency density", x)) return;
    double w = up - lo;
    ST("Class width", "Upper bound - lower bound", "%s-%s=%s", tu_w(up), tu_w(lo), tu_w(w));
    ST("Frequency density", "frequency ÷ class width", "FD=f/w");
    double r;
    if (fd) {
        r = x / w;
        ST("Substitute", "", "FD=%s/%s=%s", tu_w(x), tu_w(w), tu_w(r));
    } else {
        r = x * w;
        ST("Rearrange: frequency = FD × class width", "", "f=%s×%s=%s", tu_w(x), tu_w(w), tu_w(r));
    }
    tu_ans(o, tu_f(r, s, 6, -1), "");
    tu_val(o, r);
}

RUN(run_pie)
{
    tu_begin(o);
    double f[TU_MAXLIST], ang[TU_MAXLIST];
    int n;
    if (!tu_list(o, "Frequencies", v[0], f, TU_MAXLIST, &n)) return;
    double tot = 0;
    for (int i = 0; i < n; i++) { if (f[i] < 0) { tu_fail(o, "Frequencies cannot be negative"); return; } tot += f[i]; }
    if (tot <= 0) { tu_fail(o, "Total is 0"); return; }
    ST("Total frequency", "", "%s", tu_w(tot));
    ST("Degrees for one item", "360° ÷ total", "360/%s=%s", tu_w(tot), tu_w(360 / tot));
    for (int i = 0; i < n; i++) {
        ang[i] = f[i] / tot * 360;
        ST(asprintf_a("Sector %d", i + 1), "frequency × degrees per item", "%s×%s=%s°", tu_w(f[i]), tu_w(360 / tot), tu_w(ang[i]));
    }
    char b[300];
    size_t len = 0;
    b[0] = 0;
    for (int i = 0; i < n && len < sizeof b - 30; i++)
        len += (size_t)snprintf(b + len, sizeof b - len, "%s%s°", i ? ", " : "", tu_f(ang[i], s, 4, -1));
    tu_ans(o, astrdup(b), "");
}

RUN(run_stratified)
{
    tu_begin(o);
    double g[TU_MAXLIST], sample;
    int n;
    if (!tu_list(o, "Group sizes", v[0], g, TU_MAXLIST, &n) || !tu_num(o, "Sample size", v[1], &sample)) return;
    double tot = 0;
    for (int i = 0; i < n; i++) {
        if (g[i] < 0) { tu_fail(o, "Group %d: the size cannot be negative", i + 1); return; }
        tot += g[i];
    }
    if (tot <= 0) { tu_fail(o, "Group sizes must add up to more than 0"); return; }
    if (sample <= 0 || fabs(sample - round(sample)) > 1e-9) { tu_fail(o, "Sample size must be a positive whole number"); return; }
    if (sample > tot) { tu_fail(o, "The sample (%s) cannot be larger than the population (%s)", tu_w(sample), tu_w(tot)); return; }
    ST("Total population", "", "%s", tu_w(tot));
    double x[TU_MAXLIST], r[TU_MAXLIST], rsum = 0;
    for (int i = 0; i < n; i++) {
        x[i] = g[i] / tot * sample;
        r[i] = round(x[i]);
        rsum += r[i];
        ST(asprintf_a("Group %d", i + 1), asprintf_a("group size ÷ total × sample size, rounded to %s", tu_w(r[i])), "%s/%s×%s=%s", tu_w(g[i]), tu_w(tot), tu_w(sample), tu_w(x[i]));
    }
    if (fabs(rsum - sample) > 1e-9) {
        /* largest remainder: round every group down, then give the spare places to the biggest remainders (a tie goes to the bigger group) */
        double left = sample;
        for (int i = 0; i < n; i++) { r[i] = floor(x[i] + 1e-9); left -= r[i]; }
        for (; left > 0.5; left--) {
            int best = -1;
            for (int i = 0; i < n; i++) {
                double ri = x[i] - r[i], rb = best < 0 ? 0 : x[best] - r[best];
                if (best < 0 || ri > rb + 1e-9 || (fabs(ri - rb) <= 1e-9 && g[i] > g[best])) best = i;
            }
            r[best] += 1;
            x[best] = r[best];   /* its remainder is used up */
        }
        tu_text(o, "Adjust", asprintf_a("Rounding gave a total of %s, not %s: round every group down, then add 1 to the largest remainders until the total is %s.",
                                        tu_w(rsum), tu_w(sample), tu_w(sample)));
    }
    char b[300];
    size_t len = 0;
    b[0] = 0;
    for (int i = 0; i < n; i++) len += (size_t)snprintf(b + len, sizeof b - len, "%s%s", i ? ", " : "", tu_w(r[i]));
    tu_ans(o, astrdup(b), "");
}

RUN(run_capture)
{
    tu_begin(o);
    double m1, n2, m2;
    if (!tu_num(o, "Marked in first catch", v[0], &m1) || !tu_num(o, "Second catch", v[1], &n2) || !tu_num(o, "Marked in second catch", v[2], &m2)) return;
    if (!tu_pos(o, "Marked in first catch", m1) || !tu_pos(o, "Second catch", n2) || !tu_pos(o, "Marked in second catch", m2)) return;
    if (m2 > n2) { tu_fail(o, "Marked in second catch (%s) can't be more than the second catch (%s)", tu_w(m2), tu_w(n2)); return; }
    double N = m1 * n2 / m2;
    tu_text(o, "Capture-recapture", "Assume the marked fraction of the second catch matches the marked fraction of the whole population: (marked first) / N = (marked second) / (second catch)");
    ST("Substitute", "", "%s/N=%s/%s", tu_w(m1), tu_w(m2), tu_w(n2));
    ST("Rearrange", "", "N=%s×%s/%s=%s", tu_w(m1), tu_w(n2), tu_w(m2), tu_w(N));
    tu_ans(o, tu_f(N, s, 4, -1), "");
    tu_val(o, N);
}

RUN(run_moving)
{
    tu_begin(o);
    double a[TU_MAXLIST], r[TU_MAXLIST];
    int n;
    int64_t k;
    if (!tu_list(o, "Data", v[0], a, TU_MAXLIST, &n) || !tu_int(o, "Period", v[1], &k)) return;
    if (k < 2 || k > n) { tu_fail(o, "Period must be from 2 to %d", n); return; }
    int cnt = n - (int)k + 1;
    for (int i = 0; i < cnt; i++) {
        double sum = 0;
        for (int j = 0; j < k; j++) sum += a[i + j];
        r[i] = sum / (double)k;
        ST(asprintf_a("Average %d", i + 1), asprintf_a("Mean of values %d to %d", i + 1, i + (int)k), "%s/%lld=%s", tu_w(sum), (long long)k, tu_w(r[i]));
    }
    tu_ans(o, dl(r, cnt, s, true), "");
    tu_val(o, r[cnt - 1]);
}

static const Tool T[] = {
    TOOL("stat_list", TOOL_CAT_STATS, "Mean, median, mode, range", "Averages and range of a list", 1, run_list_stats,
         F_LIST("Data", "e.g. 4, 7, 7, 9, 12", "")),
    TOOL("stat_freq", TOOL_CAT_STATS, "Averages from a frequency table", "Mean, median, mode, range from values and frequencies", 2, run_freq_table,
         F_LIST("Values", "e.g. 1, 2, 3, 4", ""), F_LIST("Frequencies", "e.g. 5, 8, 4, 3", "")),
    TOOL("stat_grouped", TOOL_CAT_STATS, "Estimated mean (grouped data)", "Midpoints, modal and median class", 3, run_grouped,
         F_LIST("Class lower bounds", "e.g. 0, 10, 20", ""), F_LIST("Class upper bounds", "e.g. 10, 20, 30", ""), F_LIST("Frequencies", "e.g. 4, 9, 7", "")),
    TOOL("stat_quartiles", TOOL_CAT_STATS, "Quartiles and IQR", "Q1, median, Q3, interquartile range", 1, run_quartiles,
         F_LIST("Data", "e.g. 3, 5, 6, 8, 9, 12, 14", "")),
    TOOL("freq_density", TOOL_CAT_STATS, "Frequency density", "For histograms: frequency ÷ class width", 4, run_freq_density,
         F_CH("Find", "frequency density|frequency", "frequency density"), F_NUM("Class lower bound", "e.g. 10", ""),
         F_NUM("Class upper bound", "e.g. 30", ""), F_NUM("Frequency or frequency density", "e.g. 24", "")),
    TOOL("pie_angles", TOOL_CAT_STATS, "Pie chart angles", "Angle of each sector", 1, run_pie,
         F_LIST("Frequencies", "e.g. 12, 18, 6, 24", "")),
    TOOL("stratified", TOOL_CAT_STATS, "Stratified sampling", "How many to sample from each group", 2, run_stratified,
         F_LIST("Group sizes", "e.g. 120, 80, 200", ""), F_NUM("Total sample size", "e.g. 40", "")),
    TOOL("capture", TOOL_CAT_STATS, "Capture-recapture", "Estimate a population size", 3, run_capture,
         F_NUM("Marked in first catch", "e.g. 30", ""), F_NUM("Size of second catch", "e.g. 50", ""), F_NUM("Marked in second catch", "e.g. 6", "")),
    TOOL("moving_avg", TOOL_CAT_STATS, "Moving averages", "Mean of each run of values", 2, run_moving,
         F_LIST("Data in time order", "e.g. 12, 15, 11, 18, 20", ""), F_NUM("Period", "e.g. 3", "3")),
};

const Tool *tools_stats(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
