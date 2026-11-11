/* alg_expand: exact answers in descending powers, the GCSE working for single, double,
 * triple brackets and powers, sums of products, idempotence and errors. */
#include "testlib.h"
#include "engine/algebra.h"
#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/poly.h"

static Steps st;
static char err[128];

static const char *E(const char *in)
{
    Expr *e = ex_parse(in, NULL, 0, NULL), *out = NULL;
    if (!e) return "PARSE";
    steps_init(&st);
    err[0] = 0;
    return alg_expand(e, &st, &out, err, sizeof err) ? ex_print(out) : asprintf_a("ERR:%s", err);
}

static bool has_title(const char *t)
{
    for (int i = 0; i < st.n; i++)
        if (strncmp(st.v[i].title, t, strlen(t)) == 0) return true;
    return false;
}

static bool has_math(const char *m)
{
    for (int i = 0; i < st.n; i++)
        if (st.v[i].math && strcmp(st.v[i].math, m) == 0) return true;
    return false;
}

static void exact(void)
{
    static const char *cases[][2] = {
        {"(x+2)(x-3)", "x^2-x-6"},
        {"(x+1)(x+2)(x+3)", "x^3+6x^2+11x+6"},
        {"(2x-1)(x+3)(x-4)(x+1)", "2x^4-x^3-26x^2-11x+12"},
        {"(x+2)^2", "x^2+4x+4"},
        {"(2x-3)^3", "8x^3-36x^2+54x-27"},
        {"3x(x-4)", "3x^2-12x"},
        {"-2(x-5)", "-2x+10"},
        {"2(x+3)-3(x-1)", "-x+9"},
        {"(x+y)(x-y+2)", "x^2+2x-y^2+2y"},
        {"(a+b)(c+d)", "ac+ad+bc+bd"},
        {"(x+1)^5", "x^5+5x^4+10x^3+10x^2+5x+1"},
        {"(x-1)^4", "x^4-4x^3+6x^2-4x+1"},
        {"x(x+1)(x-1)", "x^3-x"},
        {"(x+2)(x-2)", "x^2-4"},
        {"5-(x+2)", "-x+3"},
        {"x^2+3x", "x^2+3x"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) CHECK_STR(E(cases[i][0]), cases[i][1]);
}

static void working(void)
{
    E("(x+2)(x-3)");
    CHECK(has_title("Multiply out the brackets") && has_math("x^2-3x+2x-6"), "four products shown");
    CHECK(has_title("Collect like terms") && has_math("x^2-x-6"), "collected");

    E("(x+1)(x+2)(x+3)");
    CHECK(has_title("Expand the first two brackets") && has_math("x^2+2x+x+2"), "stage 1");
    CHECK(has_math("(x^2+3x+2)(x+3)"), "stage 1 collected, next bracket kept");
    CHECK(has_title("Multiply by (x+3)"), "stage 2 title");

    E("(x+2)^2");
    CHECK(has_title("Write the power as repeated brackets") && has_math("(x+2)(x+2)"), "square as repeated");
    E("(2x-3)^3");
    CHECK(has_math("(2x-3)(2x-3)(2x-3)"), "cube as repeated");
    E("(x+1)^5");
    CHECK(has_title("Use the binomial expansion"), "binomial for n > 4");
    CHECK(st.n == 1, "binomial: one step, got %d", st.n);

    E("3x(x-4)");
    CHECK(has_title("Multiply each term in the bracket by 3x"), "single bracket title");
    E("2(x+3)-3(x-1)");
    CHECK(has_title("Multiply out each bracket") && has_math("2x+6-3x+3"), "term by term, signs kept");
    CHECK(has_math("-x+9"), "collected");
    E("x^2+3x");
    CHECK(has_title("Nothing to expand"), "no brackets");
}

static void errors(void)
{
    CHECK_STR(E("sin(x)"), "ERR:Can only expand polynomials");
    CHECK_STR(E("1/x"), "ERR:Can only expand polynomials");
    CHECK_STR(E("x=2"), "ERR:Can only expand polynomials");
}

static void agree_and_idempotent(void)
{
    const char *ins[] = {"(x+2)(x-3)", "(2x-1)(x+3)(x-4)(x+1)", "(2x-3)^3", "2(x+3)-3(x-1)",
                         "(x+y)(x-y+2)", "(a+b)(c+d)", "(x+1)^5", "(x/2+1)(x-2)", "-(x+1)^2", "(3-x)(x+4)"};
    for (int i = 0; i < 10; i++) {
        Expr *e = ex_parse(ins[i], NULL, 0, NULL);
        Poly P, Q;
        const char *once = E(ins[i]);
        Expr *o = ex_parse(once, NULL, 0, NULL);
        CHECK(e && o && poly_from_expr(e, &P) && poly_from_expr(o, &Q) && poly_equal(&P, &Q), "%s: wrong value %s", ins[i], once);
        CHECK_STR(E(once), once);
    }
}

int main(void)
{
    exact();
    working();
    errors();
    agree_and_idempotent();
    return test_finish();
}
