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

static void hist_use(int mode, int i, bool answer)
{
	CalcState *s = &g_cs[mode];
	const HEntry *h = history_get(i);
	if (!h)
		return;
	if (answer) {
		if (s->fresh)
			ed_clear(&s->ed);
		ed_insert_display(&s->ed, h->r.answer);
	} else {
		ed_load(&s->ed, h->r.input);
	}
	s->fresh = false;
	s->focus = FOC_EDITOR;
}

static void nav_keypad(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	const Keyset *ks = keyset_get(cv_keyset(mode));
	int c = s->key_sel % ks->cols, r = s->key_sel / ks->cols;
	if (in->pressed & IN_UP) {
		if (r == 0) s->focus = FOC_EDITOR;
		else r--;
	}
	if ((in->pressed & IN_DOWN) && r < ks->rows - 1) r++;
	if (in->pressed & IN_LEFT) {
		if (c == 0) { s->focus = FOC_ACTIONS; s->act_sel = 2; }
		else c--;
	}
	if ((in->pressed & IN_RIGHT) && c < ks->cols - 1) c++;
	s->key_sel = r * ks->cols + c;
	if (s->focus == FOC_KEYPAD && (in->pressed & IN_CONFIRM))
		calc_screen_key(mode, s->key_sel);
}

static void nav_editor(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	if (in->pressed & IN_LEFT) { ed_move(&s->ed, 0); s->fresh = false; }
	if (in->pressed & IN_RIGHT) { ed_move(&s->ed, 1); s->fresh = false; }
	if ((in->pressed & IN_UP) && !ed_move(&s->ed, 2) && history_count() > 0) {
		s->focus = FOC_HISTORY;
		s->hist_sel = history_count() - 1;
	}
	if ((in->pressed & IN_DOWN) && !ed_move(&s->ed, 3))
		s->focus = FOC_KEYPAD;
	if (in->pressed & IN_CONFIRM)
		cv_run(mode, ACT_AUTO, NULL);
}

static void nav_history(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	int n = history_count();
	if (n == 0) { s->focus = FOC_EDITOR; return; }
	if (s->hist_sel < 0 || s->hist_sel >= n) s->hist_sel = n - 1;
	if ((in->pressed & IN_UP) && s->hist_sel > 0) s->hist_sel--;
	if (in->pressed & IN_DOWN) {
		if (s->hist_sel < n - 1) s->hist_sel++;
		else s->focus = FOC_EDITOR;
	}
	if (in->pressed & IN_CONFIRM) hist_use(mode, s->hist_sel, false);
	if (in->pressed & IN_SQUARE) hist_use(mode, s->hist_sel, true);
	if (in->pressed & IN_TRIANGLE) {
		const HEntry *h = history_get(s->hist_sel);
		if (h && h->r.nsteps) app_open_steps(&h->r);
	}
}

static void nav_actions(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	int n = cv_action_count(mode);
	int c = s->act_sel % CV_ACT_COLS, r = s->act_sel / CV_ACT_COLS;
	int rows = (n + CV_ACT_COLS - 1) / CV_ACT_COLS;
	if (in->pressed & IN_LEFT && c > 0) c--;
	if (in->pressed & IN_RIGHT) {
		if (c < CV_ACT_COLS - 1 && r * CV_ACT_COLS + c + 1 < n) c++;
		else s->focus = FOC_KEYPAD;
	}
	if (in->pressed & IN_UP) {
		if (r > 0) r--;
		else s->focus = FOC_EDITOR;
	}
	if ((in->pressed & IN_DOWN) && r < rows - 1 && (r + 1) * CV_ACT_COLS + c < n) r++;
	s->act_sel = r * CV_ACT_COLS + c;
	if (s->focus == FOC_ACTIONS && (in->pressed & IN_CONFIRM))
		cv_do_action(mode, s->act_sel);
	if (s->pick_subject && (in->pressed & IN_BACK)) {
		s->pick_subject = false;
		s->act_sel = 0;
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
		if (tx > CV_PANE_X + CV_PANE_W - 120 && ty < CV_ANS_Y + 34 && s->cur.nsteps)
			app_open_steps(&s->cur);
		else if (s->cur.ok && s->cur.alt[0])
			s->show_alt = !s->show_alt;
		return;
	}
	int h = cv_hist_hit(mode, ty);
	if (h >= 0 && in_rect(tx, ty, CV_PANE_X, CV_HIST_Y, CV_PANE_W, CV_HIST_H)) {
		s->hist_sel = h;
		s->focus = FOC_HISTORY;
		hist_use(mode, h, false);
	}
}

void calc_screen_update(int mode, const input_t *in)
{
	CalcState *s = &g_cs[mode];
	bool was_picking = s->pick_subject;
	touch(mode, in);
	switch (s->focus) {
	case FOC_EDITOR: nav_editor(mode, in); break;
	case FOC_HISTORY: nav_history(mode, in); break;
	case FOC_ACTIONS: nav_actions(mode, in); break;
	default: nav_keypad(mode, in); break;
	}
	if ((in->pressed & IN_BACK) && s->focus != FOC_HISTORY && !was_picking) {
		ed_backspace(&s->ed);
		s->fresh = false;
	}
	if (in->pressed & IN_START)
		cv_run(mode, ACT_AUTO, NULL);
	if ((in->pressed & IN_SQUARE) && s->focus != FOC_HISTORY) {
		if (s->has_cur && s->cur.ok && s->cur.alt[0])
			s->show_alt = !s->show_alt;
	}
	if ((in->pressed & IN_TRIANGLE) && s->focus != FOC_HISTORY && s->has_cur && s->cur.nsteps)
		app_open_steps(&s->cur);
	if (s->pressed_until && app_now() > s->pressed_until) {
		s->pressed_key = -1;
		s->pressed_act = -1;
		s->pressed_until = 0;
	}

	static const Hint hk[] = { { GLYPH_CONFIRM, "Press" }, { GLYPH_BACK, "Delete" },
		{ GLYPH_SQUARE, "S\xE2\x87\x94" "D" }, { GLYPH_TRIANGLE, "Steps" }, { GLYPH_START, "=" },
		{ GLYPH_DPAD, "\xE2\x97\x80 edge: Actions" }, { GLYPH_LR, "Mode" }, { GLYPH_SELECT, "Settings" } };
	static const Hint he[] = { { GLYPH_DPAD, "Move cursor" }, { GLYPH_CONFIRM, "=" }, { GLYPH_BACK, "Delete" },
		{ GLYPH_TRIANGLE, "Steps" }, { GLYPH_LR, "Mode" } };
	static const Hint hh[] = { { GLYPH_DPAD, "Choose" }, { GLYPH_CONFIRM, "Edit this" },
		{ GLYPH_SQUARE, "Insert answer" }, { GLYPH_TRIANGLE, "Steps" } };
	static const Hint ha[] = { { GLYPH_DPAD, "Choose" }, { GLYPH_CONFIRM, "Apply to the input" },
		{ GLYPH_START, "=" }, { GLYPH_LR, "Mode" } };
	switch (s->focus) {
	case FOC_EDITOR: app_hints(he, 5); break;
	case FOC_HISTORY: app_hints(hh, 4); break;
	case FOC_ACTIONS: app_hints(ha, 4); break;
	default: app_hints(hk, 8); break;
	}
}
