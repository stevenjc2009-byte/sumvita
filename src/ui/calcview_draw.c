/* calcview_draw.c - drawing of the Calc / Scientific / Algebra screens:
 * history list, editor box, answer area, action row, keypad. */
#include "ui/calcview_int.h"

#include "ui/draw.h"
#include "ui/editor_draw.h"
#include "ui/font.h"
#include "ui/history.h"
#include "ui/mlayout.h"
#include "ui/settings.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

#define HIST_PAD 7
#define HIST_IN_PX 16
#define HIST_ANS_PX 20
#define HIST_MAXW (CV_PANE_W - 40)

static const char *entry_answer(const HEntry *h)
{
	return h->r.ok ? h->r.answer : h->r.error;
}

int cv_hist_entry_h(int i)
{
	const HEntry *h = history_get(i);
	if (!h)
		return 0;
	const MLayout *a = mlc_fit(h->r.echo, HIST_IN_PX, 12, HIST_MAXW);
	const MLayout *b = h->r.ok ? mlc_fit(entry_answer(h), HIST_ANS_PX, 14, HIST_MAXW)
				   : mlc_text(entry_answer(h), 15, 0);
	return HIST_PAD + a->asc + a->desc + 4 + b->asc + b->desc + HIST_PAD;
}

/* Entries are bottom-anchored: newest just above the editor. */
int cv_hist_hit(int mode, int ty)
{
	int y = CV_HIST_Y + CV_HIST_H + g_cs[mode].hist_scroll;
	for (int i = history_count() - 1; i >= 0; i--) {
		int h = cv_hist_entry_h(i);
		if (ty >= y - h && ty < y && y - h >= CV_HIST_Y - 1 && y <= CV_HIST_Y + CV_HIST_H + 1)
			return i;
		y -= h;
		if (y < CV_HIST_Y)
			break;
	}
	return -1;
}

static void draw_history(int mode)
{
	CalcState *s = &g_cs[mode];
	int n = history_count();
	draw_clip(CV_PANE_X, CV_HIST_Y, CV_PANE_W, CV_HIST_H);
	if (n == 0) {
		font_draw_c(FW_REG, CV_PANE_X + CV_PANE_W / 2, CV_HIST_Y + CV_HIST_H / 2 - 20, COL_DIM, 15,
			    "History appears here");
		font_draw_c(FW_REG, CV_PANE_X + CV_PANE_W / 2, CV_HIST_Y + CV_HIST_H / 2 + 2, COL_DIM, 13,
			    "Up from the input to scroll back and reuse a sum");
		draw_clip_off();
		return;
	}
	/* keep the selected entry visible */
	if (s->focus == FOC_HISTORY && s->hist_sel >= 0) {
		int below = 0;
		for (int i = n - 1; i > s->hist_sel; i--)
			below += cv_hist_entry_h(i);
		int hh = cv_hist_entry_h(s->hist_sel);
		if (below < s->hist_scroll)
			s->hist_scroll = below;
		if (below + hh > s->hist_scroll + CV_HIST_H)
			s->hist_scroll = below + hh - CV_HIST_H;
	}
	int total = 0;
	for (int i = 0; i < n; i++)
		total += cv_hist_entry_h(i);
	int maxs = total - CV_HIST_H;
	if (s->hist_scroll > maxs)
		s->hist_scroll = maxs > 0 ? maxs : 0;

	int y = CV_HIST_Y + CV_HIST_H + s->hist_scroll;
	for (int i = n - 1; i >= 0 && y > CV_HIST_Y; i--) {
		const HEntry *h = history_get(i);
		int eh = cv_hist_entry_h(i);
		int top = y - eh;
		/* An entry cut by the top or bottom edge is left out (the scroll bar
		 * shows there is more): half a row of maths reads as garbage. */
		if (top >= CV_HIST_Y - 1 && y <= CV_HIST_Y + CV_HIST_H + 1) {
			bool sel = s->focus == FOC_HISTORY && i == s->hist_sel;
			if (sel)
				draw_rrect(CV_PANE_X + 2, top + 2, CV_PANE_W - 4, eh - 4, 10, theme_accent_soft());
			const MLayout *a = mlc_fit(h->r.echo, HIST_IN_PX, 12, HIST_MAXW);
			int by = top + HIST_PAD + a->asc;
			mdraw(a, CV_PANE_X + 14, by, COL_MUTED);
			if (h->r.label && h->r.label[0])
				font_draw_r(FW_MED, CV_PANE_X + CV_PANE_W - 14, by - a->asc, COL_DIM, 12, h->r.label);
			if (h->r.ok) {
				const MLayout *b = mlc_fit(h->r.answer, HIST_ANS_PX, 14, HIST_MAXW);
				int ay = by + a->desc + 4 + b->asc;
				mdraw(b, CV_PANE_X + CV_PANE_W - 14 - b->w, ay, sel ? theme_accent() : COL_TEXT);
			} else {
				font_draw_r(FW_REG, CV_PANE_X + CV_PANE_W - 14, by + a->desc + 4, COL_ERR, 15, h->r.error);
			}
			if (i > 0)
				draw_rect(CV_PANE_X + 14, top, CV_PANE_W - 28, 1, COL_LINE_SOFT);
		}
		y = top;
	}
	draw_clip_off();
	if (total > CV_HIST_H) {
		/* thin scroll indicator */
		int th = CV_HIST_H * CV_HIST_H / total;
		if (th < 16) th = 16;
		int ty = CV_HIST_Y + (CV_HIST_H - th) - (CV_HIST_H - th) * s->hist_scroll / (maxs > 0 ? maxs : 1);
		draw_rrect(CV_PANE_X + CV_PANE_W - 4, ty, 3, th, 1, COL_LINE);
	}
}

static void draw_editor(int mode)
{
	CalcState *s = &g_cs[mode];
	bool foc = s->focus == FOC_EDITOR;
	int x = CV_PANE_X, y = CV_ED_Y, w = CV_PANE_W, h = CV_ED_H;
	draw_rrect(x, y, w, h, 14, COL_PANEL);
	draw_rrect_outline(x, y, w, h, 14, foc ? 2 : 1, foc ? theme_accent() : COL_LINE);
	int px = 26;
	const MLayout *l = ed_layout(&s->ed, px);
	while ((l->asc + l->desc > h - 14) && px > 16) {
		px = ml_quant(px - 4);
		l = ed_layout(&s->ed, px);
	}
	int inner = w - 32;
	int cx = ed_caret_x(&s->ed, l);
	if (cx - s->ed_scroll > inner - 10)
		s->ed_scroll = cx - inner + 10;
	if (cx - s->ed_scroll < 10)
		s->ed_scroll = cx - 10;
	if (l->w <= inner || s->ed_scroll < 0)
		s->ed_scroll = 0;
	int base = y + (h + l->asc - l->desc) / 2;
	bool blink = ((app_now() / 530) & 1) == 0 || foc;
	if (s->fresh)
		blink = false;
	draw_clip(x + 6, y + 3, w - 12, h - 6);
	ed_draw(&s->ed, l, x + 16 - s->ed_scroll, base, s->fresh ? COL_TEXT2 : COL_TEXT, blink);
	draw_clip_off();
	if (ed_is_empty(&s->ed))
		font_draw(x + 24, base - ml_text_asc(18) - 2, COL_DIM, 18,
			  mode == MODE_ALG ? "Type an expression or equation" : "Type a calculation");
}

static void draw_answer(int mode)
{
	CalcState *s = &g_cs[mode];
	int x = CV_PANE_X, y = CV_ANS_Y, w = CV_PANE_W, h = CV_ANS_H;
	draw_rrect(x, y, w, h, 14, COL_BG2);
	if (!s->has_cur) {
		font_draw(x + 16, y + 14, COL_DIM, 15, "Answer");
		const char *tip = mode == MODE_ALG ? "Pick an action below, or press START to solve / simplify"
						   : "Press START or = to work it out";
		font_draw(x + 16, y + h - 30, COL_DIM, 14, tip);
		return;
	}
	Res *r = &s->cur;
	const char *lab = r->label && r->label[0] ? r->label : "Answer";
	font_draw_w(FW_MED, x + 16, y + 10, COL_MUTED, 14, lab);
	if (s->show_alt)
		font_draw_w(FW_MED, x + 16 + font_width_w(FW_MED, 14, lab) + 10, y + 10, theme_accent(), 13,
			    "S\xE2\x87\x94" "D");
	if (r->nsteps) {
		int bw = 104;
		int bx = x + w - bw - 10;
		draw_rrect(bx, y + 8, bw, 26, 13, COL_PANEL3);
		int gx = bx + 10;
		draw_glyph(GLYPH_TRIANGLE, gx, y + 12);
		font_draw_w(FW_MED, gx + GLYPH_SIZE + 6, y + 13, COL_TEXT2, 14, "Steps");
		char nb[8];
		snprintf(nb, sizeof nb, "%d", r->nsteps);
		font_draw_r(FW_REG, bx + bw - 10, y + 13, COL_DIM, 13, nb);
	}
	if (!r->ok) {
		font_draw_w(FW_MED, x + 16, y + 44, COL_ERR, 18, r->error);
		return;
	}
	const char *txt = s->show_alt && r->alt[0] ? r->alt : r->answer;
	/* fit the width, then the height under the label row (a fraction at 32 px
	 * is taller than the box and used to run into its bottom edge) */
	const MLayout *l = NULL;
	for (int apx = 34;; apx = ml_quant(apx - 1)) {
		l = mlc_fit(txt, apx, 16, w - 70);
		if (l->asc + l->desc <= h - 46 || apx <= 16)
			break;
	}
	int base = y + 40 + (h - 40 + l->asc - l->desc) / 2;
	if (base - l->asc < y + 34)
		base = y + 34 + l->asc;
	int ax = x + w - 18 - l->w;
	mdraw(l, ax, base, COL_TEXT);
	const char *eq = r->has_value && s->show_alt ? "\xE2\x89\x88" : "=";
	if (!strchr(txt, '=') && !strchr(txt, '<') && !strchr(txt, '>'))
		font_draw_base(FW_REG, ax - 14 - font_width(28, eq), base, COL_DIM, 28, eq);
	if (r->alt[0] && !s->show_alt) {
		draw_glyph(GLYPH_SQUARE, x + 16, y + h - 26);
		font_draw(x + 16 + GLYPH_SIZE + 6, y + h - 26, COL_MUTED, 14, "other form");
	}
}

static void draw_actions(int mode)
{
	CalcState *s = &g_cs[mode];
	int n = cv_action_count(mode);
	if (s->pick_subject)
		font_draw(CV_PANE_X + 4, CV_ACT_Y - 22, COL_MUTED, 13, "Make which letter the subject?");
	for (int i = 0; i < n; i++) {
		int x, y, w, h;
		cv_action_rect(i, &x, &y, &w, &h);
		button_draw(x, y, w, h, cv_action_label(mode, i), s->pick_subject ? 1 : 2,
			    s->focus == FOC_ACTIONS && s->act_sel == i, s->pressed_act == i);
	}
}

void calc_screen_draw(int mode)
{
	CalcState *s = &g_cs[mode];
	draw_rect(0, BODY_Y, SCR_W, BODY_H, COL_BG);
	draw_rect(CV_KP_X - 8, BODY_Y, SCR_W - CV_KP_X + 8, BODY_H, COL_BG2);
	draw_rect(CV_KP_X - 8, BODY_Y, 1, BODY_H, COL_LINE_SOFT);
	draw_history(mode);
	draw_editor(mode);
	draw_answer(mode);
	draw_actions(mode);
	const Keyset *ks = keyset_get(cv_keyset(mode));
	keypad_draw(ks, CV_KP_X, CV_KP_Y, CV_KP_W, CV_KP_H, s->focus == FOC_KEYPAD ? s->key_sel : -1, s->pressed_key,
		    g_set.degrees);
}
