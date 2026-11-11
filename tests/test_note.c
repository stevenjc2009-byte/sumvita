/* note_text (src/ui/note_text.c): ASCII maths in step titles / notes shown as
 * the maths line shows it - and the bug-check engine notes come out with no
 * "^", "*" or "sqrt(" left in them. */
#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"
#include "ui/note_text.h"
#include <stdlib.h>

#define SQ "\xC2\xB2"
#define CU "\xC2\xB3"
#define SUPN "\xE2\x81\xBF"
#define SUPM "\xE2\x81\xBB"
#define SUP1 "\xC2\xB9"

static char got[512];

static const char *NT(const char *s)
{
	char *d = note_text(s);
	snprintf(got, sizeof got, "%s", d ? d : "(null)");
	free(d);
	return got;
}

/* no ASCII maths left in any title or note of one run */
static void clean(const char *what, const CalcResult *r)
{
	for (int i = 0; i < r->steps.n; i++)
		for (int k = 0; k < 2; k++) {
			const char *s = NT(k ? r->steps.v[i].note : r->steps.v[i].title);
			CHECK(!strchr(s, '^') && !strchr(s, '*') && !strstr(s, "sqrt("), "%s: step %d %s \"%s\"", what, i,
			      k ? "note" : "title", s);
		}
}

static void run(CalcAction a, const char *in, const char *var)
{
	CalcSettings s;
	CalcResult r;
	calc_settings_default(&s);
	if (var) snprintf(s.target_var, sizeof s.target_var, "%s", var);
	arena_reset(arena_current());
	calc_run(a, in, &s, &r);
	CHECK(r.ok, "%s failed: %s", in, r.error);
	clean(in, &r);
}

int main(void)
{
	/* the converter */
	CHECK_STR(NT("from the x^2 and x terms"), "from the x" SQ " and x terms");
	CHECK_STR(NT("x^2+4x=(x+2)^2-4"), "x" SQ "+4x=(x+2)" SQ "-4");
	CHECK_STR(NT("(x^2-4) is in every group."), "(x" SQ "-4) is in every group.");
	CHECK_STR(NT("V=4/3*pi*r^3"), "V=4/3\xC3\x97pi\xC3\x97r" CU);
	CHECK_STR(NT("a^-1 and r^n"), "a" SUPM SUP1 " and r" SUPN);
	CHECK_STR(NT("first term x ratio^(n-1)"), "first term x ratio" SUPN SUPM SUP1);
	CHECK_STR(NT("x^10"), "x\xC2\xB9\xE2\x81\xB0");
	CHECK_STR(NT("sqrt(2) but not isqrt(2)"), "\xE2\x88\x9A(2) but not isqrt(2)");
	CHECK_STR(NT("e^x, x^(a+b), 2^"), "e^x, x^(a+b), 2^");    /* nothing to raise: left as typed */
	CHECK_STR(NT("r^nd"), "r^nd");
	CHECK_STR(NT("already x\xC2\xB2 \xC3\x97 2"), "already x\xC2\xB2 \xC3\x97 2");
	CHECK_STR(NT(NULL), "");
	CHECK_STR(NT(""), "");

	/* the bug-check cases, through the real engine */
	run(ACT_COMPLETE_SQ, "x^2+4x", NULL);
	run(ACT_COMPLETE_SQ, "2x^2+8x+3", NULL);
	run(ACT_COMPLETE_SQ, "x^2+5", NULL);
	run(ACT_FACTOR, "x^3+2x^2-4x-8", NULL);
	run(ACT_FACTOR, "2x^2y-8y+3x^2-12", NULL);
	run(ACT_REARRANGE, "y=(x+1)/(x-2)", "x");
	run(ACT_REARRANGE, "a=x^2b+c", "x");
	run(ACT_SOLVE, "x^2-5x+6=0", NULL);
	run(ACT_SURD, "1/(3-\xE2\x88\x9A" "2)", NULL);

	/* every GCSE tool's notes are swept in test_tools.c (real field values) */
	return test_finish();
}
