/* GCSE Probability tools. Probabilities are exact fractions (Rat). */
#include "tools_util.h"

static Rat M(Rat a, Rat b) { Rat r = a; rat_mul(a, b, &r); return r; }
static Rat A(Rat a, Rat b) { Rat r = a; rat_add(a, b, &r); return r; }
static Rat S(Rat a, Rat b) { Rat r = a; rat_sub(a, b, &r); return r; }

static const char *dec(Rat r)
{
    char b[64];
    rat_to_decimal_str(r, 6, b, sizeof b);
    return astrdup(b);
}

static void out_rat(CalcResult *o, Rat r)
{
    tu_ans(o, tu_rs(r), dec(r));
    tu_val(o, rat_to_double(r));
}

static bool tu_prob(CalcResult *o, const char *label, const char *txt, Rat *p)
{
    if (!tu_rat(o, label, txt, p)) return false;
    if (rat_is_neg(*p) || rat_cmp(*p, rat_int(1)) > 0) { tu_fail(o, "%s must be between 0 and 1", label); return false; }
    return true;
}

RUN(run_complement)
{
    tu_begin(o);
    Rat p;
    if (!tu_prob(o, "P(A)", v[0], &p)) return;
    Rat r = S(rat_int(1), p);
    ST("Probabilities add up to 1", "", "P(not A)=1-P(A)");
    ST("Substitute", "", "P(not A)=1-%s=%s", tu_rs(p), tu_rs(r));
    out_rat(o, r);
}

RUN(run_combined)
{
    tu_begin(o);
    Rat p, q, r;
    if (!tu_prob(o, "P(A)", v[0], &p) || !tu_prob(o, "P(B)", v[1], &q)) return;
    int m = tu_choice("independent: A and B|independent: A or B|mutually exclusive: A or B", v[2]);
    if (m == 0) {
        r = M(p, q);
        tu_text(o, "Independent events: multiply", "P(A and B) = P(A) × P(B)");
        ST("Substitute", "", "%s×%s=%s", tu_rs(p), tu_rs(q), tu_rs(r));
    } else if (m == 1) {
        Rat both = M(p, q);
        r = S(A(p, q), both);
        tu_text(o, "Add, then take off the overlap", "P(A or B) = P(A) + P(B) - P(A and B)");
        ST("Overlap (independent)", "P(A and B) = P(A) × P(B)", "%s×%s=%s", tu_rs(p), tu_rs(q), tu_rs(both));
        ST("Substitute", "", "%s+%s-%s=%s", tu_rs(p), tu_rs(q), tu_rs(both), tu_rs(r));
    } else {
        r = A(p, q);
        tu_text(o, "Mutually exclusive: add", "P(A or B) = P(A) + P(B)");
        ST("Substitute", "", "%s+%s=%s", tu_rs(p), tu_rs(q), tu_rs(r));
        if (rat_cmp(r, rat_int(1)) > 0) { tu_fail(o, "P(A)+P(B) is more than 1: they cannot be mutually exclusive"); return; }
    }
    out_rat(o, r);
}

RUN(run_expected)
{
    tu_begin(o);
    Rat p;
    double n;
    if (!tu_prob(o, "Probability", v[0], &p) || !tu_num(o, "Number of trials", v[1], &n)) return;
    if (n < 0) { tu_fail(o, "Trials cannot be negative"); return; }
    double r = rat_to_double(p) * n;
    ST("Expected frequency", "probability × number of trials", "%s×%s=%s", tu_rp(p), tu_w(n), tu_w(r));
    Rat rn, exact;
    if (rat_from_double(n, 1000000, &rn) && rat_mul(p, rn, &exact)) tu_rat_ans(o, s, exact, 3, -1);
    else { tu_ans(o, tu_f(r, s, 3, -1), ""); tu_val(o, r); }
}

RUN(run_relative)
{
    tu_begin(o);
    int64_t k, n;
    if (!tu_int(o, "Number of successes", v[0], &k) || !tu_int(o, "Number of trials", v[1], &n)) return;
    if (n <= 0 || k < 0 || k > n) { tu_fail(o, "Need 0 ≤ successes ≤ trials, trials > 0"); return; }
    Rat r = RAT(k, n);
    ST("Relative frequency", "number of successes ÷ number of trials", "%lld/%lld=%s", (long long)k, (long long)n, tu_rs(r));
    ST("As a decimal", "", "%s", dec(r));
    tu_ans(o, tu_rs(r), dec(r));
    tu_val(o, rat_to_double(r));
}

static void tree(const char *const *v, CalcResult *o, bool replace)
{
    tu_begin(o);
    int64_t r, n;
    if (!tu_int(o, "Number of type A", v[0], &r) || !tu_int(o, "Total number", v[1], &n)) return;
    if (n < 2 || r < 0 || r > n || n > 100000) { tu_fail(o, "Need 0 ≤ type A ≤ total, and total ≥ 2"); return; }
    int64_t b = n - r, n2 = replace ? n : n - 1;
    Rat pa = RAT(r, n), pb = RAT(b, n);
    Rat a_a = RAT(replace ? r : r - 1, n2), b_a = RAT(r, n2), a_b = RAT(b, n2), b_b = RAT(replace ? b : b - 1, n2);
    if (!replace && r == 0) a_a = rat_int(0);
    if (!replace && b == 0) b_b = rat_int(0);
    ST("First pick", "", "P(A)=%s, P(B)=%s", tu_rs(pa), tu_rs(pb));
    ST("Second pick, after A first", replace ? "Put it back: the probabilities stay the same" : "Not put back: one fewer item",
       "P(A)=%s, P(B)=%s", tu_rs(a_a), tu_rs(a_b));
    ST("Second pick, after B first", "", "P(A)=%s, P(B)=%s", tu_rs(b_a), tu_rs(b_b));
    Rat AA = M(pa, a_a), AB = M(pa, a_b), BA = M(pb, b_a), BB = M(pb, b_b);
    ST("Branch AA: multiply along it", "A then A", "AA=%s×%s=%s", tu_rs(pa), tu_rs(a_a), tu_rs(AA));
    ST("Branch AB: multiply along it", "A then B", "AB=%s×%s=%s", tu_rs(pa), tu_rs(a_b), tu_rs(AB));
    ST("Branch BA: multiply along it", "B then A", "BA=%s×%s=%s", tu_rs(pb), tu_rs(b_a), tu_rs(BA));
    ST("Branch BB: multiply along it", "B then B", "BB=%s×%s=%s", tu_rs(pb), tu_rs(b_b), tu_rs(BB));
    int m = tu_choice("two of type A|one of each|at least one of type A|none of type A|both the same type", v[2]);
    Rat res;
    const char *how;
    switch (m) {
    case 0: res = AA; how = "AA"; break;
    case 1: res = A(AB, BA); how = "AB + BA"; break;
    case 2: res = S(rat_int(1), BB); how = "1 - BB"; break;
    case 3: res = BB; how = "BB"; break;
    default: res = A(AA, BB); how = "AA + BB"; break;
    }
    ST("Add the branches you need", how, "%s", tu_rs(res));
    out_rat(o, res);
}

RUN(run_tree_with) { tree(v, o, true); }
RUN(run_tree_without) { tree(v, o, false); }

RUN(run_conditional)
{
    tu_begin(o);
    Rat ab, pb;
    if (!tu_prob(o, "P(A and B)", v[0], &ab) || !tu_prob(o, "P(B)", v[1], &pb)) return;
    if (rat_is_zero(pb)) { tu_fail(o, "P(B) cannot be 0"); return; }
    Rat r;
    if (!rat_div(ab, pb, &r) || rat_cmp(r, rat_int(1)) > 0) { tu_fail(o, "P(A and B) cannot be bigger than P(B)"); return; }
    tu_text(o, "Conditional probability", "P(A given B) = P(A and B) ÷ P(B): the chance of A, given that B has happened");
    ST("Substitute", "", "%s÷%s=%s", tu_rp(ab), tu_rp(pb), tu_rs(r));
    out_rat(o, r);
}

RUN(run_venn)
{
    tu_begin(o);
    Rat a, b, ab;
    if (!tu_prob(o, "P(A)", v[0], &a) || !tu_prob(o, "P(B)", v[1], &b) || !tu_prob(o, "P(A and B)", v[2], &ab)) return;
    if (rat_cmp(ab, a) > 0 || rat_cmp(ab, b) > 0) { tu_fail(o, "P(A and B) cannot be bigger than P(A) or P(B)"); return; }
    Rat ao = S(a, ab), bo = S(b, ab), uni = S(A(a, b), ab), none = S(rat_int(1), uni);
    ST("A only", "P(A) - P(A and B)", "%s-%s=%s", tu_rs(a), tu_rs(ab), tu_rs(ao));
    ST("B only", "P(B) - P(A and B)", "%s-%s=%s", tu_rs(b), tu_rs(ab), tu_rs(bo));
    ST("A or B", "P(A) + P(B) - P(A and B)", "%s+%s-%s=%s", tu_rs(a), tu_rs(b), tu_rs(ab), tu_rs(uni));
    ST("Neither", "1 - P(A or B)", "1-%s=%s", tu_rs(uni), tu_rs(none));
    tu_ans(o, asprintf_a("\"A or B\"=%s, \"Neither\"=%s", tu_rs(uni), tu_rs(none)),
           asprintf_a("\"A only\"=%s, \"B only\"=%s", tu_rs(ao), tu_rs(bo)));
    tu_val(o, rat_to_double(uni));
}

RUN(run_counting)
{
    tu_begin(o);
    int64_t n, r = 0;
    if (!tu_int(o, "n", v[1], &n)) return;
    int m = tu_choice("nCr|nPr|n!", v[0]);
    if (m < 2 && !tu_int(o, "r", v[2], &r)) return;
    if (n < 0 || n > 20 || (m < 2 && (r < 0 || r > n))) { tu_fail(o, "Use 0 ≤ r ≤ n ≤ 20"); return; }
    long long num = 1, den = 1;
    if (m == 2) {
        for (long long i = 2; i <= n; i++) num *= i;
        ST("Factorial", "Multiply all whole numbers from 1 up to n", "%lld! =%lld", (long long)n, num);
    } else {
        for (long long i = 0; i < r; i++) num *= (n - i);
        for (long long i = 2; i <= r; i++) den *= i;
        if (m == 0) {
            tu_text(o, "Combinations (order does not matter)", "nCr = n! ÷ (r! × (n-r)!)");
            ST("Top: r numbers counting down from n", "", "%lld/%lld", num, den);
            num /= den;
        } else {
            tu_text(o, "Permutations (order matters)", "nPr = n! ÷ (n-r)!");
            ST("r numbers counting down from n", "", "%lld", num);
        }
    }
    /* "5! =120": the space keeps "!=" from reading as "not equal" */
    if (m == 2) ST("Answer", "", "%lld! =%lld", (long long)n, num);
    else ST("Answer", "", "%s(%lld, %lld)=%lld", m == 0 ? "nCr" : "nPr", (long long)n, (long long)r, num);
    tu_ans(o, asprintf_a("%lld", num), "");
    tu_val(o, (double)num);
}

static const Tool T[] = {
    TOOL("prob_not", TOOL_CAT_PROB, "Probability of not A", "Complement: 1 - P(A)", 1, run_complement,
         F_NUM("P(A)", "e.g. 0.35 or 7/20", "")),
    TOOL("prob_combined", TOOL_CAT_PROB, "A and B / A or B", "Combine two probabilities", 3, run_combined,
         F_NUM("P(A)", "e.g. 0.3", ""), F_NUM("P(B)", "e.g. 0.5", ""),
         F_CH("Events are", "independent: A and B|independent: A or B|mutually exclusive: A or B", "independent: A and B")),
    TOOL("prob_expected", TOOL_CAT_PROB, "Expected frequency", "How many times to expect an outcome", 2, run_expected,
         F_NUM("Probability", "e.g. 0.15", ""), F_NUM("Number of trials", "e.g. 200", "")),
    TOOL("prob_relative", TOOL_CAT_PROB, "Relative frequency", "Experimental probability from results", 2, run_relative,
         F_NUM("Number of successes", "e.g. 18", ""), F_NUM("Number of trials", "e.g. 60", "")),
    TOOL("tree_with", TOOL_CAT_PROB, "Tree diagram: with replacement", "Two picks, item put back", 3, run_tree_with,
         F_NUM("Number of type A", "e.g. 3", ""), F_NUM("Total number", "e.g. 8", ""),
         F_CH("Find the probability of", "two of type A|one of each|at least one of type A|none of type A|both the same type", "two of type A")),
    TOOL("tree_without", TOOL_CAT_PROB, "Tree diagram: without replacement", "Two picks, item not put back", 3, run_tree_without,
         F_NUM("Number of type A", "e.g. 3", ""), F_NUM("Total number", "e.g. 8", ""),
         F_CH("Find the probability of", "two of type A|one of each|at least one of type A|none of type A|both the same type", "two of type A")),
    TOOL("prob_cond", TOOL_CAT_PROB, "Conditional probability", "P(A | B) from P(A and B) and P(B)", 2, run_conditional,
         F_NUM("P(A and B)", "e.g. 1/10", ""), F_NUM("P(B)", "e.g. 1/4", "")),
    TOOL("prob_venn", TOOL_CAT_PROB, "Venn diagram probabilities", "A or B, neither, A only, B only", 3, run_venn,
         F_NUM("P(A)", "e.g. 0.5", ""), F_NUM("P(B)", "e.g. 0.4", ""), F_NUM("P(A and B)", "e.g. 0.2", "")),
    TOOL("counting", TOOL_CAT_PROB, "Counting: nCr, nPr, n!", "Combinations, permutations, factorials (n up to 20)", 3, run_counting,
         F_CH("Calculate", "nCr|nPr|n!", "nCr"), F_NUM("n", "e.g. 10", ""), F_OPT("r (not needed for n!)", "e.g. 3")),
};

const Tool *tools_prob(int *n) { *n = (int)(sizeof T / sizeof T[0]); return T; }
