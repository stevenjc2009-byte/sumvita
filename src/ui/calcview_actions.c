/* calcview_actions.c - the action row under the answer: FDP conversions in
 * Calc / Scientific, algebra actions (with a subject picker for Rearrange). */
#include "ui/calcview_int.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "ui/editor.h"

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

