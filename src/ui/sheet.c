/* sheet.c - a modal maths entry sheet (structured editor + the Entry keypad).
 * Used by the GCSE forms and the graph function list; calls done(text, ctx)
 * with the display syntax on OK, nothing on cancel. */
#include "ui/app.h"

#include "ui/draw.h"
#include "ui/editor_draw.h"
#include "ui/font.h"
#include "ui/keypad.h"
#include "ui/osk.h"
#include "ui/settings.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

#define SH_X 20
#define SH_Y (BODY_Y + 6)
#define SH_W (SCR_W - 40)
#define SH_H (BODY_H - 12)
#define SH_ED_X (SH_X + 18)
#define SH_ED_Y (SH_Y + 62)
#define SH_ED_W (SH_W - 36)
#define SH_ED_H 84
#define SH_KP_X (SH_X + 18)
#define SH_KP_Y (SH_ED_Y + SH_ED_H + 12)
#define SH_KP_W (SH_W - 36)
#define SH_KP_H (SH_Y + SH_H - SH_KP_Y - 14)

static Editor g_ed;
static char g_title[64], g_hint[96];
static sheet_done_fn g_done;
static void *g_ctx;
static bool g_on_editor;
static int g_sel;
static int g_pressed = -1;
static uint64_t g_pressed_until;
static int g_scroll;

void sheet_open(const char *title, const char *hint, const char *initial, sheet_done_fn done, void *ctx)
{
	snprintf(g_title, sizeof g_title, "%s", title ? title : "");
	snprintf(g_hint, sizeof g_hint, "%s", hint ? hint : "");
	g_done = done;
	g_ctx = ctx;
	ed_init(&g_ed);
	if (initial && initial[0])
		ed_load(&g_ed, initial);
	g_on_editor = false;
	g_sel = 4 * 8 + 0;   /* the "7" key */
	g_scroll = 0;
}

static void finish(void)
{
	char out[512];
	ed_serialize(&g_ed, out, (int)sizeof out);
	if (g_done)
		g_done(out, g_ctx);
}

/* returns false when the sheet closed */
static bool press(int idx)
{
	const Keyset *ks = keyset_get(KS_ENTRY);
	if (idx < 0 || idx >= ks->cols * ks->rows)
		return true;
	g_pressed = idx;
	g_pressed_until = app_now() + 120;
	switch (key_apply(&ks->k[idx], &g_ed)) {
	case KC_OK:
	case KC_EQ:
		finish();
		return false;
	case KC_AC: ed_clear(&g_ed); break;
	case KC_OSK: {
		char cur[512], out[512];
		ed_serialize(&g_ed, cur, (int)sizeof cur);
		if (osk_prompt(g_title, cur, OSK_TEXT, out, sizeof out) == 1)
			ed_load(&g_ed, out);
		break;
	}
	case KC_DEGRAD:
		g_set.degrees = !g_set.degrees;
		settings_save();
		break;
	default: break;
	}
	return true;
}

bool sheet_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_CONFIRM, "Press key" },
		{ GLYPH_BACK, "Delete" },
		{ GLYPH_START, "OK" },
		{ GLYPH_SELECT, "Cancel" },
		{ GLYPH_TRIANGLE, "Keyboard" },
	};
	app_hints(hints, 5);
	const Keyset *ks = keyset_get(KS_ENTRY);
	if (app_now() > g_pressed_until)
		g_pressed = -1;
	if (in->pressed & IN_SELECT)
		return false;
	if (in->pressed & IN_START) {
		finish();
		return false;
	}
	if (in->pressed & IN_BACK)
		ed_backspace(&g_ed);
	if (in->pressed & IN_TRIANGLE) {
		for (int i = 0; i < ks->cols * ks->rows; i++)
			if (ks->k[i].act == KA_CMD && ks->k[i].arg == KC_OSK)
				return press(i);
	}
	if (in->pressed & IN_L)
		ed_move(&g_ed, 0);
	if (in->pressed & IN_R)
		ed_move(&g_ed, 1);
	if (g_on_editor) {
		if (in->pressed & IN_LEFT) ed_move(&g_ed, 0);
		if (in->pressed & IN_RIGHT) ed_move(&g_ed, 1);
		if (in->pressed & IN_UP) ed_move(&g_ed, 2);
		if ((in->pressed & IN_DOWN) && !ed_move(&g_ed, 3))
			g_on_editor = false;
		if (in->pressed & IN_CONFIRM) {
			finish();
			return false;
		}
	} else {
		int c = g_sel % ks->cols, r = g_sel / ks->cols;
		if (in->pressed & IN_UP) {
			if (r == 0) g_on_editor = true;
			else r--;
		}
		if ((in->pressed & IN_DOWN) && r < ks->rows - 1) r++;
		if ((in->pressed & IN_LEFT) && c > 0) c--;
		if ((in->pressed & IN_RIGHT) && c < ks->cols - 1) c++;
		g_sel = r * ks->cols + c;
		if (!g_on_editor && (in->pressed & IN_CONFIRM) && !press(g_sel))
			return false;
	}
	if (in->tap) {
		if (!in_rect(in->tap_x, in->tap_y, SH_X, SH_Y, SH_W, SH_H))
			return false;
		int k = keypad_hit(ks, SH_KP_X, SH_KP_Y, SH_KP_W, SH_KP_H, in->tap_x, in->tap_y);
		if (k >= 0) {
			g_sel = k;
			g_on_editor = false;
			return press(k);
		}
		if (in_rect(in->tap_x, in->tap_y, SH_ED_X, SH_ED_Y, SH_ED_W, SH_ED_H)) {
			const MLayout *l = ed_layout(&g_ed, 26);
			int base = SH_ED_Y + (SH_ED_H + l->asc - l->desc) / 2;
			ed_hit(&g_ed, l, in->tap_x - (SH_ED_X + 16 - g_scroll), in->tap_y - base);
			g_on_editor = true;
		}
	}
	return true;
}

void sheet_draw(void)
{
	draw_rect(0, 0, SCR_W, SCR_H - HINT_H, COL_SCRIM);
	draw_shadow(SH_X, SH_Y, SH_W, SH_H, 18);
	draw_rrect(SH_X, SH_Y, SH_W, SH_H, 18, COL_BG2);
	draw_rrect_outline(SH_X, SH_Y, SH_W, SH_H, 18, 1, COL_LINE);
	font_draw_w(FW_BOLD, SH_X + 20, SH_Y + 12, COL_TEXT, 21, g_title);
	if (g_hint[0])
		font_draw(SH_X + 20, SH_Y + 38, COL_MUTED, 14, g_hint);

	draw_rrect(SH_ED_X, SH_ED_Y, SH_ED_W, SH_ED_H, 14, COL_PANEL);
	draw_rrect_outline(SH_ED_X, SH_ED_Y, SH_ED_W, SH_ED_H, 14, g_on_editor ? 2 : 1,
			   g_on_editor ? theme_accent() : COL_LINE);
	int px = 26;
	const MLayout *l = ed_layout(&g_ed, px);
	while (l->asc + l->desc > SH_ED_H - 12 && px > 16) {
		px = ml_quant(px - 4);
		l = ed_layout(&g_ed, px);
	}
	int inner = SH_ED_W - 32;
	int cx = ed_caret_x(&g_ed, l);
	if (cx - g_scroll > inner - 10)
		g_scroll = cx - inner + 10;
	if (cx - g_scroll < 10)
		g_scroll = cx - 10;
	if (l->w <= inner || g_scroll < 0)
		g_scroll = 0;
	int base = SH_ED_Y + (SH_ED_H + l->asc - l->desc) / 2;
	draw_clip(SH_ED_X + 6, SH_ED_Y + 3, SH_ED_W - 12, SH_ED_H - 6);
	ed_draw(&g_ed, l, SH_ED_X + 16 - g_scroll, base, COL_TEXT, ((app_now() / 530) & 1) == 0 || g_on_editor);
	draw_clip_off();
	if (ed_is_empty(&g_ed))
		font_draw(SH_ED_X + 24, base - ml_text_asc(18) - 2, COL_DIM, 18, "Empty");

	keypad_draw(keyset_get(KS_ENTRY), SH_KP_X, SH_KP_Y, SH_KP_W, SH_KP_H, g_on_editor ? -1 : g_sel, g_pressed,
		    g_set.degrees);
}
