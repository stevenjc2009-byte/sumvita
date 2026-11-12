/* ex_print / ex_print_plain: exact text, and parse(print(parse(s))) == parse(s). */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/expr.h"

/* Print the parse of s; also check the printed text parses back to the same tree. */
static const char *RT(const char *s, int line)
{
    char err[96];
    int pos;
    Expr *a = ex_parse(s, err, sizeof err, &pos);
    if (!a) { printf("FAIL line %d: parse of \"%s\" failed: %s\n", line, s, err); t_fail++; return "(parse error)"; }
    const char *p = ex_print(a);
    Expr *b = ex_parse(p, err, sizeof err, &pos);
    t_total++;
    if (!b || !ex_equal(a, b)) {
        t_fail++;
        printf("FAIL line %d: round trip \"%s\" -> \"%s\" %s\n", line, s, p, b ? "differs" : err);
    }
    return p;
}
#define R(s, want) CHECK_STR(RT(s, __LINE__), want)

/* Engine-made tree: print, and check the text re-parses (value checked by eval). */
static const char *PE(const Expr *e) { return ex_print(e); }

static Expr *V(const char *n) { return ex_var(n); }
static Expr *N(long long n) { return ex_int(n); }
static Expr *Q(long long n, long long d) { return ex_num(rat_make(n, d)); }
static Expr *M3(Expr *a, Expr *b, Expr *c) { Expr *k[3] = {a, b, c}; return ex_node(E_MUL, 3, k); }
static Expr *A3(Expr *a, Expr *b, Expr *c) { Expr *k[3] = {a, b, c}; return ex_node(E_ADD, 3, k); }

int main(void)
{
    /* round trips of typed input */
    R("2x^2y-8y+3x^2-12", "2x^2y-8y+3x^2-12");
    R("y(2x+4)", "y(2x+4)");
    R("(x+2)(x-3)(2x+1)", "(x+2)(x-3)(2x+1)");
    R("3/4+5/6", "3/4+5/6");
    R("2_1/3\xC3\x97" "1_1/2", "2_1/3\xC3\x97" "1_1/2");
    R("\xE2\x88\x9A" "50", "\xE2\x88\x9A" "50");
    R("\xE2\x88\x9A(50)", "\xE2\x88\x9A" "50");
    R("sqrt(x+1)", "\xE2\x88\x9A(x+1)");
    R("\xE2\x88\x9A" "2x", "\xE2\x88\x9A" "2x");
    R("\xE2\x88\x9A(2x)", "\xE2\x88\x9A(2x)");
    R("x^-2", "x^-2");
    R("x^-y", "x^-y");
    R("x^(-2x)", "x^(-2x)");
    R("5!", "5!");
    R("3!!", "3!!");
    R("20%", "20%");
    R("sin 30\xC2\xB0", "sin(30\xC2\xB0)");
    R("sin 2x", "sin(2x)");
    R("sin x cos x", "sin(x)cos(x)");
    R("-x^2", "-x^2");
    R("-2^2", "-2^2");
    R("(-2)^2", "(-2)^2");
    R("-3x", "-3x");
    R("--3", "--3");
    R("-(3)", "-(3)");
    R("2^3^2", "2^3^2");
    R("(2^3)^2", "(2^3)^2");
    R("2/3x", "(2/3)x");     /* F1: a fraction before a letter is bracketed, never 2/3x */
    R("1/(2x)", "1/(2x)");
    R("a/b/c", "a/b/c");
    R("a/(b/c)", "a/(b/c)");
    R("2*3", "2\xC3\x97" "3");
    R("2\xC3\x97" "3\xC3\x97" "4", "2\xC3\x97" "3\xC3\x97" "4");
    R("3.2\xC3\x97" "10^5", "3.2\xC3\x97" "10^5");
    R("x2", "x\xC3\x97" "2");
    R("2*-3", "2(-3)");
    R("2(-3)", "2(-3)");
    R("x-(a+b)", "x-(a+b)");
    R("a-b-c", "a-b-c");
    R("a+(b+c)", "a+(b+c)");
    R("0.5x+0.25", "0.5x+0.25");
    R(".5", "0.5");
    R("recur(0.1(27))", "recur(0.1(27))");
    R("x=2 \"or\" x=-3", "x=2 \"or\" x=-3");
    R("\"HCF\"=6", "\"HCF\"=6");
    R("\"HCF\"=2×" "3=6, \"LCM\"=36", "\"HCF\"=2×" "3=6, \"LCM\"=36");
    R("\"Area\"=12 \"cm\", \"Perimeter\"=14 \"cm\"", "\"Area\"=12 \"cm\", \"Perimeter\"=14 \"cm\"");
    R("1 \"metre\"=100 \"cm\"", "1 \"metre\"=100 \"cm\"");
    R("\"so\" x=2", "\"so\" x=2");
    R("x+y=3,x-y=1", "x+y=3, x-y=1");
    R("(2,-3)", "(2, -3)");
    R("-3<2x+1<=9", "-3<2x+1\xE2\x89\xA4" "9");
    R("x>=2", "x\xE2\x89\xA5" "2");
    R("x!=2", "x\xE2\x89\xA0" "2");
    R("5! =120", "5! =120");
    R("x=(3\xC2\xB1\xE2\x88\x9A" "5)/2", "x=(3\xC2\xB1\xE2\x88\x9A" "5)/2");
    R("-b\xC2\xB1\xE2\x88\x9A(b^2-4ac)", "-b\xC2\xB1\xE2\x88\x9A(b^2-4ac)");
    R("\xC2\xB1" "3", "\xC2\xB1" "3");
    R("|x-1|", "|x-1|");
    R("|x||y|", "|x||y|");
    R("||x|-1|", "abs(|x|-1)");
    R("abs(2|x|)", "abs(2|x|)");
    R("log(2,8)", "log(2,8)");
    R("cbrt(8)", "root(3,8)");
    R("nCr(5,2)", "nCr(5,2)");
    R("sin^-1(0.5)", "asin(0.5)");
    R("2\xCF\x80r", "2\xCF\x80r");
    R("pi", "\xCF\x80");
    R("e^x", "e^x");
    R("Ans\xC3\x97" "2", "Ans\xC3\x97" "2");
    R("2Ans", "2Ans");
    R("\xCE\xB8+1", "\xCE\xB8+1");
    R("x sin(x)", "xsin(x)");
    R("a sin(x)", "a sin(x)");
    R("p i", "p i");
    R("s i n", "si n");
    R("e x p", "ex p");
    R("(x+1)^2", "(x+1)^2");
    R("x^2y", "x^2y");
    R("x^(1/2)", "x^(1/2)");
    R("(1/2)^2", "(1/2)^2");
    R("\xE2\x88\x9A" "x^2", "\xE2\x88\x9A" "x^2");
    R("\xE2\x88\x9A(x^2)", "\xE2\x88\x9A(x^2)");
    R("\xE2\x88\x9A(5!)", "\xE2\x88\x9A(5!)");
    R("\xE2\x88\x9A" "5!", "\xE2\x88\x9A" "5!");
    R("2_1/3x", "2_1/3x");
    R("(2_1/3)^2", "(2_1/3)^2");
    R("((x))", "(x)");
    R("[x+1]", "(x+1)");
    R("x(1/2)", "x(1/2)");
    R("-x/2", "-x/2");
    R("a(b(c+d))", "a(b(c+d))");
    R("x^2\xC3\x97" "3", "x^2\xC3\x97" "3");
    R("-(x-1)", "-(x-1)");
    R("x--1", "x--1");
    R("x^-(a+b)", "x^-(a+b)");

    /* engine-made trees */
    CHECK_STR(PE(ex_add2(V("x"), N(-3))), "x-3");
    CHECK_STR(PE(ex_sub2(V("x"), N(3))), "x-3");
    CHECK_STR(PE(A3(ex_pow2(V("x"), N(2)), ex_mul2(N(-4), V("x")), N(3))), "x^2-4x+3");
    CHECK_STR(PE(ex_mul2(Q(1, 2), V("x"))), "x/2");
    CHECK_STR(PE(ex_mul2(Q(-1, 2), V("x"))), "-x/2");
    CHECK_STR(PE(M3(Q(1, 2), V("x"), V("y"))), "xy/2");
    CHECK_STR(PE(ex_mul2(Q(2, 3), V("x"))), "2x/3");
    CHECK_STR(PE(ex_mul2(Q(-2, 3), V("x"))), "-2x/3");
    /* F1 items 7, 11: a rational coefficient goes over the whole product, and the text parses back */
    {
        Expr *pi = ex_const(C_PI);
        CHECK_STR(PE(ex_mul2(Q(5, 6), V("x"))), "5x/6");
        CHECK_STR(PE(ex_mul2(Q(4, 3), pi)), "4π/3");
        CHECK_STR(PE(M3(Q(4, 3), ex_const(C_PI), N(27))), "4π×" "27/3");
        CHECK_STR(PE(ex_mul2(Q(2, 3), ex_add2(V("x"), N(1)))), "2(x+1)/3");
        CHECK_STR(PE(A3(ex_pow2(V("x"), N(2)), ex_mul2(Q(5, 6), V("x")), Q(1, 6))), "x^2+5x/6+1/6");
        CHECK_STR(PE(A3(ex_pow2(V("x"), N(2)), ex_mul2(Q(-5, 6), V("x")), Q(1, 6))), "x^2-5x/6+1/6");
        const Expr *cases[] = {
            ex_mul2(Q(5, 6), V("x")), ex_mul2(Q(-2, 3), V("x")), ex_mul2(Q(4, 3), ex_const(C_PI)),
            M3(Q(4, 3), ex_const(C_PI), N(27)), ex_mul2(Q(2, 3), ex_add2(V("x"), N(1))),
            M3(Q(3, 4), V("x"), ex_pow2(V("y"), N(2))),
            A3(ex_pow2(V("x"), N(2)), ex_mul2(Q(5, 6), V("x")), Q(1, 6)),
        };
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            char err[96];
            int pos;
            const char *txt = ex_print(cases[i]);
            Expr *back = ex_parse(txt, err, sizeof err, &pos);
            CHECK(back != NULL, "F1 print round trip: \"%s\" does not parse: %s", txt, err);
            if (!back) continue;
            Expr *sa = ex_simplify(ex_subst(ex_subst(cases[i], "x", N(7)), "y", N(5)));
            Expr *sb = ex_simplify(ex_subst(ex_subst(back, "x", N(7)), "y", N(5)));
            bool ok = ex_equal(sa, sb);
            CHECK(ok, "F1 print round trip: \"%s\" parses to a different value", txt);
        }
    }
    CHECK_STR(PE(ex_mul2(Q(1, 2), ex_add2(V("x"), N(1)))), "(x+1)/2");
    CHECK_STR(PE(ex_add2(V("a"), ex_mul2(Q(-1, 2), V("x")))), "a-x/2");
    CHECK_STR(PE(Q(2, 3)), "2/3");
    CHECK_STR(PE(ex_pow2(Q(2, 3), N(2))), "(2/3)^2");
    CHECK_STR(PE(ex_pow2(V("x"), Q(1, 2))), "x^(1/2)");
    CHECK_STR(PE(ex_mul2(V("x"), Q(2, 3))), "x(2/3)");
    CHECK_STR(PE(ex_pow2(N(-2), N(2))), "(-2)^2");
    CHECK_STR(PE(ex_neg(ex_add2(V("a"), V("b")))), "-(a+b)");
    CHECK_STR(PE(ex_neg(ex_mul2(N(2), V("x")))), "-2x");
    CHECK_STR(PE(ex_mul2(V("x"), ex_neg(V("y")))), "x(-y)");
    CHECK_STR(PE(ex_pow2(V("x"), ex_neg(ex_mul2(N(2), V("x"))))), "x^(-2x)");
    CHECK_STR(PE(ex_mul2(N(3), ex_func1(F_SQRT, N(2)))), "3\xE2\x88\x9A" "2");
    CHECK_STR(PE(ex_mul2(ex_func1(F_SQRT, N(2)), N(3))), "\xE2\x88\x9A" "2\xC3\x97" "3");
    CHECK_STR(PE(ex_func1(F_SQRT, Q(1, 2))), "\xE2\x88\x9A(1/2)");
    CHECK_STR(PE(ex_func1(F_SQRT, N(-4))), "\xE2\x88\x9A(-4)");
    CHECK_STR(PE(ex_div2(N(1), ex_pow2(V("x"), N(2)))), "1/x^2");
    CHECK_STR(PE(ex_div2(N(3), ex_mul2(N(4), V("x")))), "3/(4x)");
    CHECK_STR(PE(ex_mul2(ex_mul2(N(2), V("x")), V("y"))), "2xy");
    CHECK_STR(PE(ex_mul2(V("x"), ex_mul2(N(2), V("y")))), "x\xC3\x97" "2y");
    CHECK_STR(PE(ex_mul2(V("p"), V("i"))), "p i");
    CHECK_STR(PE(M3(V("a"), V("b"), V("s"))), "ab s");
    CHECK_STR(PE(ex_add2(ex_add2(V("a"), V("b")), V("c"))), "a+b+c");
    CHECK_STR(PE(ex_add2(V("c"), ex_node(E_PM, 2, (Expr *[]){V("a"), V("b")}))), "c+(a\xC2\xB1" "b)");
    CHECK_STR(PE(ex_rel(R_EQ, ex_node(E_FACT, 1, (Expr *[]){N(5)}), N(120))), "5! =120");
    CHECK_STR(PE(ex_real(2.5)), "2.5");
    CHECK_STR(PE(ex_real(-1e20)), "-1\xC3\x97" "10^20");
    CHECK_STR(PE(ex_real(2.5e-12)), "2.5\xC3\x97" "10^-12");
    CHECK_STR(PE(ex_real(1.0 / 3.0)), "0.3333333333");
    CHECK_STR(PE(ex_pow2(V("x"), ex_real(1e20))), "x^(1\xC3\x97" "10^20)");
    CHECK_STR(PE(ex_mul2(ex_real(1.5e20), V("x"))), "1.5\xC3\x97" "10^20x");
    CHECK_STR(PE(ex_text("or")), "\"or\"");
    { Expr *r = ex_num(rat_make(13, 4)); r->flags |= NF_DECIMAL; CHECK_STR(PE(r), "3.25"); }
    { Expr *r = ex_num(rat_make(1, 3)); r->flags |= NF_DECIMAL; CHECK_STR(PE(r), "1/3"); }
    { Expr *r = amalloc(sizeof(Expr)); r->kind = E_RECUR; r->r = rat_make(1, 7); CHECK_STR(PE(r), "recur(0.(142857))"); }
    { Expr *k[3] = {N(1), N(2), N(3)}; Expr *l = ex_node(E_LIST, 3, k); CHECK_STR(PE(ex_func1(F_ABS, l)), "|(1, 2, 3)|"); }

    /* every engine-made print above must re-parse */
    {
        Expr *made[] = {
            ex_mul2(Q(1, 2), V("x")), ex_mul2(Q(-2, 3), V("x")), ex_neg(ex_mul2(N(2), V("x"))),
            ex_pow2(V("x"), ex_neg(ex_mul2(N(2), V("x")))), ex_div2(N(3), ex_mul2(N(4), V("x"))),
            ex_mul2(V("p"), V("i")), ex_mul2(ex_real(1.5e20), V("x")),
        };
        for (unsigned i = 0; i < sizeof made / sizeof made[0]; i++) {
            const char *s = ex_print(made[i]);
            CHECK(ex_parse(s, NULL, 0, NULL) != NULL, "engine print %u re-parses: %s", i, s);
        }
    }

    /* plain ASCII-ish form */
    {
        Expr *e = ex_parse("2\xCF\x80r^2+\xE2\x88\x9A" "2\xC3\x97" "3", NULL, 0, NULL);
        CHECK_STR(ex_print_plain(e), "2pir^2+sqrt(2)*3");
        e = ex_parse("x\xE2\x89\xA4" "3\xC2\xB1\xCE\xB8", NULL, 0, NULL);
        CHECK_STR(ex_print_plain(e), "x<=3+/-theta");
        e = ex_parse("|x|+30\xC2\xB0", NULL, 0, NULL);
        CHECK_STR(ex_print_plain(e), "abs(x)+30deg");
    }
    CHECK_STR(ex_print(NULL), "");
    return test_finish();
}
