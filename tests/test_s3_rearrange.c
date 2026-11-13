/* Tidy answers when changing the subject: nested fractions are cleared, common
 * factors taken out, products written number, π, letters. Every answer is also
 * substituted back into the formula at several points. */
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

/* the last step's maths is the answer line */
static bool last_is(const char *m)
{
    return ST.n > 0 && ST.v[ST.n - 1].math && strcmp(ST.v[ST.n - 1].math, m) == 0;
}

/* put the answer back into the formula at 6 sets of values for the other letters */
static void spot(const char *in, const Expr *ans, const char *var)
{
    Expr *e = P(in);
    char names[8][12];
    int nv = ex_vars(e, names, 8), checked = 0;
    for (int trial = 0; trial < 6; trial++) {
        Sol s = { 0, { 0 }, { 0 } };
        for (int i = 0; i < nv && s.n < 7; i++) {
            if (strcmp(names[i], var) == 0) continue;
            s.name[s.n] = names[i];
            s.val[s.n++] = 1.7 + 0.9 * trial + 1.3 * i;
        }
        double v = ev(ans->kids[1], &s);
        if (isnan(v)) continue;
        Sol t = s;
        t.name[t.n] = var;
        t.val[t.n++] = v;
        CHECK(eq_holds(e, &t), "%s: answer %s fails at trial %d", in, ex_print(ans), trial);
        checked++;
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

/* answer is exactly `want` and the working says "Simplify the fraction" last */
static void RT(const char *in, const char *var, const char *want)
{
    RA(in, var, want);
    CHECK(has_step("Simplify the fraction"), "%s for %s: no Simplify the fraction step", in, var);
    CHECK(last_is(want) || (ST.n > 1 && ST.v[ST.n - 2].math && strcmp(ST.v[ST.n - 2].math, want) == 0),
          "%s for %s: working does not end at %s", in, var, want);
}

static void reviewer_cases(void)
{
    RT("1/f=1/u+1/v", "f", "f=uv/(u+v)");
    RT("1/f=1/u+1/v", "v", "v=fu/(u-f)");
    RT("1/x+1/y=1/f", "y", "y=fx/(x-f)");
    RT("s=(u+v)/2×t", "t", "t=2s/(u+v)");
    RT("T=2π√(l/g)", "g", "g=4π^2l/T^2");
    RT("x=(a+b)/(a-b)", "a", "a=b(x+1)/(x-1)");
}

static void variations(void)
{
    RT("1/a=1/b-1/c", "a", "a=bc/(c-b)");
    RT("1/a=1/b-1/c", "b", "b=ac/(a+c)");
    RT("1/a=1/b-1/c", "c", "c=ab/(a-b)");
    RT("1/a=1/b+1/c", "a", "a=bc/(b+c)");
    RT("1/a=1/b+1/c", "b", "b=ac/(c-a)");
    RT("1/a=1/b+1/c", "c", "c=ab/(b-a)");
    RT("1/p+1/q=1/f", "p", "p=fq/(q-f)");
    RT("d=(a+b)/2×t", "t", "t=2d/(a+b)");
    RT("A=(a+b)/2×h", "h", "h=2A/(a+b)");
}

/* answers that were already tidy are left alone (no extra step) */
static void unchanged(void)
{
    const char *f[][3] = {
        { "v^2=u^2+2as", "a", "a=(v^2-u^2)/(2s)" },
        { "v^2=u^2+2as", "s", "s=(v^2-u^2)/(2a)" },
        { "P=2(l+w)", "w", "w=P/2-l" },
        { "y=(x+1)/(x-1)", "x", "x=(y+1)/(y-1)" },
        { "y=(x-1)/(x+1)", "x", "x=(y+1)/(1-y)" },
        { "y=2x/(x+1)", "x", "x=y/(2-y)" },
        { "V=πr^2h", "h", "h=V/(πr^2)" },
        { "T=2π√(l/g)", "l", "l=gT^2/(4π^2)" },
        { "ax+b=cx+d", "x", "x=(d-b)/(a-c)" },
    };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) {
        RA(f[i][0], f[i][1], f[i][2]);
        CHECK(!has_step("Simplify the fraction"), "%s for %s: tidy step where nothing needed tidying", f[i][0], f[i][1]);
    }
}

int main(void)
{
    dump = getenv("DUMP") != NULL;
    reviewer_cases();
    variations();
    unchanged();
    return test_finish();
}
