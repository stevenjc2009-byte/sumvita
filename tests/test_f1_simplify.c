/* simp_working (src/engine/simplify_steps.c): the Simplify action's GCSE working.
 * Answers, step titles in order, and every step's maths re-parses to the same value.
 * Build with -DF1_RED to run the same checks against the old path (ex_simplify only). */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/simplify_int.h"

static Steps st;

#ifdef F1_RED
static bool old_path(const Expr *t, Steps *s, Expr **out)
{
    *out = ex_tidy(ex_simplify(t));
    steps_add(s, "Simplify", "", "%s", ex_print(*out));
    return true;
}
#define simp_working old_path
#endif

static Expr *P(const char *s)
{
    char err[96];
    int pos;
    return ex_parse(s, err, sizeof err, &pos);
}

/* value of e with x, a, b replaced by sample numbers (exact) */
static Expr *at(const Expr *e, int k)
{
    Expr *v = ex_subst(e, "x", ex_int(3 + k));
    v = ex_subst(v, "a", ex_int(5 + 2 * k));
    v = ex_subst(v, "b", ex_int(7 + k));
    return ex_simplify(v);
}

static const char *run(const char *in)
{
    arena_reset(arena_current());
    steps_init(&st);
    Expr *out = NULL;
    if (!simp_working(P(in), &st, &out) || !out) return "(not handled)";
    return ex_print(out);
}

/* titles joined with " | " */
static const char *titles(void)
{
    const char *s = "";
    for (int i = 0; i < st.n; i++) s = asprintf_a("%s%s%s", s, i ? " | " : "", st.v[i].title);
    return s;
}

/* every "a=b" or plain step line parses, and each side equals the input in value */
static void lines_ok(const char *in)
{
    Expr *e = P(in);
    for (int i = 0; i < st.n; i++) {
        const char *m = st.v[i].math;
        if (!m || !*m) continue;
        Expr *line = P(m);
        CHECK(line != NULL, "%s: step %d \"%s\" does not parse", in, i + 1, m);
        if (!line) continue;
        int nside = line->kind == E_REL ? line->nkids : 1;
        for (int s = 0; s < nside; s++) {
            const Expr *side = line->kind == E_REL ? line->kids[s] : line;
            for (int k = 0; k < 3; k++)
                CHECK(ex_equal(at(side, k), at(e, k)), "%s: step %d \"%s\" side %d has a different value", in,
                      i + 1, m, s + 1);
        }
    }
}

#define S(in, want, want_titles) do { \
    CHECK_STR(run(in), want); \
    CHECK_STR(titles(), want_titles); \
    lines_ok(in); \
} while (0)

int main(void)
{
    /* item 1: numeric denominators over a common denominator */
    S("(x+1)/2+(x-1)/3", "(5x+1)/6",
      "Write over a common denominator 6 | Add the tops | Expand the brackets | Collect like terms");
    CHECK(st.n && strstr(st.v[0].note, "lowest common multiple of 2 and 3"), "item 1: LCM note");
    S("(x+1)/2-(x-1)/3", "(x+5)/6",
      "Write over a common denominator 6 | Add the tops | Expand the brackets | Collect like terms");
    S("x/2+x/3", "5x/6", "Write over a common denominator 6 | Add the tops | Collect like terms");
    S("(x+1)/4+(x+3)/4", "(x+2)/2",
      "Write over a common denominator 4 | Add the tops | Collect like terms | Cancel the common factor 2");
    /* items 2, 3, 4: expand, then collect */
    S("3(x+1)-3x", "3", "Expand the brackets | Collect like terms");
    S("2(x+3)+4(x-1)", "6x+2", "Expand the brackets | Collect like terms");
    S("(a+b)^2-a^2-b^2", "2ab", "Expand the brackets | Collect like terms");
    CHECK(st.n && strstr(st.v[0].note, "(a+b)(a+b)"), "item 4: squared bracket note");
    S("x(x+2)-x^2", "2x", "Expand the brackets | Collect like terms");
    /* item 6 and T1's sqrt(4a^2): GCSE convention, letters taken as ≥ 0 */
    S("sqrt(x^2)", "x", "Square root each part");
    CHECK(st.n && strstr(st.v[0].note, "x \xE2\x89\xA5 0"), "item 6: x >= 0 note");
    S("sqrt(4a^2)", "2a", "Square root each part");
    S("\xE2\x88\x9A(9x^2)+x", "4x", "Square root each part | Collect like terms");
    /* not a simplification by expanding: left to the old path */
    CHECK_STR(run("2(x+3)+4y"), "(not handled)");
    CHECK_STR(run("3(x+1)"), "(not handled)");
    CHECK_STR(run("x^2+2x"), "(not handled)");
    CHECK_STR(run("sqrt(2x)"), "(not handled)");
    CHECK_STR(run("x=2"), "(not handled)");
    return test_finish();
}
