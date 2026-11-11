/* poly.h: conversion, arithmetic, degrees, coefficients, content, division, univariate helpers. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/poly.h"

static Poly PP(const char *s)
{
    Poly p = poly_zero();
    Expr *e = ex_parse(s, NULL, 0, NULL);
    if (!e || !poly_from_expr(e, &p)) printf("poly_from_expr failed: %s\n", s);
    return p;
}

static bool converts(const char *s)
{
    Poly p;
    Expr *e = ex_parse(s, NULL, 0, NULL);
    return e && poly_from_expr(e, &p);
}

static const char *PS(const Poly *p, const char *mainvar) { return ex_print(ex_tidy(poly_to_expr(p, mainvar))); }

/* expression -> polynomial -> display text */
#define X(s, mv, w) do { Poly p_ = PP(s); CHECK_STR(PS(&p_, mv), w); } while (0)

int main(void)
{
    Poly a, b, c, r;
    Rat k;

    /* conversion and normal form */
    X("2x^2y-8y+3x^2-12", "x", "2x^2y+3x^2-8y-12");
    X("y(2x+4)", "x", "2xy+4y");
    X("(x+2)(x-3)(2x+1)", "x", "2x^3-x^2-13x-6");
    X("(x+y)^2", "x", "x^2+2xy+y^2");
    X("(x+y)^2", "y", "y^2+2xy+x^2");
    X("(a-b)(a+b)", NULL, "a^2-b^2");
    X("(x-1)^3", "x", "x^3-3x^2+3x-1");
    X("x+1-x", "x", "1");
    X("x-x", "x", "0");
    X("3/4+5/6", NULL, "19/12");
    X("2_1/2x", "x", "5/2x");
    X("(x+1)/2", "x", "x/2+1/2");
    X("50%x", "x", "x/2");
    X("recur(0.(3))x", "x", "x/3");
    X("2.5x", "x", "5/2x");
    X("-x", "x", "-x");
    X("x^2y+xy^2+y^3+x^3", "x", "x^3+x^2y+xy^2+y^3");
    X("x^2y+xy^2+y^3+x^3", "y", "y^3+xy^2+x^2y+x^3");
    X("4", NULL, "4");
    X("-(2x-3)", "x", "-2x+3");

    /* things that are not polynomials */
    CHECK(!converts("sin(x)"), "sin(x)");
    CHECK(!converts("x^-1"), "x^-1");
    CHECK(!converts("1/x"), "1/x");
    CHECK(!converts("\xCF\x80x"), "pi x");
    CHECK(!converts("x^0.5"), "x^0.5");
    CHECK(!converts("\xE2\x88\x9A" "x"), "sqrt x");
    CHECK(!converts("x^y"), "x^y");
    CHECK(!converts("x/(2-2)"), "divide by zero");
    CHECK(!converts("x=2"), "relation");
    CHECK(!converts("99999999999^3"), "overflow");
    CHECK(!converts("(99999999999x)^3"), "coefficient overflow");
    CHECK(!converts("abcdfghjk"), "nine variables");
    CHECK(!converts("(a+b+c+d+f+g+h+k)^10"), "too many terms");
    CHECK(converts("abcdfghk"), "eight variables");
    a = PP("x+1");
    CHECK(!poly_pow(&a, POLY_MAXEXP + 1, &r), "exponent cap");
    CHECK(!poly_from_expr(NULL, &r), "NULL expr");

    /* arithmetic */
    a = PP("x^2+2x+1");
    b = PP("x-1");
    CHECK(poly_add(&a, &b, &r), "add ok");
    CHECK_STR(PS(&r, "x"), "x^2+3x");
    CHECK(poly_sub(&a, &b, &r), "sub ok");
    CHECK_STR(PS(&r, "x"), "x^2+x+2");
    CHECK(poly_mul(&a, &b, &r), "mul ok");
    CHECK_STR(PS(&r, "x"), "x^3+x^2-x-1");
    CHECK(poly_pow(&b, 2, &r), "pow ok");
    CHECK_STR(PS(&r, "x"), "x^2-2x+1");
    CHECK(poly_pow(&b, 0, &r) && poly_is_const(&r, &k) && rat_eq(k, rat_int(1)), "p^0 = 1");
    CHECK(poly_scale(&a, rat_make(-1, 2), &r), "scale ok");
    CHECK_STR(PS(&r, "x"), "-x^2/2-x-1/2");
    CHECK(poly_scale(&a, rat_int(0), &r) && poly_is_zero(&r), "scale by 0");
    CHECK(poly_neg(&b, &r), "neg ok");
    CHECK_STR(PS(&r, "x"), "-x+1");
    c = PP("(x+1)^2");
    CHECK(poly_equal(&a, &c), "equal after expansion");
    CHECK(!poly_equal(&a, &b), "not equal");
    c = PP("y+1");
    CHECK(!poly_equal(&b, &c), "different variables");
    CHECK(poly_sub(&a, &a, &r) && poly_is_zero(&r) && r.nv == 0, "a-a is zero, no variables left");
    c = PP("xy");
    CHECK(poly_mul(&a, &c, &r), "mul across variables");
    CHECK_STR(PS(&r, "x"), "x^3y+2x^2y+xy");

    /* inspection */
    a = PP("2x^2y-8y+3x^2-12");
    CHECK(poly_total_degree(&a) == 3, "total degree");
    CHECK(poly_degree(&a, "x") == 2, "degree in x");
    CHECK(poly_degree(&a, "y") == 1, "degree in y");
    CHECK(poly_degree(&a, "z") == 0, "degree in absent var");
    r = poly_zero();
    CHECK(poly_total_degree(&r) == -1 && poly_degree(&r, "x") == -1, "zero degree");
    CHECK(poly_is_const(&r, &k) && rat_is_zero(k), "zero is const 0");
    CHECK(!poly_is_const(&a, NULL), "not const");
    CHECK(poly_coeff(&a, "x", 2, &r), "coeff x^2");
    CHECK_STR(PS(&r, "y"), "2y+3");
    CHECK(poly_coeff(&a, "x", 0, &r), "coeff x^0");
    CHECK_STR(PS(&r, "y"), "-8y-12");
    CHECK(poly_coeff(&a, "x", 1, &r) && poly_is_zero(&r), "coeff x^1 = 0");
    CHECK(poly_coeff(&a, "z", 0, &r) && poly_equal(&r, &a), "coeff of absent var^0 = all");

    /* evaluation: remainder theorem / factor theorem */
    a = PP("2x^3-x^2-13x-6");
    CHECK(poly_eval(&a, "x", rat_int(-2), &r) && poly_is_zero(&r), "f(-2) = 0");
    CHECK(poly_eval(&a, "x", rat_int(3), &r) && poly_is_zero(&r), "f(3) = 0");
    CHECK(poly_eval(&a, "x", rat_make(-1, 2), &r) && poly_is_zero(&r), "f(-1/2) = 0");
    CHECK(poly_eval(&a, "x", rat_int(1), &r) && poly_is_const(&r, &k) && rat_eq(k, rat_int(-18)), "f(1) = -18");
    CHECK(poly_eval(&a, "x", rat_int(0), &r) && poly_is_const(&r, &k) && rat_eq(k, rat_int(-6)), "f(0) = -6");
    b = PP("2x^2y-8y+3x^2-12");
    CHECK(poly_eval(&b, "y", rat_int(1), &r), "partial eval");
    CHECK_STR(PS(&r, "x"), "5x^2-20");
    c = PP("x^60");
    CHECK(!poly_eval(&c, "x", rat_int(3), &r), "eval overflow");

    /* content, monomial factors */
    a = PP("6x^2+4x");
    CHECK(poly_content(&a, &k) && rat_eq(k, rat_int(2)), "content 2");
    a = PP("x/2+1/3");
    CHECK(poly_content(&a, &k) && rat_eq(k, rat_make(1, 6)), "content 1/6");
    a = PP("-4x-6");
    CHECK(poly_content(&a, &k) && rat_eq(k, rat_int(2)), "content positive");
    r = poly_zero();
    CHECK(poly_content(&r, &k) && rat_is_zero(k), "content of 0");
    a = PP("6x^3y^2+4x^2y^3");
    b = poly_monomial_gcd(&a);
    CHECK_STR(PS(&b, "x"), "x^2y^2");
    CHECK(poly_div_monomial(&a, &b, &r), "div monomial");
    CHECK_STR(PS(&r, "x"), "6x+4y");
    b = PP("2x");
    CHECK(poly_div_monomial(&a, &b, &r), "div by 2x");
    CHECK_STR(PS(&r, "x"), "3x^2y^2+2xy^3");
    b = PP("z");
    CHECK(!poly_div_monomial(&a, &b, &r), "z does not divide");
    b = PP("x+1");
    CHECK(!poly_div_monomial(&a, &b, &r), "not a monomial");
    a = PP("x^2+x");
    b = poly_monomial_gcd(&a);
    CHECK_STR(PS(&b, "x"), "x");
    a = PP("x^2+1");
    b = poly_monomial_gcd(&a);
    CHECK(poly_is_const(&b, &k) && rat_is_one(k), "gcd monomial 1");

    /* exact division */
    a = PP("x^2-1");
    b = PP("x-1");
    CHECK(poly_div_exact(&a, &b, &r), "x^2-1 / x-1");
    CHECK_STR(PS(&r, "x"), "x+1");
    a = PP("x^3-y^3");
    b = PP("x-y");
    CHECK(poly_div_exact(&a, &b, &r), "x^3-y^3 / x-y");
    CHECK_STR(PS(&r, "x"), "x^2+xy+y^2");
    a = PP("2x^2y-8y+3x^2-12");
    b = PP("x^2-4");
    CHECK(poly_div_exact(&a, &b, &r), "grouping quotient");
    CHECK_STR(PS(&r, "y"), "2y+3");
    b = PP("2y+3");
    CHECK(poly_div_exact(&a, &b, &r), "other factor");
    CHECK_STR(PS(&r, "x"), "x^2-4");
    a = PP("x^2+1");
    b = PP("x-1");
    CHECK(!poly_div_exact(&a, &b, &r), "not exact");
    b = poly_zero();
    CHECK(!poly_div_exact(&a, &b, &r), "divide by zero poly");
    b = PP("2");
    CHECK(poly_div_exact(&a, &b, &r), "divide by constant");
    CHECK_STR(PS(&r, "x"), "x^2/2+1/2");
    a = PP("(x+y+1)(x-2y)(3x+y)");
    b = PP("(x-2y)");
    CHECK(poly_div_exact(&a, &b, &r), "3-factor product");
    c = PP("(x+y+1)(3x+y)");
    CHECK(poly_equal(&r, &c), "quotient matches");

    /* univariate view */
    {
        Rat cf[8], q[8], rm[8], v;
        int d, dq, dr;
        a = PP("2x^3-x^2-13x-6");
        CHECK(poly_to_upoly(&a, "x", cf, 7, &d) && d == 3, "to upoly");
        CHECK(rat_eq(cf[0], rat_int(-6)) && rat_eq(cf[1], rat_int(-13)) && rat_eq(cf[2], rat_int(-1)) &&
              rat_eq(cf[3], rat_int(2)), "upoly coefficients");
        CHECK(!poly_to_upoly(&a, "x", cf, 2, &d), "maxdeg");
        CHECK(!poly_to_upoly(&a, "y", cf, 7, &d), "wrong var");
        b = PP("xy");
        CHECK(!poly_to_upoly(&b, "x", cf, 7, &d), "multivariate");
        b = PP("5");
        CHECK(poly_to_upoly(&b, "x", cf, 7, &d) && d == 0 && rat_eq(cf[0], rat_int(5)), "constant upoly");
        b = poly_zero();
        CHECK(poly_to_upoly(&b, "x", cf, 7, &d) && d == 0 && rat_is_zero(cf[0]), "zero upoly");
        CHECK(poly_to_upoly(&a, "x", cf, 7, &d), "again");
        c = poly_from_upoly(cf, d, "x");
        CHECK(poly_equal(&c, &a), "from upoly round trip");
        CHECK(upoly_eval(cf, d, rat_int(3), &v) && rat_is_zero(v), "upoly f(3) = 0");
        CHECK(upoly_eval(cf, d, rat_int(2), &v) && rat_eq(v, rat_int(-20)), "upoly f(2) = -20");
        CHECK(upoly_div_linear(cf, d, rat_int(3), q, &v) && rat_is_zero(v), "synthetic division");
        c = poly_from_upoly(q, d - 1, "x");
        CHECK_STR(PS(&c, "x"), "2x^2+5x+2");
        CHECK(upoly_div_linear(cf, d, rat_int(1), q, &v) && rat_eq(v, rat_int(-18)), "remainder theorem");
        Rat n3[4] = {rat_int(1), rat_int(0), rat_int(0), rat_int(1)}, d2[3] = {rat_int(1), rat_int(0), rat_int(1)};
        CHECK(upoly_divmod(n3, 3, d2, 2, q, &dq, rm, &dr), "divmod");
        c = poly_from_upoly(q, dq, "x");
        CHECK_STR(PS(&c, "x"), "x");
        c = poly_from_upoly(rm, dr, "x");
        CHECK_STR(PS(&c, "x"), "-x+1");
        CHECK(upoly_divmod(d2, 2, n3, 3, q, &dq, rm, &dr) && dq == 0 && rat_is_zero(q[0]) && dr == 2, "divmod small/big");
        Rat two[1] = {rat_int(2)};
        CHECK(upoly_divmod(n3, 3, two, 0, q, &dq, rm, &dr) && dq == 3 && rat_eq(q[3], rat_make(1, 2)) && rat_is_zero(rm[0]),
              "divmod by constant");
        Rat zero[1] = {rat_int(0)};
        CHECK(!upoly_divmod(n3, 3, zero, 0, q, &dq, rm, &dr), "divmod by zero");
        CHECK(!upoly_div_linear(two, 0, rat_int(1), q, &v), "div_linear needs degree >= 1");
    }

    /* round trip through Expr */
    {
        const char *src[] = {"2x^2y-8y+3x^2-12", "(x+2)(x-3)(2x+1)", "(a-b)^3", "x/3-2/5", "0", "-7"};
        for (size_t i = 0; i < sizeof src / sizeof *src; i++) {
            a = PP(src[i]);
            CHECK(poly_from_expr(poly_to_expr(&a, "x"), &b) && poly_equal(&a, &b), "round trip %s", src[i]);
            CHECK(poly_from_expr(ex_tidy(poly_to_expr(&a, "x")), &b) && poly_equal(&a, &b), "tidy round trip %s", src[i]);
        }
    }
    return test_finish();
}
