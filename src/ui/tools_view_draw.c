/* tools_view_draw.c - drawing of the GCSE screens: topic + tool lists, and
 * the tool form with its result card. */
#include "ui/tools_view_int.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/mlayout.h"
#include "ui/text.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

void tv_draw_list(void)
{
	/* topics */
	draw_rrect(TV_CAT_X, TV_LIST_Y, TV_CAT_W, TV_LIST_H, 14, COL_BG2);
	font_draw_w(FW_BOLD, TV_CAT_X + 16, TV_LIST_Y + 12, COL_TEXT, 18, "GCSE topics");
	{	/* search affordance (tap or triangle) */
		int sx = TV_CAT_X + TV_CAT_W - 16 - font_width(14, "Search");
		font_draw(sx, TV_LIST_Y + 15, theme_accent(), 14, "Search");
		draw_glyph(GLYPH_TRIANGLE, sx - 4 - GLYPH_SIZE, TV_LIST_Y + 15 + (font_height(14) - GLYPH_SIZE) / 2);
	}
	for (int i = 0; i < g_tv.ncats; i++) {
		int y = TV_CAT_ROW_Y + i * TV_CAT_ROW_H;
		bool sel = i == g_tv.cat && !g_tv.searching;
		if (sel) {
			draw_rrect(TV_CAT_X + 6, y + 2, TV_CAT_W - 12, TV_CAT_ROW_H - 4, 10,
				   g_tv.on_tools ? COL_PANEL2 : theme_accent_soft());
			draw_rrect(TV_CAT_X + 6, y + 10, 4, TV_CAT_ROW_H - 20, 2, theme_accent());
		}
		if (sel && !g_tv.on_tools)
			draw_rrect_outline(TV_CAT_X + 6, y + 2, TV_CAT_W - 12, TV_CAT_ROW_H - 4, 10, 2, theme_accent());
		font_draw_w(sel ? FW_MED : FW_REG, TV_CAT_X + 22, y + (TV_CAT_ROW_H - font_height(17)) / 2,
			    sel ? COL_TEXT : COL_TEXT2, 17, g_tv.cats[i]);
	}

	/* tools in the topic */
	draw_rrect(TV_TOOL_X, TV_LIST_Y, TV_TOOL_W, TV_LIST_H, 14, COL_BG2);
	if (g_tv.ncats == 0) {
		font_draw_c(FW_REG, TV_TOOL_X + TV_TOOL_W / 2, TV_LIST_Y + 100, COL_DIM, 16, "No tools available");
		return;
	}
	char cnt[24];
	snprintf(cnt, sizeof cnt, "%d %s%s", g_tv.ntools, g_tv.searching ? "match" : "tool",
		 g_tv.ntools == 1 ? "" : g_tv.searching ? "es" : "s");
	font_draw_r(FW_REG, TV_TOOL_X + TV_TOOL_W - 18, TV_LIST_Y + 16, COL_DIM, 13, cnt);
	if (g_tv.searching) {
		char head[TV_QUERY_MAX + 16];
		snprintf(head, sizeof head, "Search: %s", g_tv.query);
		char line[TEXT_LINE_MAX];
		text_ellipsize(18, head, TV_TOOL_W - 36 - font_width(13, cnt) - 12, line);
		font_draw_w(FW_BOLD, TV_TOOL_X + 18, TV_LIST_Y + 12, theme_accent(), 18, line);
		if (g_tv.ntools == 0) {
			font_draw_c(FW_REG, TV_TOOL_X + TV_TOOL_W / 2, TV_TOOL_ROW_Y + 30, COL_DIM, 16,
				    "No tools match that word");
			font_draw_c(FW_REG, TV_TOOL_X + TV_TOOL_W / 2, TV_TOOL_ROW_Y + 56, COL_DIM, 14,
				    "Try a shorter word, e.g. area, percent, angle");
		}
	} else {
		font_draw_w(FW_BOLD, TV_TOOL_X + 18, TV_LIST_Y + 12, COL_TEXT, 18, g_tv.cats[g_tv.cat]);
	}
	for (int r = 0; r < TV_TOOL_ROWS; r++) {
		int i = g_tv.tool_scroll + r;
		if (i >= g_tv.ntools)
			break;
		const Tool *t = g_tv.tools[i];
		int y = TV_TOOL_ROW_Y + r * TV_TOOL_ROW_H;
		bool sel = g_tv.on_tools && i == g_tv.tool_sel;
		draw_rrect(TV_TOOL_X + 8, y + 2, TV_TOOL_W - 16, TV_TOOL_ROW_H - 4, 10, sel ? theme_accent_soft() : COL_PANEL);
		if (sel)
			draw_rrect_outline(TV_TOOL_X + 8, y + 2, TV_TOOL_W - 16, TV_TOOL_ROW_H - 4, 10, 2, theme_accent());
		char line[TEXT_LINE_MAX];
		int tag_w = g_tv.searching ? font_width(12, t->category) + 16 : 0;   /* topic of a search hit */
		text_ellipsize(17, t->name, TV_TOOL_W - 70 - tag_w, line);
		font_draw_w(FW_MED, TV_TOOL_X + 22, y + 8, sel ? COL_TEXT : COL_TEXT2, 17, line);
		if (tag_w)
			font_draw_r(FW_REG, TV_TOOL_X + TV_TOOL_W - 44, y + 11, COL_DIM, 12, t->category);
		text_ellipsize(13, t->desc ? t->desc : "", TV_TOOL_W - 70, line);
		font_draw(TV_TOOL_X + 22, y + 32, COL_MUTED, 13, line);
		font_draw_r(FW_REG, TV_TOOL_X + TV_TOOL_W - 24, y + 16, sel ? theme_accent() : COL_DIM, 18, "\xE2\x80\xBA");
	}
	if (g_tv.ntools > TV_TOOL_ROWS) {
		int track = TV_TOOL_ROWS * TV_TOOL_ROW_H;
		int th = track * TV_TOOL_ROWS / g_tv.ntools;
		int ty = TV_TOOL_ROW_Y + (track - th) * g_tv.tool_scroll / (g_tv.ntools - TV_TOOL_ROWS);
		draw_rrect(TV_TOOL_X + TV_TOOL_W - 6, ty, 3, th, 1, COL_LINE);
	}
}

static void draw_field(int i, int y)
{
	const ToolField *f = &g_tv.t->fields[i];
	bool sel = g_tv.field == i;
	char line[TEXT_LINE_MAX];
	text_ellipsize(15, f->label, TF_LABEL_W - 14, line);
	font_draw_w(FW_MED, TF_X + 14, y + 6, sel ? COL_TEXT : COL_TEXT2, 15, line);
	const char *sub = f->optional ? "optional" : (f->kind == TF_LIST ? "list, comma separated" : "");
	if (*sub)
		font_draw(TF_X + 14, y + 26, COL_DIM, 12, sub);
	int bx = TF_X + TF_LABEL_W, bw = TF_W - TF_LABEL_W - 10, bh = TF_ROW_H - 8;
	draw_rrect(bx, y + 3, bw, bh, 10, sel ? COL_PANEL2 : COL_PANEL);
	if (sel)
		draw_rrect_outline(bx, y + 3, bw, bh, 10, 2, theme_accent());
	const char *v = g_tv.vals[i];
	int base = y + 3 + bh / 2 + 6;
	if (f->kind == TF_CHOICE) {
		font_draw_c(FW_MED, bx + bw / 2, y + 3 + (bh - font_height(17)) / 2, COL_TEXT, 17, v);
		font_draw(bx + 12, y + 3 + (bh - font_height(13)) / 2, sel ? COL_TEXT2 : COL_DIM, 13, "\xE2\x97\x80");
		font_draw_r(FW_REG, bx + bw - 12, y + 3 + (bh - font_height(13)) / 2, sel ? COL_TEXT2 : COL_DIM, 13,
			    "\xE2\x96\xB6");
		return;
	}
	if (!*v) {
		text_ellipsize(14, f->hint ? f->hint : "", bw - 24, line);
		font_draw(bx + 12, y + 3 + (bh - font_height(14)) / 2, COL_DIM, 14, line);
		return;
	}
	const MLayout *l = mlc_fit(v, 20, 12, bw - 24);
	base = y + 3 + (bh + l->asc - l->desc) / 2;
	draw_clip(bx + 4, y + 4, bw - 8, bh - 2);
	mdraw(l, bx + 12, base, COL_TEXT);
	draw_clip_off();
}

static void draw_result(int x, int y, int w, int h)
{
	const Res *r = &g_tv.res;
	draw_rrect(x, y, w, h, 14, COL_PANEL);
	if (!g_tv.has_res) {
		font_draw(x + 16, y + 14, COL_DIM, 15, "Answer");
		char lines[3][TEXT_LINE_MAX];
		int n = text_wrap(14, "Fill in the boxes, then press START or Work it out.", w - 32, 3, lines);
		for (int i = 0; i < n; i++)
			font_draw(x + 16, y + 44 + i * 20, COL_DIM, 14, lines[i]);
		return;
	}
	font_draw_w(FW_MED, x + 16, y + 12, r->ok ? COL_MUTED : COL_ERR, 14, r->ok ? "Answer" : "Couldn't work that out");
	if (!r->ok) {
		char lines[4][TEXT_LINE_MAX];
		int n = text_wrap(15, r->error, w - 32, 4, lines);
		for (int i = 0; i < n; i++)
			font_draw(x + 16, y + 40 + i * 21, COL_ERR, 15, lines[i]);
		return;
	}
	const MLayout *l = mlc_fit(r->answer, 30, 14, w - 32);
	int base = y + 44 + l->asc;
	mdraw(l, x + 16, base, COL_TEXT);
	if (r->alt && r->alt[0]) {
		/* The second line is only an approximation when it brings decimals to an
		 * answer that had none (25π cm² -> 78.5 cm²); "Mode=2" or "Change of
		 * 737.62" are extra facts, and a ≈ in front of them is false. */
		bool approx = strchr(r->alt, '.') && !strchr(r->answer, '.');
		int ax = approx ? x + 34 : x + 16;
		const MLayout *a = mlc_fit(r->alt, 18, 12, w - (ax - x) - 16);
		if (approx)
			font_draw(x + 16, base + l->desc + 8, COL_DIM, 16, "\xE2\x89\x88");
		mdraw(a, ax, base + l->desc + 8 + a->asc, COL_MUTED);
		base += a->asc + a->desc + 8;
	}
	int sy = base + l->desc + 16;
	draw_rect(x + 16, sy, w - 32, 1, COL_LINE_SOFT);
	sy += 10;
	if (r->nsteps > 0) {
		draw_glyph(GLYPH_TRIANGLE, x + 16, sy);
		char t[32];
		snprintf(t, sizeof t, "Working (%d step%s)", r->nsteps, r->nsteps == 1 ? "" : "s");
		font_draw_w(FW_MED, x + 16 + GLYPH_SIZE + 8, sy, theme_accent(), 15, t);
		sy += 28;
		int num = 0;   /* same numbering as the steps view: top-level steps */
		for (int i = 0; i < r->nsteps && sy < y + h - 24; i++) {
			if (r->steps[i].depth > 0)
				continue;
			num++;
			if (!r->steps[i].title)
				continue;
			char line[TEXT_LINE_MAX];
			snprintf(line, sizeof line, "%d.", num);
			font_draw(x + 18, sy, COL_DIM, 14, line);
			text_ellipsize(14, r->steps[i].title, w - 64, line);
			font_draw(x + 42, sy, COL_TEXT2, 14, line);
			sy += 22;
		}
	}
}

void tv_draw_form(void)
{
	const Tool *t = g_tv.t;
	/* fields */
	draw_rrect(TF_X, BODY_Y + 8, TF_W, BODY_H - 16, 14, COL_BG2);
	draw_glyph(GLYPH_BACK, TF_X + 14, BODY_Y + 20);
	font_draw(TF_X + 14 + GLYPH_SIZE + 6, BODY_Y + 20, COL_DIM, 14, t->category);
	for (int i = 0; i < t->nfields; i++)
		draw_field(i, TF_ROW_Y + i * TF_ROW_H);
	if (t->nfields == 0)
		font_draw(TF_X + 14, TF_ROW_Y + 10, COL_DIM, 15, "Nothing to fill in.");

	/* side: name, what it does, run button, result */
	int x = TF_SIDE_X, w = TF_SIDE_W;
	font_draw_w(FW_BOLD, x + 4, BODY_Y + 12, COL_TEXT, 21, t->name);
	char lines[2][TEXT_LINE_MAX];
	int n = text_wrap(14, t->desc ? t->desc : "", w - 8, 2, lines);
	for (int i = 0; i < n; i++)
		font_draw(x + 4, BODY_Y + 42 + i * 19, COL_MUTED, 14, lines[i]);
	int by = BODY_Y + 92;
	bool foc = g_tv.field == t->nfields;
	button_draw(x + 4, by, w - 8, 44, "Work it out", 1, foc, false);
	draw_glyph(GLYPH_START, x + 16, by + 13);
	draw_result(x, BODY_Y + 152, w, BODY_H - 160);
}
