/* calcview_nav.c - d-pad navigation between the keypad, editor, history and
 * action row of the Calc / Scientific / Algebra screens, plus the hint bar. */
#include "ui/calcview_int.h"

#include "ui/draw.h"
#include "ui/history.h"

#include <string.h>

void cv_hist_use(int mode, int i, bool answer)
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
	if (in->pressed & IN_DOWN) {
		if (r < ks->rows - 1) {
			r++;
		} else {   /* past the bottom row: the action row (also reached by ◀ at the left edge) */
			s->focus = FOC_ACTIONS;
			s->act_sel = 0;
		}
	}
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
	if (in->pressed & IN_CONFIRM) cv_hist_use(mode, s->hist_sel, false);
	if (in->pressed & IN_SQUARE) cv_hist_use(mode, s->hist_sel, true);
	if (in->pressed & IN_TRIANGLE) {
		const HEntry *h = history_get(s->hist_sel);
		if (h && h->r.nsteps) app_open_steps(&h->r);
		else app_toast("No working for this answer");
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

void cv_nav(int mode, const input_t *in)
{
	switch (g_cs[mode].focus) {
	case FOC_EDITOR: nav_editor(mode, in); break;
	case FOC_HISTORY: nav_history(mode, in); break;
	case FOC_ACTIONS: nav_actions(mode, in); break;
	default: nav_keypad(mode, in); break;
	}
}

/* The editor tree has no character offsets, so only a flat input (digits,
 * letters and operators, no brackets or fractions) can place the cursor at the
 * engine's error position; anything structured keeps the cursor at the end. */
static void put_cursor_at(Editor *ed, int pos)
{
	const EdSeq *q = &ed->pool[ed->root];
	int at = 0;
	for (int i = 0; i < q->n; i++) {
		if (q->it[i].kind != EK_TEXT)
			return;
		int len = !strcmp(q->it[i].text, "\xE2\x88\x92") ? 1 : (int)strlen(q->it[i].text);
		if (at + len > pos) {
			ed_set_cursor(ed, ed->root, i);
			return;
		}
		at += len;
	}
}

bool cv_err_edit_ready(int mode)
{
	const CalcState *s = &g_cs[mode];
	return s->err_edit && s->has_cur && !s->cur.ok && s->ed.version == s->err_ver;
}

/* BACK on an engine error: go back to the input, cursor on the bad spot */
bool cv_edit_error(int mode)
{
	CalcState *s = &g_cs[mode];
	if (!cv_err_edit_ready(mode))
		return false;
	s->err_edit = false;
	s->focus = FOC_EDITOR;
	s->fresh = false;
	if (s->cur.error_pos >= 0)
		put_cursor_at(&s->ed, s->cur.error_pos);
	return true;
}

void cv_hints(int mode)
{
	const CalcState *s = &g_cs[mode];
	const Keyset *ks = keyset_get(cv_keyset(mode));
	bool steps = s->has_cur && s->cur.nsteps;
	Hint h[10];
	int n = 0;
	switch (s->focus) {
	case FOC_EDITOR:
		h[n++] = (Hint){ GLYPH_DPAD, "Move cursor" };
		h[n++] = (Hint){ GLYPH_CONFIRM, "=" };
		h[n++] = (Hint){ GLYPH_BACK, "Delete" };
		if (steps) h[n++] = (Hint){ GLYPH_TRIANGLE, "Steps" };
		h[n++] = (Hint){ GLYPH_LR, "Mode" };
		break;
	case FOC_HISTORY: {
		const HEntry *e = history_get(s->hist_sel);
		h[n++] = (Hint){ GLYPH_DPAD, "Choose" };
		h[n++] = (Hint){ GLYPH_CONFIRM, "Edit this" };
		h[n++] = (Hint){ GLYPH_SQUARE, "Insert answer" };
		if (e && e->r.nsteps) h[n++] = (Hint){ GLYPH_TRIANGLE, "Steps" };
		break;
	}
	case FOC_ACTIONS:
		h[n++] = (Hint){ GLYPH_DPAD, "Choose" };
		h[n++] = (Hint){ GLYPH_CONFIRM, "Apply to the input" };
		h[n++] = (Hint){ GLYPH_START, "=" };
		h[n++] = (Hint){ GLYPH_LR, "Mode" };
		break;
	default: {
		bool left = s->key_sel % ks->cols == 0, bottom = s->key_sel / ks->cols == ks->rows - 1;
		h[n++] = (Hint){ GLYPH_CONFIRM, "Press" };
		h[n++] = (Hint){ GLYPH_BACK, "Delete" };
		h[n++] = (Hint){ GLYPH_SQUARE, "Exact/Decimal" };
		if (steps) h[n++] = (Hint){ GLYPH_TRIANGLE, "Steps" };
		h[n++] = (Hint){ GLYPH_START, "=" };
		if (left && bottom) h[n++] = (Hint){ GLYPH_DPAD, "\xE2\x97\x80 \xE2\x96\xBC Actions" };
		else if (left) h[n++] = (Hint){ GLYPH_DPAD, "\xE2\x97\x80 Actions" };
		else if (bottom) h[n++] = (Hint){ GLYPH_DPAD, "\xE2\x96\xBC Actions" };
		h[n++] = (Hint){ GLYPH_LR, "Mode" };
		h[n++] = (Hint){ GLYPH_SELECT, "Settings" };
		break;
	}
	}
	app_hints(h, n);
}
