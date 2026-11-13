/* ex_parse: structure (as an S-expression dump), precedence, errors. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/expr.h"

static const char *KN[] = {"NUM", "REAL", "VAR", "CONST", "ADD", "MUL", "DIV", "NEG", "POW", "FUNC",
                           "REL", "LIST", "TEXT", "PM", "PCT", "FACT", "DEG", "MIXED", "RECUR"};

static void dump(const Expr *e, char *b, int cap)
{
    char t[64];
    int n = (int)strlen(b);
    switch (e->kind) {
    case E_NUM:
        if (e->r.d == 1) snprintf(t, sizeof t, "%lld%s", (long long)e->r.n, (e->flags & NF_DECIMAL) ? "d" : "");
        else snprintf(t, sizeof t, "%lld/%lld%s", (long long)e->r.n, (long long)e->r.d, (e->flags & NF_DECIMAL) ? "d" : "");
        snprintf(b + n, (size_t)(cap - n), "%s", t);
        return;
    case E_REAL: snprintf(b + n, (size_t)(cap - n), "R%g", e->v); return;
    case E_VAR: snprintf(b + n, (size_t)(cap - n), "%s", e->name); return;
    case E_CONST: snprintf(b + n, (size_t)(cap - n), "%s", e->op == 1 ? "pi" : "e"); return;
    case E_TEXT: snprintf(b + n, (size_t)(cap - n), "'%s'", e->text); return;
    case E_RECUR: snprintf(b + n, (size_t)(cap - n), "[%s=%lld/%lld]", e->text, (long long)e->r.n, (long long)e->r.d); return;
    default: break;
    }
    if (e->kind == E_FUNC) snprintf(t, sizeof t, "(F%d", e->op);
    else if (e->kind == E_REL) {
        snprintf(t, sizeof t, "(REL");
        for (int i = 0; i + 1 < e->nkids; i++) snprintf(t + strlen(t), sizeof t - strlen(t), "%d", e->ops[i]);
    } else snprintf(t, sizeof t, "(%s", KN[e->kind]);
    snprintf(b + n, (size_t)(cap - n), "%s", t);
    for (int i = 0; i < e->nkids; i++) {
        n = (int)strlen(b);
        snprintf(b + n, (size_t)(cap - n), " ");
        dump(e->kids[i], b, cap);
    }
    n = (int)strlen(b);
    snprintf(b + n, (size_t)(cap - n), ")");
}

static char out[1024];

static const char *D(const char *s)
{
    char err[96];
    int pos;
    Expr *e = ex_parse(s, err, sizeof err, &pos);
    if (!e) { snprintf(out, sizeof out, "ERR %s @%d", err, pos); return out; }
    out[0] = 0;
    dump(e, out, sizeof out);
    return out;
}

int main(void)
{
    /* numbers */
    CHECK_STR(D("12"), "12");
    CHECK_STR(D("3.25"), "13/4d");
    CHECK_STR(D(".5"), "1/2d");
    CHECK_STR(D("3."), "3d");
    /* quoted text on either side of a relation: GCSE tool lines */
    CHECK_STR(D("\"HCF\"=6"), "(REL1 'HCF' 6)");
    CHECK_STR(D("x=\"none\""), "(REL1 x 'none')");
    CHECK_STR(D("\"HCF\"=2*3=6"), "(REL11 'HCF' (MUL 2 3) 6)");
    CHECK_STR(D("\"HCF\"=6, \"LCM\"=36"), "(LIST (REL1 'HCF' 6) (REL1 'LCM' 36))");
    CHECK_STR(D("\"Area\"=12 \"cm\", \"Perimeter\"=14 \"cm\""), "(LIST (REL1 'Area' 12) 'cm' (REL1 'Perimeter' 14) 'cm')");
    CHECK_STR(D("x=2 \"or\" x=3"), "(LIST (REL1 x 2) 'or' (REL1 x 3))");
    CHECK_STR(D("\"so\" x=3"), "(LIST 'so' (REL1 x 3))");
    /* bare recurring decimal: a decimal with a dot directly followed by (digits) */
    CHECK_STR(D("0.3(18)"), "[0.3(18)=7/22]");
    CHECK_STR(D(".3(18)"), "[0.3(18)=7/22]");
    CHECK_STR(D("2.(18)+1"), "(ADD [2.(18)=24/11] 1)");
    CHECK_STR(D("2(18)"), "(MUL 2 18)");
    CHECK_STR(D("0.3*(18)"), "(MUL 3/10d 18)");
    CHECK_STR(D("0.3(x)"), "(MUL 3/10d x)");
    CHECK_STR(D("0.3(1+8)"), "(MUL 3/10d (ADD 1 8))");
    CHECK_STR(D("0.3 (18)"), "(MUL 3/10d 18)");
    CHECK_STR(D("2_1/3"), "(MIXED 2 1 3)");
    CHECK_STR(D("recur(0.1(27))"), "[0.1(27)=7/55]");
    CHECK_STR(D("recur(.(3))"), "[0.(3)=1/3]");
    CHECK_STR(D("recur(1.(142857))"), "[1.(142857)=8/7]");
    CHECK_STR(D("123456789012345678901234"), "R1.23457e+23");
    CHECK_STR(D("99999999999^3"), "(POW 99999999999 3)");
    /* variables, constants */
    CHECK_STR(D("xy"), "(MUL x y)");
    CHECK_STR(D("2ab"), "(MUL 2 a b)");
    CHECK_STR(D("e"), "e");
    CHECK_STR(D("2e^x"), "(MUL 2 (POW e x))");
    CHECK_STR(D("pi"), "pi");
    CHECK_STR(D("2\xCF\x80r"), "(MUL 2 pi r)");
    CHECK_STR(D("\xCE\xB8"), "\xCE\xB8");
    CHECK_STR(D("Ans+1"), "(ADD Ans 1)");
    CHECK_STR(D("pix"), "(MUL pi x)");
    /* precedence */
    CHECK_STR(D("1+2*3"), "(ADD 1 (MUL 2 3))");
    CHECK_STR(D("2^3^2"), "(POW 2 (POW 3 2))");
    CHECK_STR(D("-x^2"), "(NEG (POW x 2))");
    CHECK_STR(D("-2^2"), "(NEG (POW 2 2))");
    CHECK_STR(D("-3"), "-3");
    CHECK_STR(D("-3x"), "(MUL -3 x)");
    CHECK_STR(D("--3"), "(NEG -3)");
    CHECK_STR(D("-(3)"), "(NEG 3)");
    CHECK_STR(D("x^-2"), "(POW x -2)");
    CHECK_STR(D("x^-y"), "(POW x (NEG y))");
    CHECK_STR(D("2/3x"), "(MUL (DIV 2 3) x)");
    CHECK_STR(D("1/2x"), "(MUL (DIV 1 2) x)");
    CHECK_STR(D("1/(2x)"), "(DIV 1 (MUL 2 x))");
    CHECK_STR(D("a/b/c"), "(DIV (DIV a b) c)");
    CHECK_STR(D("ab/c"), "(DIV (MUL a b) c)");
    CHECK_STR(D("x-3"), "(ADD x (NEG 3))");
    CHECK_STR(D("a-b+c"), "(ADD a (NEG b) c)");
    CHECK_STR(D("2\xC3\x97" "3\xC3\xB7" "4"), "(DIV (MUL 2 3) 4)");
    CHECK_STR(D("2*-3"), "(MUL 2 -3)");
    CHECK_STR(D("3\xE2\x88\x92" "1"), "(ADD 3 (NEG 1))");
    /* implicit multiplication */
    CHECK_STR(D("3(x+1)"), "(MUL 3 (ADD x 1))");
    CHECK_STR(D("(x+1)(x-2)"), "(MUL (ADD x 1) (ADD x (NEG 2)))");
    CHECK_STR(D("y(2x+4)"), "(MUL y (ADD (MUL 2 x) 4))");
    CHECK_STR(D("(x+2)(x-3)(2x+1)"), "(MUL (ADD x 2) (ADD x (NEG 3)) (ADD (MUL 2 x) 1))");
    CHECK_STR(D("2x^2y-8y+3x^2-12"), "(ADD (MUL 2 (POW x 2) y) (NEG (MUL 8 y)) (MUL 3 (POW x 2)) (NEG 12))");
    CHECK_STR(D("x sin(x)"), "(MUL x (F1 x))");
    CHECK_STR(D("x2"), "(MUL x 2)");
    /* functions */
    CHECK_STR(D("sin 30"), "(F1 30)");
    CHECK_STR(D("sin 2x"), "(F1 (MUL 2 x))");
    CHECK_STR(D("sin x cos x"), "(MUL (F1 x) (F2 x))");
    CHECK_STR(D("sin(x)^2"), "(POW (F1 x) 2)");
    CHECK_STR(D("sin 30\xC2\xB0"), "(F1 (DEG 30))");
    CHECK_STR(D("sin^-1(0.5)"), "(F4 1/2d)");
    CHECK_STR(D("cos\xE2\x81\xBB\xC2\xB9(1)"), "(F5 1)");
    CHECK_STR(D("asin(1)"), "(F4 1)");
    CHECK_STR(D("sinh(1)"), "(F7 1)");
    CHECK_STR(D("log(2,8)"), "(F11 2 8)");
    CHECK_STR(D("nCr(5,2)"), "(F18 5 2)");
    CHECK_STR(D("cbrt(8)"), "(F13 3 8)");
    CHECK_STR(D("root(4,16)"), "(F13 4 16)");
    CHECK_STR(D("\xE2\x88\x9A" "2x"), "(MUL (F12 2) x)");
    CHECK_STR(D("\xE2\x88\x9A(2x)"), "(F12 (MUL 2 x))");
    CHECK_STR(D("\xE2\x88\x9A" "50"), "(F12 50)");
    CHECK_STR(D("3\xE2\x88\x9A" "2"), "(MUL 3 (F12 2))");
    CHECK_STR(D("sqrt(9)"), "(F12 9)");
    CHECK_STR(D("|x-1|"), "(F14 (ADD x (NEG 1)))");
    CHECK_STR(D("|x||y|"), "(MUL (F14 x) (F14 y))");
    CHECK_STR(D("||x|-1|"), "(F14 (ADD (F14 x) (NEG 1)))");
    CHECK_STR(D("2|x|"), "(MUL 2 (F14 x))");
    /* postfix */
    CHECK_STR(D("5!"), "(FACT 5)");
    CHECK_STR(D("3!!"), "(FACT (FACT 3))");
    CHECK_STR(D("20%"), "(PCT 20)");
    CHECK_STR(D("2^3!"), "(POW 2 (FACT 3))");
    CHECK_STR(D("-3!"), "(NEG (FACT 3))");
    /* mixed, ± */
    CHECK_STR(D("2_1/3\xC3\x97" "1_1/2"), "(MUL (MIXED 2 1 3) (MIXED 1 1 2))");
    CHECK_STR(D("3/4+5/6"), "(ADD (DIV 3 4) (DIV 5 6))");
    CHECK_STR(D("x=(3\xC2\xB1\xE2\x88\x9A" "5)/2"), "(REL1 x (DIV (PM 3 (F12 5)) 2))");
    CHECK_STR(D("\xC2\xB1" "3"), "(PM 3)");
    CHECK_STR(D("1+2\xC2\xB1" "3"), "(PM (ADD 1 2) 3)");
    /* relations, lists, text */
    CHECK_STR(D("2x+1=7"), "(REL1 (ADD (MUL 2 x) 1) 7)");
    CHECK_STR(D("-3<2x+1<=9"), "(REL23 -3 (ADD (MUL 2 x) 1) 9)");
    CHECK_STR(D("x\xE2\x89\xA5" "2"), "(REL5 x 2)");
    CHECK_STR(D("x\xE2\x89\xA0" "2"), "(REL6 x 2)");
    CHECK_STR(D("x!=2"), "(REL6 x 2)");
    CHECK_STR(D("5! =120"), "(REL1 (FACT 5) 120)");
    CHECK_STR(D("a<b<c<d<e"), "(REL2222 a b c d e)");
    CHECK_STR(D("x+y=3, x-y=1"), "(LIST (REL1 (ADD x y) 3) (REL1 (ADD x (NEG y)) 1))");
    CHECK_STR(D("x=2 \"or\" x=-3"), "(LIST (REL1 x 2) 'or' (REL1 x -3))");
    CHECK_STR(D("\"so\" x=2"), "(LIST 'so' (REL1 x 2))");
    CHECK_STR(D("\"hello\""), "'hello'");
    CHECK_STR(D("(2,-3)"), "(LIST 2 -3)");
    CHECK_STR(D("[x+1]2"), "(MUL (ADD x 1) 2)");
    CHECK_STR(D("  7  "), "7");
    /* errors */
    CHECK_STR(D(""), "ERR Type something first @0");
    CHECK_STR(D("   "), "ERR Type something first @3");
    CHECK_STR(D("2+"), "ERR Something is missing at the end @2");
    CHECK_STR(D("(2+3"), "ERR A bracket is missing: add a ) @4");
    CHECK_STR(D("2+3)"), "ERR That symbol can't go there @3");
    CHECK_STR(D("((x)"), "ERR A bracket is missing: add a ) @4");
    CHECK_STR(D("(x]"), "ERR The brackets don't match: use ( with ) and [ with ] @2");
    CHECK_STR(D("()"), "ERR There is nothing inside the brackets @1");
    CHECK_STR(D("*3"), "ERR That symbol can't go there @0");
    CHECK_STR(D("2**3"), "(POW 2 3)");
    CHECK_STR(D("2*/3"), "ERR That symbol can't go there @2");
    CHECK_STR(D("|x"), "ERR The | is not closed: add another | @2");
    CHECK_STR(D("1.2.3"), "ERR A number can only have one decimal point @3");
    CHECK_STR(D("2_1"), "ERR Type a mixed number like 2_1/3 @1");
    CHECK_STR(D("2_1/0"), "ERR Can't divide by 0 @4");
    CHECK_STR(D("x$2"), "ERR Unknown symbol $ @1");
    CHECK_STR(D("\"abc"), "ERR A \" is missing to close the text @0");
    CHECK_STR(D("a<b<c<d<e<f"), "ERR Use only one = or < sign between two sides @9");
    CHECK_STR(D("root 2"), "ERR Put brackets after root, like root(...) @5");
    CHECK_STR(D("sin(1,2)"), "ERR sin needs 1 value in the brackets @0");
    CHECK_STR(D("recur(0.1(2)"), "ERR Type a recurring decimal like 0.1(27) @12");
    CHECK_STR(D("x="), "ERR Something is missing at the end @2");
    CHECK_STR(D("1,"), "ERR Something is missing at the end @2");

    /* deep nesting must fail cleanly, not overflow the stack */
    static char deep[1300];
    for (int i = 0; i < 600; i++) deep[i] = '(';
    deep[600] = '1';
    for (int i = 0; i < 600; i++) deep[601 + i] = ')';
    CHECK(strncmp(D(deep), "ERR That is too long", 20) == 0, "deep nesting: %s", out);
    char err[8];
    int pos;
    CHECK(ex_parse(NULL, err, sizeof err, &pos) == NULL && pos == 0, "NULL input");
    CHECK(ex_parse("2+", NULL, 0, NULL) == NULL, "NULL err buffers");
    return test_finish();
}
