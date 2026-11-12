/* alg_factor: every GCSE method, exact display strings, working steps, the check step,
 * idempotence, unfactorisable inputs, errors, and expand(factor(p)) == p for 30+ polynomials. */
#include "testlib.h"
#include "engine/algebra.h"
#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/poly.h"

static Steps st;
static char err[128];

static const char *F(const char *in)
{
    Expr *e = ex_parse(in, NULL, 0, NULL), *out = NULL;
    if (!e) return "PARSE";
    steps_init(&st);
    err[0] = 0;
    return alg_factor(e, &st, &out, err, sizeof err) ? ex_print(out) : asprintf_a("ERR:%s", err);
}

static bool has_title(const char *t)
{
    for (int i = 0; i < st.n; i++)
        if (strncmp(st.v[i].title, t, strlen(t)) == 0) return true;
    return false;
}

/* any step's title or note contains t */
static bool has_note_f1(const char *t)
{
    for (int i = 0; i < st.n; i++)
        if (strstr(st.v[i].title, t) || (st.v[i].note && strstr(st.v[i].note, t))) return true;
    return false;
}

static bool has_math(const char *m)
{
    for (int i = 0; i < st.n; i++)
        if (st.v[i].math && strstr(st.v[i].math, m)) return true;
    return false;
}

static void exact(void)
{
    static const char *cases[][2] = {
        {"y(2x+4)", "2y(x+2)"},
        {"2x^2y-8y+3x^2-12", "(2y+3)(x+2)(x-2)"},
        {"x^4-16", "(x^2+4)(x+2)(x-2)"},
        {"6x^2+x-2", "(2x-1)(3x+2)"},
        {"x^3-6x^2+11x-6", "(x-1)(x-2)(x-3)"},
        {"6x^2y+9xy^2", "3xy(2x+3y)"},
        {"x^2+5x+6", "(x+2)(x+3)"},
        {"x^2-x-6", "(x+2)(x-3)"},
        {"-x^2+5x-6", "-(x-2)(x-3)"},
        {"4x^2-9y^2", "(2x+3y)(2x-3y)"},
        {"50-2x^2", "2(5+x)(5-x)"},
        {"x^2+6x+9", "(x+3)^2"},
        {"4x^2+12x+9", "(2x+3)^2"},
        {"ax+ay+bx+by", "(x+y)(a+b)"},
        {"2x^3+3x^2-8x+3", "(x-1)(x+3)(2x-1)"},
        {"x^3-8", "(x-2)(x^2+2x+4)"},
        {"8-x^3", "(2-x)(4+2x+x^2)"},
        {"x^2+5xy+6y^2", "(x+2y)(x+3y)"},
        {"x^4-5x^2+4", "(x+1)(x-1)(x+2)(x-2)"},
        {"x^4+4", "(x^2-2x+2)(x^2+2x+2)"},
        {"x^6-1", "(x+1)(x^2-x+1)(x-1)(x^2+x+1)"},
        {"x^5+x+1", "(x^2+x+1)(x^3-x^2+1)"},
        {"(x+2)(x+3)", "(x+2)(x+3)"},
        {"x/2+x^2/4", "x(x+2)/4"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) CHECK_STR(F(cases[i][0]), cases[i][1]);
}

/* a perfect square minus a square: brackets are written letters first, like every other answer */
static void perfect_square_minus(void)
{
    static const char *cases[][2] = {
        {"x^2+2xy+y^2-9", "(x+y+3)(x+y-3)"},
        {"x^2-6x+9-y^2", "(x+y-3)(x-y-3)"},
        {"4x^2+4x+1-z^2", "(2x+z+1)(2x-z+1)"},
        {"x^2-y^2+2y-1", "(x+y-1)(x-y+1)"},
        {"2x^2+4xy+2y^2-18", "2(x+y+3)(x+y-3)"},
        {"(x+y+3)(x+y-3)", "(x+y+3)(x+y-3)"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) CHECK_STR(F(cases[i][0]), cases[i][1]);
    F("x^2-6x+9-y^2");
    CHECK(has_title("Spot the perfect square") && has_math("(x-3)^2-y^2"), "psq: (x-3)^2-y^2 line");
    CHECK(has_title("Difference of two squares") && has_math("(x-3+y)(x-3-y)"), "psq: (a+b)(a-b) line");
    CHECK(st.n && strcmp(st.v[st.n - 1].title, "Check by expanding") == 0, "psq: check step is last");
    F("x^2+2xy+y^2-9");
    CHECK(has_math("(x+y)^2-3^2"), "psq: (x+y)^2-3^2 line");
    F("4x^2+4x+1-z^2");
    CHECK(has_math("(2x+1)^2-z^2") && has_math("(2x+1+z)(2x+1-z)"), "psq: 4x^2+4x+1-z^2 working");
    F("x^2-y^2+2y-1");
    CHECK(has_math("x^2-(y-1)^2") && has_math("(x+(y-1))(x-(y-1))"), "psq: square minus a bracket squared");
    const char *ins[] = {"x^2+2xy+y^2-9", "x^2-6x+9-y^2", "4x^2+4x+1-z^2", "x^2-y^2+2y-1"};
    for (int i = 0; i < 4; i++) {
        const char *once = F(ins[i]);
        CHECK_STR(F(once), once);
    }
}

static int title_at(const char *t)
{
    for (int i = 0; i < st.n; i++)
        if (strcmp(st.v[i].title, t) == 0) return i;
    return -1;
}

/* the rewrite comes before the line that factorises it: working reads top to bottom */
static void order(const char *in, const char *rewrite, const char *method, const char *rmath)
{
    F(in);
    int r = title_at(rewrite), m = title_at(method);
    CHECK(r >= 0 && m >= 0 && r < m, "%s: \"%s\" (%d) before \"%s\" (%d)", in, rewrite, r, method, m);
    CHECK(r >= 0 && strcmp(st.v[r].math, rmath) == 0, "%s: rewrite line \"%s\"", in, r >= 0 ? st.v[r].math : "");
}

static void working(void)
{
    order("2x^2y-8y+3x^2-12", "Write it as one square minus another", "Difference of two squares",
          "(x^2-2^2)(2y+3)");
    order("x^4-16", "Write it as one square minus another", "Difference of two squares", "(x^2)^2-4^2");
    order("x^3-8", "Write it with two cubes", "Difference of two cubes", "x^3-2^3");
    order("x^3+27", "Write it with two cubes", "Sum of two cubes", "x^3+3^3");
    order("x^2-6x+9-y^2", "Write down (a+b)(a-b)", "Difference of two squares", "(x-3+y)(x-3-y)");

    F("2x^2y-8y+3x^2-12");
    CHECK(has_title("Group the terms"), "grouping step");
    CHECK(has_math("2y(x^2-4)+3(x^2-4)"), "each group factorised");
    CHECK(has_math("(x^2-4)(2y+3)"), "common bracket taken out");
    CHECK(has_math("(2y+3)(x+2)(x-2)"), "difference of two squares line");
    CHECK(has_title("Check by expanding"), "check step");
    CHECK(st.n && strcmp(st.v[st.n - 1].title, "Check by expanding") == 0, "check step is last");

    F("y(2x+4)");
    CHECK(has_title("Multiply out the brackets"), "flatten first");
    CHECK(has_title("Take out the common factor 2y"), "HCF title");

    F("6x^2+x-2");
    CHECK(has_title("Multiply a and c") && has_math("6×(-2)=-12"), "ac shown");
    CHECK(has_title("Find the two numbers") && has_math("4×(-3)=-12"), "the pair");
    CHECK(has_title("Split the middle term"), "split");
    CHECK(has_title("Factorise each pair"), "grouping the split");

    F("x^3-6x^2+11x-6");
    CHECK(has_title("Use the factor theorem") && has_math("f(1)=0"), "f(a)=0 shown");
    CHECK(has_title("Divide by (x-1)"), "division shown");

    F("x^2+5x+6");
    CHECK(has_title("Find the two numbers") && has_math("2×3=6"), "a=1 pair");
    F("x^2+6x+9");
    CHECK(has_title("Perfect square"), "perfect square title");
    F("x^4-16");
    CHECK(has_title("Difference of two squares"), "two squares title");
    F("x^3-8");
    CHECK(has_title("Difference of two cubes"), "cubes title");
    F("x^3+27");
    CHECK(has_title("Sum of two cubes"), "sum of cubes title");
    F("x^4-5x^2+4");
    CHECK(has_title("Treat it as a quadratic in x^2"), "quadratic in disguise");
}

static void unfactorisable(void)
{
    const char *none[] = {"x^2+1", "x^2+x+1", "2x^2-3", "6x^2y"};
    for (int i = 0; i < 4; i++) {
        const char *got = F(none[i]);
        CHECK_STR(got, none[i]);
    }
    F("x^2+1");
    CHECK(has_title("Can't factorise this any further"), "x^2+1 message");
    CHECK(!has_note_f1("rationals") && has_note_f1("multiply to give 1 and add to give 0"), "x^2+1: plain GCSE reason");
    F("x^2-5");
    CHECK(has_note_f1("multiply to give -5 and add to give 0") && !has_note_f1("p/q"), "x^2-5: plain GCSE reason");
    F("2x^2-3");
    CHECK(has_note_f1("a×c = -6 and add to give 0"), "2x^2-3: plain GCSE reason");
    F("x^2+x+1");
    CHECK(has_title("Can't factorise this any further"), "x^2+x+1 message");
    CHECK(has_note_f1("multiply to give 1 and add to give 1"), "x^2+x+1: plain GCSE reason");
    CHECK(st.n == 1, "only the one step, got %d", st.n);
    CHECK_STR(F("sin(x)"), "ERR:Can only factorise polynomials");
    CHECK_STR(F("1/x"), "ERR:Can only factorise polynomials");
    CHECK_STR(F("x=2"), "ERR:Can only factorise polynomials");
}

static const char *polys[] = {
    "x^2+5x+6", "x^2-x-6", "6x^2+x-2", "2x^2-7x+3", "x^3-6x^2+11x-6", "2x^3+3x^2-8x+3",
    "x^4-16", "50-2x^2", "x^2+6x+9", "9x^2-12x+4", "ax+ay+bx+by", "2x^2y-8y+3x^2-12",
    "x^3-8", "8-x^3", "27x^3+1", "x^6-1", "x^4+4", "x^5+x+1", "x^2+5xy+6y^2",
    "x^4-5x^2+4", "4x^2-9y^2", "6x^2y+9xy^2", "x^2+1", "x^2+x+1", "-x^2+5x-6",
    "3x^2-12", "x^3-x", "x^4-1", "12x^2-5x-2", "x^3+2x^2-x-2", "a^2-b^2", "x^2y^2-1",
    "2x^4-x^3-26x^2-11x+12", "x^3-3x^2+3x-1", "y(2x+4)", "x/2+x^2/4",
    "x^2+2xy+y^2-9", "x^2-6x+9-y^2", "4x^2+4x+1-z^2", "x^2-y^2+2y-1", "9-x^2-2xy-y^2",
};

static void round_trip(void)
{
    int n = (int)(sizeof polys / sizeof polys[0]);
    for (int i = 0; i < n; i++) {
        Expr *e = ex_parse(polys[i], NULL, 0, NULL), *fo = NULL, *eo = NULL;
        Poly P, Q;
        Steps s1, s2;
        steps_init(&s1);
        steps_init(&s2);
        bool ok = e && poly_from_expr(e, &P) && alg_factor(e, &s1, &fo, err, sizeof err) &&
                  alg_expand(fo, &s2, &eo, err, sizeof err) && poly_from_expr(eo, &Q) && poly_equal(&P, &Q);
        CHECK(ok, "expand(factor(%s)) != p (%s)", polys[i], err);
    }
}

static void idempotent(void)
{
    const char *ins[] = {"y(2x+4)", "2x^2y-8y+3x^2-12", "x^4-16", "6x^2+x-2", "x^3-6x^2+11x-6",
                         "50-2x^2", "x^2+6x+9", "-x^2+5x-6", "x^6-1", "x^2+1"};
    for (int i = 0; i < 10; i++) {
        const char *once = F(ins[i]);
        CHECK_STR(F(once), once);
    }
}

int main(void)
{
    exact();
    perfect_square_minus();
    working();
    unfactorisable();
    round_trip();
    idempotent();
    return test_finish();
}
