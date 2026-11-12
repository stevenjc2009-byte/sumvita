/* calcview_actions.c - the action row under the answer: FDP conversions in
 * Calc / Scientific, algebra actions (with a subject picker for Rearrange). */
#include "ui/calcview_int.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "ui/editor.h"
#include "ui/log.h"
#include "ui/osk_text.h"

#include <stdio.h>
#include <string.h>

static const CalcAction k_fdp[] = { ACT_TO_FRACTION, ACT_TO_DECIMAL, ACT_TO_PERCENT, ACT_TO_MIXED, ACT_STD_FORM,
				    ACT_SURD };
static const char *k_fdp_label[] = { "\xE2\x86\x92 Fraction", "\xE2\x86\x92 Decimal", "\xE2\x86\x92 Percent",
				     "\xE2\x86\x92 Mixed", "Standard form", "Surd" };
static const CalcAction k_alg[] = { ACT_SIMPLIFY, ACT_EXPAND, ACT_FACTOR, ACT_SOLVE, ACT_COMPLETE_SQ, ACT_REARRANGE };
static const char *k_alg_label[] = { "Simplify", "Expand", "Factorise", "Solve", "Complete square", "Rearrange" };

int cv_action_count(int mode)
{
	CalcState *s = &g_cs[mode];
	return s->pick_subject ? s->nsubj : 6;
}

const char *cv_action_label(int mode, int i)
{
	CalcState *s = &g_cs[mode];
	if (s->pick_subject)
		return s->subj[i];
	return mode == MODE_ALG ? k_alg_label[i] : k_fdp_label[i];
}

void cv_action_rect(int i, int *x, int *y, int *w, int *h)
{
	int cw = (CV_PANE_W - CV_ACT_GAP * (CV_ACT_COLS - 1)) / CV_ACT_COLS;
	*x = CV_PANE_X + (i % CV_ACT_COLS) * (cw + CV_ACT_GAP);
	*y = CV_ACT_Y + (i / CV_ACT_COLS) * (CV_ACT_H + CV_ACT_GAP);
	*w = cw;
	*h = CV_ACT_H;
}

/* Rearrange: ask which letter becomes the subject when there is a choice. */
static void rearrange(int mode)
{
	CalcState *s = &g_cs[mode];
	char input[512];
	ed_serialize(&s->ed, input, (int)sizeof input);
	Arena *a = arena_new(16 * 1024), *prev = NULL;
	int n = 0;
	if (a) {
		prev = arena_use(a);
		char err[64];
		int ep;
		Expr *e = ex_parse(input, err, (int)sizeof err, &ep);
		if (e)
			n = ex_vars(e, s->subj, CV_MAX_SUBJ);
		arena_use(prev);
		arena_free(a);
	}
	if (n <= 1) {
		cv_run(mode, ACT_REARRANGE, n == 1 ? s->subj[0] : NULL);
		return;
	}
	s->nsubj = n;
	s->pick_subject = true;
	s->focus = FOC_ACTIONS;
	s->act_sel = 0;
	app_toast("Make which letter the subject?");
}

void cv_do_action(int mode, int i)
{
	CalcState *s = &g_cs[mode];
	s->pressed_act = i;
	s->pressed_until = app_now() + 120;
	if (s->pick_subject) {
		if (i < s->nsubj)
			cv_run(mode, ACT_REARRANGE, s->subj[i]);
		return;
	}
	if (mode == MODE_ALG) {
		if (k_alg[i] == ACT_REARRANGE)
			rearrange(mode);
		else
			cv_run(mode, k_alg[i], NULL);
	} else {
		cv_run(mode, k_fdp[i], NULL);
	}
}


/* ---- feedback when an action has nothing visible to do ---- */

#define EG_SQ "\xC2\xB2"

static const struct {
	CalcAction a;
	const char *verb, *eg;
} k_eg[] = {
	{ ACT_SIMPLIFY, "simplify", "(x" EG_SQ "\xE2\x88\x92" "4)/(x+2)" }, { ACT_EXPAND, "expand", "(x+2)(x+3)" },
	{ ACT_FACTOR, "factorise", "x" EG_SQ "+5x+6" }, { ACT_SOLVE, "solve", "2x+3=11" },
	{ ACT_COMPLETE_SQ, "complete the square of", "x" EG_SQ "+6x+5" }, { ACT_REARRANGE, "rearrange", "y=2x+3" },
};

/* an action pressed with nothing typed and no answer to work on */
void cv_empty_hint(int mode, CalcAction a)
{
	char msg[96];
	const char *verb = "convert", *eg = "0.75";
	for (size_t i = 0; i < sizeof k_eg / sizeof k_eg[0]; i++)
		if (k_eg[i].a == a) {
			verb = k_eg[i].verb;
			eg = k_eg[i].eg;
		}
	if (a == ACT_AUTO)
		return;
	if (mode != MODE_ALG && a == ACT_STD_FORM)
		eg = "123000";
	snprintf(msg, sizeof msg, "Type something to %s first, e.g. %s", verb, eg);
	app_toast(msg);
}

/* a successful answer that is just the input again: say why, and what to press */
void cv_same_hint(int mode, CalcAction a, const Res *r, const char *input)
{
	if (!r->ok || strcmp(r->answer, input) != 0)
		return;
	if (a == ACT_EXPAND)
		app_toast("No brackets to multiply out. Expand works on e.g. (x+2)(x+3)");
	else if (a == ACT_AUTO && mode == MODE_ALG)
		app_toast("Nothing to work out. For brackets pick Expand (d-pad \xE2\x97\x80 from the keypad)");
}

/* A maths answer (no "=", list or words) goes in as itself, so Ans after
 * Expand is x^2+5x+6, not the last number. A number keeps the Ans token. */
void cv_insert_ans(int mode)
{
	CalcState *s = &g_cs[mode];
	const char *ans = s->has_cur && s->cur.ok && !s->cur.has_value ? s->cur.answer : NULL;
	if (!ans || !*ans || strpbrk(ans, "=<>,\"") || strstr(ans, "\xE2\x89") || strstr(ans, "\xC2\xB1")) {
		ed_insert_text(&s->ed, "Ans");
		return;
	}
	char b[520];
	bool wrap = strpbrk(ans + 1, "+-") != NULL;
	snprintf(b, sizeof b, wrap ? "(%s)" : "%s", ans);
	ed_insert_display(&s->ed, b);
}

/* text accepted on the system keyboard (also the tour's "osk" command) */
void cv_osk_accept(int mode, const char *text)
{
	CalcState *s = &g_cs[mode];
	char t[512];
	snprintf(t, sizeof t, "%s", text);
	osk_text_tidy(t);
	log_printf("osk text '%s' -> '%s'", text, t);
	ed_load(&s->ed, t);
	s->fresh = false;
}
