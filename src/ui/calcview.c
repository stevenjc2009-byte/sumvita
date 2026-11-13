/* calcview.c - Calc / Scientific / Algebra screens: input handling, focus
 * model (keypad <-> editor <-> history, keypad <-> action row), evaluation. */
#include "ui/calcview_int.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "ui/draw.h"
#include "ui/editor_draw.h"
#include "ui/history.h"
#include "ui/log.h"
#include "ui/osk.h"
#include "ui/settings.h"

#include <stdio.h>
#include <string.h>

CalcState g_cs[3];

/* first-use labels, once per session */
static bool g_tip_mixed, g_tip_frac, g_tip_alg;

void calc_screen_init(void)
{
	for (int m = 0; m < 3; m++) {
		CalcState *s = &g_cs[m];
		memset(s, 0, sizeof *s);
		ed_init(&s->ed);
		res_init(&s->cur);
		s->key_sel = keyset_get(cv_keyset(m))->cols * 4; /* a digit row */
		s->pressed_key = -1;
		s->pressed_act = -1;
		s->hist_sel = -1;
	}
}

void cv_run(int mode, CalcAction a, const char *subject)
{
	CalcState *s = &g_cs[mode];
	char input[512];
	ed_serialize(&s->ed, input, (int)sizeof input);
	if (!input[0] && s->has_cur && s->cur.ok && a != ACT_AUTO)
		snprintf(input, sizeof input, "%s", s->cur.answer);
	if (!input[0]) {
		/* nothing to work out: an error left from the deleted input no longer
		 * describes anything on screen */
		if (s->has_cur && !s->cur.ok) {
			res_free(&s->cur);
			s->has_cur = false;
		}
		cv_empty_hint(mode, a);
		return;
	}
	CalcSettings cs;
	settings_calc(&cs);
	if (subject)
		snprintf(cs.target_var, sizeof cs.target_var, "%s", subject);
	res_run(&s->cur, a, input, &cs);
	cv_same_hint(mode, a, &s->cur, input);
	s->has_cur = true;
	s->show_alt = false;
	s->fresh = true;
	s->pick_subject = false;
	s->err_edit = !s->cur.ok;
	s->err_ver = s->ed.version;
	if (s->cur.ok) {
		if (s->cur.has_value)
			settings_set_ans(s->cur.value);
		Res copy;
		res_clone(&copy, &s->cur);
		history_add(&copy, mode);
		s->hist_scroll = 0;
	}
}

void calc_screen_eval(int mode, int action) { cv_run(mode, (CalcAction)action, NULL); }

void calc_screen_set_input(int mode, const char *display)
{
	CalcState *s = &g_cs[mode];
	ed_load(&s->ed, display);
	s->fresh = false;
}

static bool starts_new(const Key *k)
{
	if (k->act == KA_TEXT) {
		unsigned char c = (unsigned char)k->ins[0];
		return (c >= '0' && c <= '9') || c == '.' || ((c | 32) >= 'a' && (c | 32) <= 'z') || c >= 0x80;
	}
	return k->act == KA_TMPL || k->act == KA_FUNC || k->act == KA_FUNC2;
}

static bool is_operator_key(const Key *k)
{
	if (k->act == KA_POW || k->act == KA_TEN)
		return true;
	if (k->act != KA_TEXT)
		return false;
	static const char *ops[] = { "+", "\xE2\x88\x92", "\xC3\x97", "\xC3\xB7", "%", "!", "\xC2\xB0", NULL };
	for (int i = 0; ops[i]; i++)
		if (!strcmp(k->ins, ops[i]))
			return true;
	return false;
}

static void open_osk(int mode)
{
	CalcState *s = &g_cs[mode];
	char cur[512], out[512];
	ed_serialize(&s->ed, cur, (int)sizeof cur);
	if (osk_prompt("Type maths (e.g. 2x^2+3x-5=0)", cur, OSK_TEXT, out, sizeof out) == 1)
		cv_osk_accept(mode, out);
}

void calc_screen_key(int mode, int idx)
{
	CalcState *s = &g_cs[mode];
	const Keyset *ks = keyset_get(cv_keyset(mode));
	if (idx < 0 || idx >= ks->cols * ks->rows)
		return;
	const Key *k = &ks->k[idx];
	s->pressed_key = idx;
	s->pressed_until = app_now() + 120;
	if (s->fresh && k->act != KA_CMD) {
		bool op = is_operator_key(k);
		if (op || starts_new(k)) {
			ed_clear(&s->ed);
			if (op && s->has_cur && s->cur.ok)
				cv_insert_ans(mode);
		}
		s->fresh = false;
	}
	if (k->act == KA_TEXT && !strcmp(k->ins, "Ans")) {
		cv_insert_ans(mode);
		return;
	}
	if (k->act == KA_TMPL && k->arg == EK_MIXED && !g_tip_mixed) {
		g_tip_mixed = true;
		app_toast("Mixed number: 2 1/3");
	} else if (k->act == KA_TMPL && k->arg == EK_FRAC && !g_tip_frac) {
		g_tip_frac = true;
		app_toast("Fraction: top over bottom");
	}
	int cmd = key_apply(k, &s->ed);
	if (k->act == KA_CMD && cmd == KC_NONE)
		s->fresh = false;
	switch (cmd) {
	case KC_AC:
		ed_clear(&s->ed);
		res_free(&s->cur);
		s->has_cur = false;
		s->fresh = false;
		s->pick_subject = false;
		break;
	case KC_EQ: cv_run(mode, ACT_AUTO, NULL); break;
	case KC_SD:
		if (s->has_cur && s->cur.ok && s->cur.alt[0])
			s->show_alt = !s->show_alt;
		else if (s->has_cur)
			app_toast("No other form for this answer");
		break;
	case KC_OSK: open_osk(mode); break;
	case KC_DEGRAD:
		g_set.degrees = !g_set.degrees;
		settings_save();
		app_toast(g_set.degrees ? "Angles in degrees" : "Angles in radians");
		break;
	default: break;
	}
}

static void touch(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	if (in->dragging && in_rect(in->began_x, in->began_y, CV_PANE_X, CV_HIST_Y, CV_PANE_W, CV_HIST_H)) {
		s->hist_scroll += in->drag_dy;
		if (s->hist_scroll < 0) s->hist_scroll = 0;
	}
	if (!in->tap)
		return;
	int tx = in->tap_x, ty = in->tap_y;
	const Keyset *ks = keyset_get(cv_keyset(mode));
	if (in_rect(tx, ty, CV_KP_X - 3, CV_KP_Y - 3, CV_KP_W + 6, CV_KP_H + 6)) {
		int k = keypad_hit(ks, CV_KP_X, CV_KP_Y, CV_KP_W, CV_KP_H, tx, ty);
		if (k >= 0) {
			s->key_sel = k;
			s->focus = FOC_KEYPAD;
			calc_screen_key(mode, k);
		}
		return;
	}
	for (int i = 0; i < cv_action_count(mode); i++) {
		int x, y, w, h;
		cv_action_rect(i, &x, &y, &w, &h);
		if (in_rect(tx, ty, x, y, w, h)) {
			s->act_sel = i;
			cv_do_action(mode, i);
			return;
		}
	}
	if (in_rect(tx, ty, CV_PANE_X, CV_ED_Y, CV_PANE_W, CV_ED_H)) {
		const MLayout *l = ed_layout(&s->ed, 26);
		int bx = CV_PANE_X + 16 - s->ed_scroll;
		int base = CV_ED_Y + (CV_ED_H + l->asc - l->desc) / 2;
		ed_hit(&s->ed, l, tx - bx, ty - base);
		s->focus = FOC_EDITOR;
		s->fresh = false;
		return;
	}
	if (in_rect(tx, ty, CV_PANE_X, CV_ANS_Y, CV_PANE_W, CV_ANS_H) && s->has_cur) {
		/* the Steps pill, with 8 px of slack on every side for a finger */
		if (s->cur.nsteps && in_rect(tx, ty, CV_PILL_X - 8, CV_PILL_Y - 8, CV_PILL_W + 16, CV_PILL_H + 16))
			app_open_steps(&s->cur);
		else if (s->cur.ok && s->cur.alt[0])
			s->show_alt = !s->show_alt;
		return;
	}
	int h = cv_hist_hit(mode, ty);
	if (h >= 0 && in_rect(tx, ty, CV_PANE_X, CV_HIST_Y, CV_PANE_W, CV_HIST_H)) {
		s->hist_sel = h;
		s->focus = FOC_HISTORY;
		cv_hist_use(mode, h, false);
	}
}

void calc_screen_update(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	bool was_picking = s->pick_subject;
	if (mode == MODE_ALG && !g_tip_alg) {
		g_tip_alg = true;
		app_toast("Tip: press \xE2\x97\x80 at the keypad's left edge for Solve, Factorise, Expand...");
	}
	touch(mode, in);
	cv_nav(mode, in);
	if ((in->pressed & IN_BACK) && s->focus != FOC_HISTORY && !was_picking) {
		if (!cv_edit_error(mode)) {
			ed_backspace(&s->ed);
			s->fresh = false;
		}
	}
	if (in->pressed & IN_START)
		cv_run(mode, ACT_AUTO, NULL);
	if ((in->pressed & IN_SQUARE) && s->focus != FOC_HISTORY) {
		if (s->has_cur && s->cur.ok && s->cur.alt[0])
			s->show_alt = !s->show_alt;
	}
	if ((in->pressed & IN_TRIANGLE) && s->focus != FOC_HISTORY) {
		if (s->has_cur && s->cur.nsteps)
			app_open_steps(&s->cur);
		else
			app_toast("No working for this answer");
	}
	if (s->pressed_until && app_now() > s->pressed_until) {
		s->pressed_key = -1;
		s->pressed_act = -1;
		s->pressed_until = 0;
	}
	cv_hints(mode);
}
