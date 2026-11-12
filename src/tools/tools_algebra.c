/* GCSE Algebra tools: sequences, iteration, trapezium rule. */
#include "tools_util.h"

static bool same(double a, double b) { return fabs(a - b) <= 1e-9 * (1 + fabs(a) + fabs(b)); }

static const char *dl(const double *a, int n)
{
    char b[300];
    size_t len = 0;
    b[0] = 0;
    for (int i = 0; i < n && len < sizeof b - 30; i++)
        len += (size_t)snprintf(b + len, sizeof b - len, "%s%s", i ? ", " : "", tu_w(a[i]));
    return astrdup(b);
}

RUN(run_nth_linear)
{
    tu_begin(o);
    double t[TU_MAXLIST], d[TU_MAXLIST];
    int n;
    if (!tu_list(o, "Terms", v[0], t, TU_MAXLIST, &n)) return;
    if (n < 3) { tu_fail(o, "Give at least 3 terms"); return; }
    for (int i = 0; i + 1 < n; i++) d[i] = t[i + 1] - t[i];
    for (int i = 1; i + 1 < n; i++) if (!same(d[i], d[0])) { tu_fail(o, "The differences are not constant: not a linear sequence"); return; }
    double c = t[0] - d[0], co[2] = { d[0], c };
    ST("Find the common difference", "Subtract each term from the next", "%s", dl(d, n - 1));
    ST("Coefficient of n", "nth term = dn + c, where d is the common difference", "%sn+c", tu_w(d[0]));
    ST("Find c", "First term - common difference", "c=%s-%s=%s", tu_w(t[0]), tu_w(d[0]), tu_w(c));
    const char *nt = tu_poly(co, 1, "n");
    ST("nth term", "", "%s", nt);
    ST("Check with n = 1", "", "%s", asprintf_a("%s×1%s%s=%s", tu_w(d[0]), c < 0 ? "-" : "+", tu_w(fabs(c)), tu_w(t[0])));
    tu_ans(o, nt, "");
}

RUN(run_nth_quad)
{
    tu_begin(o);
    double t[TU_MAXLIST], d1[TU_MAXLIST], d2[TU_MAXLIST], rem[TU_MAXLIST];
    int n;
    if (!tu_list(o, "Terms", v[0], t, TU_MAXLIST, &n)) return;
    if (n < 4) { tu_fail(o, "Give at least 4 terms"); return; }
    for (int i = 0; i + 1 < n; i++) d1[i] = t[i + 1] - t[i];
    for (int i = 0; i + 2 < n; i++) d2[i] = d1[i + 1] - d1[i];
    for (int i = 1; i + 2 < n; i++) if (!same(d2[i], d2[0])) { tu_fail(o, "The second differences are not constant: not a quadratic sequence"); return; }
    double a = d2[0] / 2;
    ST("First differences", "", "%s", dl(d1, n - 1));
    if (same(d2[0], 0)) {   /* no n² part: say so, and give the linear nth term */
        ST("Second differences", "All 0: so there is no n² term", "%s", dl(d2, n - 2));
        double c1 = t[0] - d1[0], co1[2] = { d1[0], c1 };
        const char *nt1 = tu_poly(co1, 1, "n");
        tu_text(o, "This sequence is linear", asprintf_a("The second difference is 0, so it is a linear sequence (common difference %s), not a quadratic one", tu_w(d1[0])));
        ST("nth term", "dn + c, with d the common difference and c = first term - d", "%s", nt1);
        tu_ans(o, nt1, "\"linear: second difference 0\"");
        return;
    }
    ST("Second differences", "Constant: so the sequence is quadratic", "%s", dl(d2, n - 2));
    ST("Coefficient of n²", "Half the second difference", "a=%s/2=%s", tu_w(d2[0]), tu_w(a));
    for (int i = 0; i < n; i++) rem[i] = t[i] - a * (double)(i + 1) * (double)(i + 1);
    ST("Subtract an² from each term", "", "%s", dl(rem, n));
    double b = rem[1] - rem[0], c = rem[0] - b;
    ST("This is linear: find its nth term", "", "%sn%s%s", tu_w(b), c < 0 ? "-" : "+", tu_w(fabs(c)));
    double co[3] = { a, b, c };
    const char *nt = tu_poly(co, 2, "n");
    ST("nth term", "Put the parts together", "%s", nt);
    tu_ans(o, nt, "");
}

RUN(run_geometric)
{
    tu_begin(o);
    double t[TU_MAXLIST];
    int n;
    if (!tu_list(o, "Terms", v[0], t, TU_MAXLIST, &n)) return;
    if (n < 3 || t[0] == 0) { tu_fail(o, "Give at least 3 terms, first one not 0"); return; }
    double r = t[1] / t[0];
    for (int i = 1; i + 1 < n; i++) if (t[i] == 0 || !same(t[i + 1] / t[i], r)) { tu_fail(o, "There is no common ratio: not a geometric sequence"); return; }
    ST("Common ratio", "Divide each term by the one before", "%s/%s=%s", tu_w(t[1]), tu_w(t[0]), tu_w(r));
    ST("Formula", "first term × ratio^(n-1)", "a×r^(n-1)");
    Rat rr;
    const char *rs = tu_w(r);
    if (fabs(r - round(r)) > 1e-9 && rat_from_double(r, 1000, &rr)) rs = asprintf_a("(%s)", tu_rs(rr));
    else if (r < 0) rs = asprintf_a("(%s)", rs);
    const char *nt = asprintf_a("%s×%s^(n-1)", tu_w(t[0]), rs);
    ST("nth term", "", "%s", nt);
    double next = t[n - 1] * r;
    ST("Next term", "Multiply the last term by the ratio", "%s×%s=%s", tu_w(t[n - 1]), tu_w(r), tu_w(next));
    tu_ans(o, nt, asprintf_a("\"next term\" %s", tu_w(next)));
    tu_val(o, next);
}

RUN(run_iteration)
{
    tu_begin(o);
    double x;
    int64_t k;
    Expr *e = tu_expr(o, "Formula", v[0]);
    if (!e || !tu_num(o, "Starting value x0", v[1], &x) || !tu_int(o, "Number of iterations", v[2], &k)) return;
    if (k < 1 || k > 30) { tu_fail(o, "Use 1 to 30 iterations"); return; }
    ST("Iteration formula", "x_(n+1) is f(x_n): put each answer back into the formula", "%s", v[0]);
    ST("Starting value x_0", "", "%s", tu_w(x));
    for (int i = 1; i <= k; i++) {
        double nx;
        if (!tu_eval_x(e, "x", x, &nx)) { tu_fail(o, "x_%d is not defined (maths error)", i); return; }
        ST(asprintf_a("x_%d = f(x_%d)", i, i - 1), "", "%s", tu_sf(nx, 9));
        x = nx;
    }
    tu_ans(o, tu_f(x, s, 9, -1), "");
    tu_val(o, x);
}

RUN(run_trapezium)
{
    tu_begin(o);
    double a, b;
    int64_t n;
    Expr *e = tu_expr(o, "Function", v[0]);
    if (!e || !tu_num(o, "From x =", v[1], &a) || !tu_num(o, "To x =", v[2], &b) || !tu_int(o, "Number of strips", v[3], &n)) return;
    if (n < 1 || n > 20 || b <= a) { tu_fail(o, "Need to > from, and 1 to 20 strips"); return; }
    double h = (b - a) / (double)n, y[21];
    ST("Strip width", "(upper - lower) ÷ number of strips", "h=(%s-%s)/%lld=%s", tu_w(b), tu_w(a), (long long)n, tu_w(h));
    double mid = 0;
    for (int i = 0; i <= n; i++) {
        double xi = a + h * (double)i;
        if (!tu_eval_x(e, "x", xi, &y[i])) { tu_fail(o, "f(%s) is not defined", tu_w(xi)); return; }
        ST(asprintf_a("y%d", i), asprintf_a("x = %s", tu_w(xi)), "%s", tu_w(y[i]));
        if (i > 0 && i < n) mid += y[i];
    }
    double area = h / 2 * (y[0] + y[n] + 2 * mid);
    tu_text(o, "Trapezium rule", "Area = h/2 × (first y + last y + 2 × sum of the other y values)");
    ST("Substitute", "", "A=%s/2×(%s+%s+2×%s)=%s", tu_w(h), tu_w(y[0]), tu_w(y[n]), tu_w(mid), tu_w(area));
    tu_ans(o, tu_f(area, s, 4, -1), "");
    tu_val(o, area);
}

static const Tool T[] = {
    TOOL("nth_linear", TOOL_CAT_ALGEBRA, "nth term: linear sequence", "From the first few terms", 1, run_nth_linear,
         F_LIST("Terms", "e.g. 5, 8, 11, 14", "")),
    TOOL("nth_quad", TOOL_CAT_ALGEBRA, "nth term: quadratic sequence", "From at least 4 terms", 1, run_nth_quad,
         F_LIST("Terms", "e.g. 4, 9, 16, 25, 36", "")),
    TOOL("geometric", TOOL_CAT_ALGEBRA, "Geometric sequence", "Common ratio, nth term, next term", 1, run_geometric,
         F_LIST("Terms", "e.g. 3, 6, 12, 24", "")),
    TOOL("iteration", TOOL_CAT_ALGEBRA, "Iteration", "Repeat x → f(x) and see where it settles", 3, run_iteration,
         F_EXPR("Formula f(x)", "e.g. (3x+1)^(1/3)", ""), F_NUM("Starting value x0", "e.g. 1", "1"), F_NUM("Iterations", "e.g. 5", "5")),
    TOOL("trapezium", TOOL_CAT_ALGEBRA, "Trapezium rule", "Estimate area under a curve", 4, run_trapezium,
         F_EXPR("Function f(x)", "e.g. x^2+1", ""), F_NUM("From x =", "e.g. 0", ""), F_NUM("To x =", "e.g. 4", ""),
         F_NUM("Number of strips", "e.g. 4", "4")),
};

const Tool *tools_algebra(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
