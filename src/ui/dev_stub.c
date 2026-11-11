/* dev_stub.c - UI_ONLY stand-ins for the engine entry points (calc_run,
 * calc_settings_default, calc_action_name) and the GCSE tool table, so the UI
 * builds and runs before the engine is finished. Canned results with a couple
 * of fake steps. Not compiled into the real build. */
#ifdef SUMVITA_UI_ONLY

#include "engine/arena.h"
#include "engine/calc.h"
#include "engine/expr.h"
#include "engine/num.h"
#include "tools/tools.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void calc_settings_default(CalcSettings *s)
{
	memset(s, 0, sizeof *s);
	s->degrees = true;
	s->dp = -1;
	s->sf = 0;
	s->prefer_exact = true;
}

const char *calc_action_name(CalcAction a)
{
	static const char *n[ACT_COUNT] = { "Auto", "Evaluate", "Simplify", "Expand", "Factorise", "Solve",
		"Complete square", "Rearrange", "Fraction", "Decimal", "Percent", "Mixed number",
		"Standard form", "Surd" };
	return (unsigned)a < (unsigned)ACT_COUNT ? n[a] : "?";
}

static const char *canned(CalcAction a)
{
	switch (a) {
	case ACT_SIMPLIFY:    return "5x+2y";
	case ACT_EXPAND:      return "x^2+5x+6";
	case ACT_FACTOR:      return "(x+2)(x+3)";
	case ACT_SOLVE:       return "x=-2 \"or\" x=-3";
	case ACT_COMPLETE_SQ: return "(x+5/2)^2-1/4";
	case ACT_REARRANGE:   return "x=(y-c)/m";
	case ACT_TO_FRACTION: return "3/4";
	case ACT_TO_DECIMAL:  return "0.75";
	case ACT_TO_PERCENT:  return "75%";
	case ACT_TO_MIXED:    return "1_3/4";
	case ACT_STD_FORM:    return "7.5\xC3\x97" "10^-1";
	case ACT_SURD:        return "2\xE2\x88\x9A(3)";
	default:              return NULL;
	}
}

void calc_run(CalcAction action, const char *input, const CalcSettings *s, CalcResult *out)
{
	memset(out, 0, sizeof *out);
	steps_init(&out->steps);
	out->error_pos = -1;
	out->input_echo = astrdup(input ? input : "");
	if (!input || !*input) {
		snprintf(out->error, sizeof out->error, "Nothing to calculate");
		return;
	}
	char err[96];
	int pos = -1;
	Expr *e = ex_parse(input, err, (int)sizeof err, &pos);
	if (!e) {
		snprintf(out->error, sizeof out->error, "%s", err);
		out->error_pos = pos;
		return;
	}
	const char *c = canned(action);
	double v = 0;
	EvalEnv env;
	memset(&env, 0, sizeof env);
	env.degrees = s ? s->degrees : true;
	if (s && s->ans_set) {
		env.var_names[0] = "Ans";
		env.var_values[0] = s->ans;
		env.nvars = 1;
	}
	if (!c && action != ACT_SOLVE && ex_eval(e, &env, &v)) {
		char buf[64];
		dbl_to_str(v, 10, buf, sizeof buf);
		out->answer = astrdup(buf);
		out->value = v;
		out->has_value = true;
		Rat r = rat_make((long long)llround(v * 8), 8);
		if (fabs(v * 8 - (double)llround(v * 8)) < 1e-9 && r.d != 1) {
			char fb[48];
			rat_to_str(r, fb, sizeof fb);
			out->answer_alt = out->answer;
			out->answer = astrdup(fb);
		} else {
			out->answer_alt = "";
		}
	} else if (!c && ex_has_var(e, NULL)) {
		out->answer = astrdup(e->kind == E_REL ? "x=4" : "2x+3");
		out->answer_alt = "";
	} else if (c) {
		out->answer = c;
		out->answer_alt = action == ACT_TO_FRACTION ? "0.75" : "";
	} else {
		snprintf(out->error, sizeof out->error, "Maths error");
		return;
	}
	out->ok = true;
	steps_add(&out->steps, "Write the question", "(stub) what was typed", "%s", input);
	steps_push(&out->steps);
	steps_add(&out->steps, "Work it out", "(stub) a nested step, indented under its parent",
		  "1/2+1/3=3/6+2/6");
	steps_pop(&out->steps);
	steps_add(&out->steps, "Answer", "", "%s", out->answer);
}

/* ---- two fake GCSE tools ---- */

static void run_pct(const char *const *vals, const CalcSettings *s, CalcResult *out)
{
	(void)s;
	memset(out, 0, sizeof *out);
	steps_init(&out->steps);
	out->error_pos = -1;
	double a = atof(vals[0]), b = atof(vals[1]);
	if (a == 0) {
		snprintf(out->error, sizeof out->error, "Original value can't be 0");
		return;
	}
	double p = (b - a) / a * 100;
	char buf[48];
	dbl_to_str(p, 6, buf, sizeof buf);
	out->answer = asprintf_a("%s%%", buf);
	out->answer_alt = "";
	out->ok = true;
	steps_add(&out->steps, "Find the change", "new - original", "%s-%s", vals[1], vals[0]);
	steps_add(&out->steps, "Divide by the original, times 100", "",
		  "(%s-%s)/%s\xC3\x97" "100", vals[1], vals[0], vals[0]);
}

static void run_hyp(const char *const *vals, const CalcSettings *s, CalcResult *out)
{
	(void)s;
	memset(out, 0, sizeof *out);
	steps_init(&out->steps);
	out->error_pos = -1;
	double a = atof(vals[0]), b = atof(vals[1]);
	char buf[48];
	dbl_to_str(sqrt(a * a + b * b), 6, buf, sizeof buf);
	out->answer = astrdup(buf);
	out->answer_alt = asprintf_a("\xE2\x88\x9A(%g)", a * a + b * b);
	out->ok = true;
	steps_add(&out->steps, "Pythagoras", "c\xC2\xB2 = a\xC2\xB2 + b\xC2\xB2", "c^2=%s^2+%s^2", vals[0], vals[1]);
	steps_add(&out->steps, "Square root", "", "c=\xE2\x88\x9A(%g)", a * a + b * b);
}

static const Tool k_tools[] = {
	{ "pct_change", TOOL_CAT_NUMBER, "Percentage change", "How much did it go up or down, as a %?", 3,
	  { { "Original", "e.g. 240", "240", TF_NUMBER, NULL, false },
	    { "New", "e.g. 300", "300", TF_NUMBER, NULL, false },
	    { "Show as", "", "", TF_CHOICE, "Increase|Decrease", true } },
	  run_pct },
	{ "hyp", TOOL_CAT_GEOMETRY, "Hypotenuse", "Longest side of a right-angled triangle", 2,
	  { { "Side a", "e.g. 3", "3", TF_NUMBER, NULL, false },
	    { "Side b", "e.g. 4", "4", TF_NUMBER, NULL, false } },
	  run_hyp },
};

const Tool *tools_all(int *count)
{
	*count = (int)(sizeof k_tools / sizeof k_tools[0]);
	return k_tools;
}

int tools_categories(const char **out, int max)
{
	int n = 0;
	for (size_t i = 0; i < sizeof k_tools / sizeof k_tools[0]; i++) {
		int dup = 0;
		for (int j = 0; j < n; j++)
			if (!strcmp(out[j], k_tools[i].category))
				dup = 1;
		if (!dup && n < max)
			out[n++] = k_tools[i].category;
	}
	return n;
}

const Tool *tools_find(const char *id)
{
	for (size_t i = 0; i < sizeof k_tools / sizeof k_tools[0]; i++)
		if (!strcmp(k_tools[i].id, id))
			return &k_tools[i];
	return NULL;
}

#else
typedef int dev_stub_unused_t;
#endif
