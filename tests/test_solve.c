/* alg_solve: linear, quadratic, cubic/quartic, non-polynomial, inequalities,
 * simultaneous equations. Exact printed answers, every solution substituted
 * back, every step's maths re-parsed. DUMP=1 prints answers and steps. */
#include "solve_check.h"

static void reset(void)
{
    arena_reset(arena_current());
    calc_settings_default(&S);
}

/* exact answer + substitution check (at least `need` solutions verified) */
static void EQ(const char *in, const char *want, int need)
{
    reset();
    Expr *ans;
    const char *got = run_alg(A_SOLVE, in, NULL, &ans);
    CHECK_STR(got, want);
    if (ans) {
        int n = verify(in, ans);
        CHECK(n >= need, "%s: only %d solutions substituted (want %d)", in, n, need);
    }
}

static void INEQ(const char *in, const char *want)
{
    reset();
    Expr *ans;
    CHECK_STR(run_alg(A_SOLVE, in, NULL, &ans), want);
    verify_ineq(in, ans);
}

static bool has_step(const char *title)
{
    for (int i = 0; i < ST.n; i++)
        if (strstr(ST.v[i].title, title)) return true;
    return false;
}

static bool has_note(const char *text)
{
    for (int i = 0; i < ST.n; i++)
        if (ST.v[i].note && strstr(ST.v[i].note, text)) return true;
    return false;
}

static bool has_math(const char *m)
{
    for (int i = 0; i < ST.n; i++)
        if (ST.v[i].math && strcmp(ST.v[i].math, m) == 0) return true;
    return false;
}

/* every "eliminate" step's equation has whole-number coefficients (and there is one) */
static bool elim_whole(void)
{
    int n = 0;
    for (int i = 0; i < ST.n; i++) {
        if (!strstr(ST.v[i].title, "eliminate")) continue;
        n++;
        if (!ST.v[i].math || strchr(ST.v[i].math, '/')) return false;
    }
    return n > 0;
}

static void linear(void)
{
    EQ("3(x-2)=2x+5", "x=11", 1);
    CHECK(has_step("Expand the brackets") && has_step("Subtract 2x from both sides"), "3(x-2)=2x+5 steps");
    EQ("(x+1)/3+(x-2)/4=2", "x=26/7", 1);
    CHECK(has_step("Multiply both sides by 12") && has_note("lowest common multiple"), "fractions: LCM step");
    EQ("5-2x=11", "x=-3", 1);
    CHECK(has_step("Divide both sides by -2"), "5-2x=11 divide step");
    EQ("0.5x+1.2=3.7", "x=5", 1);
    CHECK(has_note("decimals"), "decimals note");
    EQ("2x+3=x-4", "x=-7", 1);
    EQ("4(2x-1)=3(x+2)", "x=2", 1);
    EQ("x/2=7", "x=14", 1);
    EQ("7=2x-1", "x=4", 1);
    EQ("x/3-x/4=1", "x=12", 1);
    EQ("3x=0", "x=0", 1);
    EQ("2(x+3)-5=3(x-1)", "x=4", 1);
    EQ("0x=5", "\"no solution\"", 0);
    EQ("2x+1=2x+3", "\"no solution\"", 0);
    EQ("2x+1=2x+1", "\"true for all values of x\"", 0);
    EQ("2y-3=7", "y=5", 1);
}

static void quadratics(void)
{
    EQ("x^2-5x+6=0", "x=2, x=3", 2);
    CHECK(has_step("Factorise") && has_math("(x-2)(x-3)=0"), "x^2-5x+6 factorised form");
    CHECK(has_math("x-2=0 \"or\" x-3=0"), "x^2-5x+6 either/or line");
    EQ("x^2-3x+1=0", "x=(3±√5)/2", 2);
    CHECK(has_step("Use the quadratic formula") && has_step("Substitute"), "formula steps");
    CHECK(has_math("x=2.62 \"or\" x=0.38") || has_math("x=0.38 \"or\" x=2.62"), "2 d.p. decimals line");
    EQ("x^2+4x-3=0", "x=-2±√7", 2);
    EQ("x^2+x+1=0", "\"no real solutions\"", 0);
    CHECK(has_step("No real solutions") || has_note("negative"), "D<0 explained");
    EQ("x^2=5x-6", "x=2, x=3", 2);
    EQ("x(x+3)=10", "x=-5, x=2", 2);
    EQ("2/x+x=3", "x=1, x=2", 2);
    CHECK(has_step("Multiply both sides by x"), "2/x+x=3 multiply by x");
    EQ("x^2=49", "x=±7", 2);
    EQ("2x^2-8=0", "x=±2", 2);
    EQ("x^2-6x+9=0", "x=3", 1);
    EQ("6x^2+x-2=0", "x=-2/3, x=1/2", 2);
    EQ("x^2-2x=0", "x=0, x=2", 2);
    EQ("3x^2-5x-1=0", "x=(5±√37)/6", 2);
    EQ("x^2-8=0", "x=±2√2", 2);
    EQ("0x^2+2x-4=0", "x=2", 1);
    EQ("(x+1)^2=4", "x=-3, x=1", 2);
    EQ("x^2+2x=2x+9", "x=±3", 2);
    reset();
    S.dp = 3;
    run_alg(A_SOLVE, "x^2-3x+1=0", NULL, NULL);
    CHECK(has_math("x=2.618 \"or\" x=0.382") || has_math("x=0.382 \"or\" x=2.618"), "dp setting used for decimals");
}

static void higher(void)
{
    EQ("x^3-6x^2+11x-6=0", "x=1, x=2, x=3", 3);
    CHECK(has_step("factor theorem"), "factor theorem step");
    EQ("x^3-x=0", "x=-1, x=0, x=1", 3);
    EQ("x^3=8", "x=2", 1);
    EQ("x^4-5x^2+4=0", "x=-2, x=-1, x=1, x=2", 4);
    EQ("x^3-2x-5=0", "x=2.094551482", 1);
    CHECK(has_step("numerically"), "cubic numeric step");
    EQ("x^3-3x^2+3x-1=0", "x=1", 1);
    EQ("2x^3-3x^2-11x+6=0", "x=-2, x=1/2, x=3", 3);
}

static void nonpoly(void)
{
    EQ("√x=5", "x=25", 1);
    CHECK(has_step("Square both sides"), "sqrt: square both sides");
    EQ("√(x+1)=-2", "\"no real solutions\"", 0);
    EQ("2^x=32", "x=5", 1);
    CHECK(has_step("as a power of 2"), "2^x: matching powers");
    EQ("4^x=8", "x=3/2", 1);
    EQ("3/x=6", "x=1/2", 1);
    EQ("2^x=10", "x=3.321928095", 1);
    EQ("ln(x)=1", "x=2.718281828", 1);
    CHECK(has_step("numerically"), "ln(x)=1 numeric note");
    EQ("2+3=5", "\"true\"", 0);
    EQ("2+2=5", "\"false\"", 0);
    EQ("x/(x-1)=2", "x=2", 1);
    EQ("1/(x-2)=1/(x-2)+1", "\"no real solutions\"", 0);
}

static void inequalities(void)
{
    INEQ("2x+1>7", "x>3");
    INEQ("5-2x>=1", "x≤2");
    CHECK(has_note("negative"), "flip note when dividing by a negative");
    INEQ("3x-2<x+4", "x<3");
    INEQ("-x/2>3", "x<-6");
    INEQ("-3<2x+1<=7", "-2<x≤3");
    CHECK(has_step("all three parts") && has_step("Integer solutions"), "double inequality steps");
    CHECK(has_math("-1, 0, 1, 2, 3"), "integer solutions -1..3");
    INEQ("4<=3-x<=10", "-7≤x≤-1");
    INEQ("x^2-x-6>0", "x<-2, x>3");
    CHECK(has_step("Find the critical values") && has_step("Sketch the graph"), "quad inequality steps");
    INEQ("x^2-x-6<=0", "-2≤x≤3");
    INEQ("x^2<9", "-3<x<3");
    INEQ("x^2+1>0", "\"true for all values of x\"");
    INEQ("x^2-2x+1<0", "\"no values of x\"");
    INEQ("x^2>=4x", "x≤0, x≥4");
    INEQ("2x+1>2x", "\"true for all values of x\"");
}

static void systems(void)
{
    EQ("2x+3y=12, x-y=1", "x=3, y=2", 1);
    CHECK(has_step("Number the equations") && has_step("eliminate") && has_step("Substitute") && has_step("Check"),
          "2x2 elimination steps");
    EQ("3x+2y=4, 5x-3y=13", "x=2, y=-1", 1);
    EQ("x+y+z=6, 2x-y+z=3, x+2y-z=2", "x=1, y=2, z=3", 1);
    EQ("x+y=2, x+y=3", "\"no solution\"", 0);
    EQ("x+y=2, 2x+2y=4", "\"infinitely many solutions\"", 0);
    EQ("y=2x+1, 3x+y=11", "x=2, y=5", 1);
    EQ("x/2+y=4, x-y=2", "x=4, y=2", 1);
    CHECK(has_step("Multiply equation (1) by 2 to clear the fraction") && has_math("x+2y=8"), "x/2+y=4: fraction cleared");
    CHECK(elim_whole(), "x/2+y=4: elimination uses whole numbers");
    EQ("x/2+y/3=4, x-y=2", "x=28/5, y=18/5", 1);
    CHECK(has_step("Multiply equation (1) by 6 to clear the fractions") && has_math("3x+2y=24"), "x/2+y/3: LCM 6");
    CHECK(elim_whole(), "x/2+y/3=4: elimination uses whole numbers");
    EQ("0.5x+y=4, x-y=2", "x=4, y=2", 1);
    CHECK(has_step("Multiply equation (1) by 2 to clear the decimal") && elim_whole(), "0.5x+y=4: decimal cleared");
    EQ("x+y=1/2, x-y=3/2", "x=1, y=-1/2", 1);
    CHECK(has_step("Multiply equation (2) by 2 to clear the fraction") && elim_whole(), "fraction on the right cleared");
    EQ("x/2+y/4+z=3, x-y+z=1, x+y-z=1", "x=1, y=2, z=2", 1);
    CHECK(has_step("Multiply equation (1) by 4 to clear the fractions") && elim_whole(), "3x3 fractions cleared");
    EQ("y=x+1, y=x^2-1", "(x=-1, y=0), (x=2, y=3)", 2);
    CHECK(has_step("Substitute into"), "linear+quadratic substitution step");
    EQ("x^2+y^2=25, y=x+1", "(x=-4, y=-3), (x=3, y=4)", 2);
    EQ("x^2+y^2=1, y=x+5", "\"no real solutions\"", 0);
    EQ("x^2+y^2=10, x+y=4", "(x=1, y=3), (x=3, y=1)", 2);
    EQ("y=x^2-3x+2, y=x-1", "(x=1, y=0), (x=3, y=2)", 2);
}

static void edges(void)
{
    reset();
    CHECK_STR(run_alg(A_SOLVE, "x+y", NULL, NULL), "x=-y");
    reset();
    CHECK_STR(run_alg(A_SOLVE, "y+z=1", NULL, NULL), "ERR:Which letter should I solve for?");
    reset();
    strcpy(S.target_var, "y");
    CHECK_STR(run_alg(A_SOLVE, "2y-4=0", NULL, NULL), "y=2");
    reset();
    strcpy(S.target_var, "y");
    CHECK_STR(run_alg(A_SOLVE, "2x+y=5", NULL, NULL), "y=5-2x");
    reset();
    CHECK_STR(run_alg(A_SOLVE, "2x+y=5", NULL, NULL), "x=(5-y)/2");
    /* huge coefficients: must not crash, falls back to numbers */
    reset();
    Expr *ans;
    const char *g = run_alg(A_SOLVE, "4611686018427387903x^2+4611686018427387903x-4611686018427387903=0", NULL, &ans);
    CHECK(ans != NULL, "huge quadratic answered (%s)", g);
    if (ans) CHECK(verify("4611686018427387903x^2+4611686018427387903x-4611686018427387903=0", ans) == 2, "huge: 2 roots");
    reset();
    g = run_alg(A_SOLVE, "x^2-1000000000000x+1=0", NULL, &ans);
    CHECK(ans != NULL, "big discriminant answered (%s)", g);
    reset();
    g = run_alg(A_SOLVE, "x^9-9223372036854775807x+3=0", NULL, &ans);
    CHECK(ans != NULL || strncmp(g, "ERR:", 4) == 0, "degree 9 huge: no crash");
    reset();
    CHECK_STR(run_alg(A_SOLVE, "x^2≠4", NULL, NULL), "ERR:Can't solve a \xE2\x89\xA0 statement");
}

int main(void)
{
    dump = getenv("DUMP") != NULL;
    linear();
    quadratics();
    higher();
    nonpoly();
    inequalities();
    systems();
    edges();
    return test_finish();
}
