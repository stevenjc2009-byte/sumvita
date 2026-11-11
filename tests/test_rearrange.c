/* alg_rearrange (change the subject) and alg_complete_square: exact printed
 * answers, numeric spot-checks that the answer is equivalent to the input, and
 * every step's maths re-parsed. DUMP=1 prints answers and steps. */
#include "solve_check.h"

static void reset(void)
{
    arena_reset(arena_current());
    calc_settings_default(&S);
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

/* pick values for the other letters, work out the subject from the answer,
 * and check the original formula holds (several sample points) */
static void spot(const char *in, const Expr *ans, const char *var)
{
    Expr *e = P(in);
    char names[8][12];
    int nv = ex_vars(e, names, 8);
    int checked = 0;
    for (int trial = 0; trial < 6; trial++) {
        Sol s = { 0, { 0 }, { 0 } };
        for (int i = 0; i < nv && s.n < 7; i++) {
            if (strcmp(names[i], var) == 0) continue;
            s.name[s.n] = names[i];
            s.val[s.n++] = 1.7 + 0.9 * trial + 1.3 * i;
        }
        bool pm = has_pm(ans->kids[1]);   /* a ± answer: both signs must work */
        for (int sg = 1; sg >= -1; sg -= 2) {
            double v = ev(pm ? pm_pick(ans->kids[1], sg) : ans->kids[1], &s);
            if (!isnan(v)) {
                Sol t = s;
                t.name[t.n] = var;
                t.val[t.n++] = v;
                CHECK(eq_holds(e, &t), "%s: answer %s fails at trial %d sign %d", in, ex_print(ans), trial, sg);
                checked++;
            }
            if (!pm) break;
        }
    }
    CHECK(checked >= 3, "%s: only %d spot checks ran", in, checked);
}

static void RA(const char *in, const char *var, const char *want)
{
    reset();
    Expr *ans;
    CHECK_STR(run_alg(A_REARR, in, var, &ans), want);
    if (ans) spot(in, ans, var);
}

/* the completed square equals the input for many x */
static void CSQ(const char *in, const char *want)
{
    reset();
    Expr *ans;
    CHECK_STR(run_alg(A_CSQ, in, NULL, &ans), want);
    if (!ans) return;
    Expr *e = P(in);
    int bad = 0;
    for (double x = -9.3; x < 9; x += 0.71) {
        Sol s = { 1, { "x" }, { x } };
        double a = ev(e, &s), b = ev(ans, &s);
        if (fabs(a - b) > 1e-9 * fmax(1, fabs(a))) bad++;
    }
    CHECK(bad == 0, "%s: completed square differs at %d points", in, bad);
}

static void rearrange(void)
{
    RA("v=u+at", "t", "t=(v-u)/a");
    CHECK(has_step("Subtract u from both sides") && has_step("Divide both sides by a"), "v=u+at steps");
    RA("A=πr^2", "r", "r=√(A/π)");
    CHECK(has_step("Square root both sides") && has_note("±"), "A=πr^2: root step with ± note");
    RA("ax+b=cx+d", "x", "x=(d-b)/(a-c)");
    CHECK(has_step("Collect the x terms") && has_math("x(a-c)=d-b"), "ax+b=cx+d: collect + factorise");
    RA("y=(x+1)/(x-2)", "x", "x=(2y+1)/(y-1)");
    CHECK(has_math("y(x-2)=x+1") && has_math("x(y-1)=2y+1"), "y=(x+1)/(x-2) steps");
    RA("y=mx+c", "x", "x=(y-c)/m");
    RA("y=3x-2", "x", "x=(y+2)/3");
    RA("P=2(l+w)", "w", "w=P/2-l");
    RA("v^2=u^2+2as", "s", "s=(v^2-u^2)/(2a)");
    RA("E=1/2mv^2", "v", "v=√(2E/m)");
    RA("y=√(x-3)", "x", "x=y^2+3");
    RA("V=4/3πr^3", "r", "r=root(3,3V/(4π))");
    RA("y=5-2x", "x", "x=(5-y)/2");
    RA("R=V/I", "I", "I=V/R");
    RA("s=(u+v)/2×t", "u", "u=2s/t-v");
    RA("a(x-b)=c", "x", "x=c/a+b");
    RA("k=x/(x+1)", "x", "x=k/(1-k)");
    RA("p=3q+r", "r", "r=p-3q");
    RA("y=e^x", "x", "x=ln(y)");
    RA("y=2^x", "x", "x=log(2,y)");
    RA("y=x/3+4", "x", "x=3(y-4)");
    reset();
    CHECK_STR(run_alg(A_REARR, "y=x+1", "z", NULL), "ERR:There is no z in this formula");
    reset();
    CHECK_STR(run_alg(A_REARR, "x+1", "x", NULL), "ERR:Type a formula with =");
    reset();
    CHECK_STR(run_alg(A_REARR, "y=x+1", "", NULL), "ERR:Which letter should be the subject?");
    reset();
    CHECK_STR(run_alg(A_REARR, "y=sin(x)", "x", NULL), "x=asin(y)");
}

/* the subject in a quadratic: quadratic formula with the other letters as numbers */
static void rearrange_quadratic(void)
{
    RA("y=x^2+x", "x", "x=(-1±√(1+4y))/2");
    CHECK(has_step("Rearrange so one side is 0") && has_math("x^2+x-y=0"), "y=x^2+x: =0 form");
    CHECK(has_step("Use the quadratic formula") && has_note("c=-y"), "y=x^2+x: a, b, c");
    CHECK(has_math("x=(-1±√(1^2-4×1×(-y)))/(2×1)"), "y=x^2+x: substitution line");
    RA("y=x^2+2x", "x", "x=-1±√(1+y)");
    CHECK(has_step("Cancel the common factor 2"), "y=x^2+2x: common factor cancelled");
    RA("y=2x^2+4x", "x", "x=(-2±√(4+2y))/2");
    RA("y=-x^2+5x", "x", "x=(5±√(25-4y))/2");
    RA("y=ax^2+bx", "x", "x=(-b±√(b^2+4ay))/(2a)");
    RA("s=ut+1/2at^2", "t", "t=(-u±√(u^2+2as))/a");
    CHECK(has_step("Multiply every term by 2"), "s=ut+1/2at^2: fraction cleared");
    RA("s=ut+1/2at^2", "u", "u=(s-1/2at^2)/t");
    RA("y=(x^2+1)/x", "x", "x=(y±√(y^2-4))/2");
    RA("y=3x^2+x^2", "x", "x=±√(y/4)");
    reset();
    CHECK_STR(run_alg(A_REARR, "y=x^3+x", "x", NULL), "ERR:Can't make x the subject of this formula");
    reset();
    CHECK_STR(run_alg(A_REARR, "y=sin(x)+x", "x", NULL), "ERR:Can't make x the subject of this formula");
    reset();
    CHECK_STR(run_alg(A_REARR, "y=x^4+x^2", "x", NULL), "ERR:Can't make x the subject of this formula");
}

static void complete_square(void)
{
    CSQ("2x^2+8x+3", "2(x+2)^2-5");
    CHECK(has_step("Take out the factor 2") && has_math("2(x^2+4x)+3"), "2x^2+8x+3: factor-out step");
    CHECK(has_step("Turning point") && has_math("(-2, -5)") && has_note("minimum"), "2x^2+8x+3 turning point");
    CSQ("x^2+6x+5", "(x+3)^2-4");
    CHECK(has_step("Halve the coefficient") && has_math("(-3, -4)"), "x^2+6x+5 steps");
    CSQ("x^2-3x+1", "(x-3/2)^2-5/4");
    CHECK(has_math("(3/2, -5/4)"), "x^2-3x+1 turning point");
    CSQ("-x^2+4x+1", "-(x-2)^2+5");
    CHECK(has_note("maximum") && has_math("(2, 5)"), "-x^2+4x+1 maximum");
    CSQ("3x^2-12x", "3(x-2)^2-12");
    CSQ("x^2+5", "x^2+5");
    CSQ("2x^2+3x+1", "2(x+3/4)^2-1/8");
    CSQ("x^2-10x+25", "(x-5)^2");
    CSQ("1/2x^2+x", "1/2(x+1)^2-1/2");
    CHECK(has_math("1/2(x^2+2x)") && has_math("1/2((x+1)^2-1)"), "1/2x^2+x: 1/2 written in front of the bracket");
    CSQ("-1/2x^2+3x+1", "-1/2(x-3)^2+11/2");
    CSQ("2/3x^2+4x", "2/3(x+3)^2-6");
    CSQ("5+4x+x^2", "(x+2)^2+1");
    CSQ("y^2-4y", "(y-2)^2-4");
    reset();
    CHECK_STR(run_alg(A_CSQ, "x+1", NULL, NULL), "ERR:Completing the square needs a quadratic like x^2+6x+5");
    reset();
    CHECK_STR(run_alg(A_CSQ, "x^3+x", NULL, NULL), "ERR:Completing the square needs a quadratic like x^2+6x+5");
    reset();
    CHECK_STR(run_alg(A_CSQ, "x^2+y", NULL, NULL), "ERR:Use just one letter");
}

int main(void)
{
    dump = getenv("DUMP") != NULL;
    rearrange();
    rearrange_quadratic();
    complete_square();
    return test_finish();
}
