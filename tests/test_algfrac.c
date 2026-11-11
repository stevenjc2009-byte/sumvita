/* alg_simplify_fraction: factorise top and bottom, cancel, and add fractions over the
 * lowest common denominator; exact strings, working steps, value checks and errors. */
#include "testlib.h"
#include "engine/algebra.h"
#include "engine/arena.h"
#include "engine/expr.h"

static Steps st;
static char err[128];

static const char *S(const char *in)
{
    Expr *e = ex_parse(in, NULL, 0, NULL), *out = NULL;
    if (!e) return "PARSE";
    steps_init(&st);
    err[0] = 0;
    return alg_simplify_fraction(e, &st, &out, err, sizeof err) ? ex_print(out) : asprintf_a("ERR:%s", err);
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
        {"(x^2-4)/(x+2)", "x-2"},
        {"(x^2+5x+6)/(x^2-9)", "(x+2)/(x-3)"},
        {"(2x+4)/(x^2+2x)", "2/x"},
        {"1/(x+1)+2/(x-1)", "(3x+1)/((x+1)(x-1))"},
        {"1/(x+1)-2/(x-1)", "(-x-3)/((x+1)(x-1))"},
        {"(3-x)/(x-3)", "-1"},
        {"(6x^2)/(9x)", "2x/3"},
        {"x+1/x", "(x^2+1)/x"},
        {"1/x+1/x^2", "(x+1)/x^2"},
        {"(x^2-1)/(x^2+2x+1)", "(x-1)/(x+1)"},
        {"(x+1)/(x+2)", "(x+1)/(x+2)"},
        {"x/(x+1)-1/(x+1)", "(x-1)/(x+1)"},
        {"2/(x+1)+2x/(x+1)", "2"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) CHECK_STR(S(cases[i][0]), cases[i][1]);
}

static void working(void)
{
    S("(x^2+5x+6)/(x^2-9)");
    CHECK(has_title("Factorise the top") && has_math("(x+2)(x+3)"), "top");
    CHECK(has_title("Factorise the bottom") && has_math("(x+3)(x-3)"), "bottom");
    CHECK(has_title("Cancel the common factor (x+3)") && has_math("(x+2)(x+3)/((x+3)(x-3))=(x+2)/(x-3)"), "cancel");

    S("(2x+4)/(x^2+2x)");
    CHECK(has_math("2(x+2)") && has_math("x(x+2)"), "both factorised");
    CHECK(has_title("Cancel the common factor (x+2)"), "cancel title");

    S("(6x^2)/(9x)");
    CHECK(has_title("Cancel the common factors 3 and x"), "numbers and letters cancel");

    S("1/(x+1)+2/(x-1)");
    CHECK(has_title("Use a common denominator") && has_math("(x-1)/((x+1)(x-1))+2(x+1)/((x+1)(x-1))"), "lcd");
    CHECK(has_title("Add the numerators") && has_math("(x-1+2(x+1))/((x+1)(x-1))"), "add tops");
    CHECK(has_title("Simplify the numerator") && has_math("(3x+1)/((x+1)(x-1))"), "simplified top");

    CHECK_STR(S("1/(x^2-1)+1/(x+1)"), "x/((x+1)(x-1))");
    CHECK(has_title("Factorise the denominators") && has_math("1/((x+1)(x-1))+1/(x+1)"), "denominators factorised first");
    CHECK(has_title("Use a common denominator") && has_math("1/((x+1)(x-1))+(x-1)/((x+1)(x-1))"), "lowest, not product");

    S("(x+1)/(x+2)");
    CHECK(has_title("Nothing cancels"), "nothing cancels");
}

static void errors(void)
{
    CHECK_STR(S("sin(x)/x"), "ERR:Can only simplify algebraic fractions");
    CHECK_STR(S("x/0"), "ERR:Can't divide by zero");
    CHECK_STR(S("x=1"), "ERR:Can only simplify algebraic fractions");
}

int main(void)
{
    exact();
    working();
    errors();
    return test_finish();
}
