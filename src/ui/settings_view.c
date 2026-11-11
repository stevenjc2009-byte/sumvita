/* settings_view.c - Settings overlay: angle unit, decimal places, significant
 * figures, exact-vs-decimal, accent colour, clear history, About. Changes save
 * at once to ux0:data/Sumvita/settings.txt. */
#include "ui/app.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/history.h"
#include "ui/settings.h"
#include "ui/theme.h"

#include <stdio.h>

#define ST_W 680
#define ST_X ((SCR_W - ST_W) / 2)
#define ST_Y (BODY_Y + 10)
#define ST_H (BODY_H - 20)
#define ST_ROW_Y (ST_Y + 64)
#define ST_ROW_H 52

enum { R_ANGLE = 0, R_DP, R_SF, R_EXACT, R_ACCENT, R_CLEAR, R_ABOUT, R_COUNT };

static int g_row;
static bool g_confirm_clear;

static const char *k_labels[R_COUNT] = {
	"Angle unit", "Decimal places", "Significant figures", "Answers", "Accent colour", "Clear history",
	"About Sumvita",
};

void settings_view_open(void)
{
	g_row = 0;
	g_confirm_clear = false;
}

static void value_text(int row, char *buf, int cap)
{
	switch (row) {
	case R_ANGLE: snprintf(buf, (size_t)cap, "%s", g_set.degrees ? "Degrees" : "Radians"); break;
	case R_DP:
		if (g_set.dp < 0) snprintf(buf, (size_t)cap, "Auto");
		else snprintf(buf, (size_t)cap, "%d", g_set.dp);
		break;
	case R_SF:
		if (g_set.sf <= 0) snprintf(buf, (size_t)cap, "Off");
		else snprintf(buf, (size_t)cap, "%d", g_set.sf);
		break;
	case R_EXACT: snprintf(buf, (size_t)cap, "%s", g_set.prefer_exact ? "Exact first" : "Decimal first"); break;
	case R_ACCENT: snprintf(buf, (size_t)cap, "%s", theme_accent_name(g_set.accent)); break;
	default: buf[0] = 0; break;
	}
}

static void change(int row, int d)
{
	switch (row) {
	case R_ANGLE: g_set.degrees = !g_set.degrees; break;
	case R_DP:
		g_set.dp += d;
		if (g_set.dp < -1) g_set.dp = 9;
		if (g_set.dp > 9) g_set.dp = -1;
		break;
	case R_SF:
		g_set.sf += d;
		if (g_set.sf < 0) g_set.sf = 10;
		if (g_set.sf > 10) g_set.sf = 0;
		break;
	case R_EXACT: g_set.prefer_exact = !g_set.prefer_exact; break;
	case R_ACCENT: {
		int n = theme_accent_count();
		g_set.accent = (g_set.accent + d + n) % n;
		theme_set_accent(g_set.accent);
		break;
	}
	default: return;
	}
	settings_save();
}

static void activate(int row)
{
	if (row == R_CLEAR) {
		if (!g_confirm_clear) {
			g_confirm_clear = true;
			return;
		}
		history_clear();
		g_confirm_clear = false;
		app_toast("History cleared");
	} else if (row == R_ABOUT) {
		app_open_about();
	} else {
		change(row, 1);
	}
}

static int swatch_x(int i)
{
	return ST_X + ST_W - 30 - (theme_accent_count() - i) * 40;
}

bool settings_view_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_DPAD, "Choose" },
		{ GLYPH_CONFIRM, "Change" },
		{ GLYPH_BACK, "Close" },
	};
	app_hints(hints, 3);
	if (in->pressed & (IN_BACK | IN_SELECT))
		return false;
	int prev = g_row;
	if (in->pressed & IN_UP) g_row = (g_row + R_COUNT - 1) % R_COUNT;
	if (in->pressed & IN_DOWN) g_row = (g_row + 1) % R_COUNT;
	if (g_row != prev)
		g_confirm_clear = false;
	if (in->pressed & IN_LEFT) change(g_row, -1);
	if (in->pressed & IN_RIGHT) change(g_row, 1);
	if (in->pressed & IN_CONFIRM) activate(g_row);
	if (in->tap) {
		if (!in_rect(in->tap_x, in->tap_y, ST_X, ST_Y, ST_W, ST_H))
			return false;
		for (int r = 0; r < R_COUNT; r++) {
			int y = ST_ROW_Y + r * ST_ROW_H;
			if (!in_rect(in->tap_x, in->tap_y, ST_X, y, ST_W, ST_ROW_H))
				continue;
			if (r != g_row)
				g_confirm_clear = false;
			g_row = r;
			if (r == R_ACCENT) {
				for (int i = 0; i < theme_accent_count(); i++)
					if (in_rect(in->tap_x, in->tap_y, swatch_x(i) - 4, y, 40, ST_ROW_H)) {
						g_set.accent = i;
						theme_set_accent(i);
						settings_save();
					}
			} else if (r <= R_EXACT) {
				change(r, 1);
			} else {
				activate(r);
			}
		}
	}
	return true;
}

void settings_view_draw(void)
{
	draw_rect(0, BODY_Y, SCR_W, BODY_H, COL_SCRIM);
	draw_shadow(ST_X, ST_Y, ST_W, ST_H, 18);
	draw_rrect(ST_X, ST_Y, ST_W, ST_H, 18, COL_BG2);
	draw_rrect_outline(ST_X, ST_Y, ST_W, ST_H, 18, 1, COL_LINE);
	font_draw_w(FW_BOLD, ST_X + 24, ST_Y + 14, COL_TEXT, 24, "Settings");
	font_draw_r(FW_REG, ST_X + ST_W - 24, ST_Y + 22, COL_DIM, 13, "Saved automatically");

	for (int r = 0; r < R_COUNT; r++) {
		int y = ST_ROW_Y + r * ST_ROW_H;
		bool sel = r == g_row;
		if (sel)
			draw_rrect(ST_X + 10, y + 3, ST_W - 20, ST_ROW_H - 6, 12, theme_accent_soft());
		if (r > 0 && !sel && r - 1 != g_row)
			draw_rect(ST_X + 24, y, ST_W - 48, 1, COL_LINE_SOFT);
		int ty = y + (ST_ROW_H - font_height(18)) / 2;
		unsigned int lc = r == R_CLEAR && g_confirm_clear ? COL_ERR : COL_TEXT;
		const char *lab = r == R_CLEAR && g_confirm_clear ? "Press again to clear history" : k_labels[r];
		font_draw_w(sel ? FW_MED : FW_REG, ST_X + 28, ty, lc, 18, lab);
		if (r == R_ACCENT) {
			for (int i = 0; i < theme_accent_count(); i++) {
				int sx = swatch_x(i);
				float cy = (float)(y + ST_ROW_H / 2);
				if (i == g_set.accent)
					draw_ring((float)sx + 16, cy, 15, 2, COL_TEXT, COL_BG2);
				draw_disc((float)sx + 16, cy, 11, theme_accent_swatch(i));
			}
			continue;
		}
		if (r >= R_CLEAR) {
			font_draw_r(FW_REG, ST_X + ST_W - 30, ty, COL_DIM, 18, "\xE2\x80\xBA");
			continue;
		}
		char v[32];
		value_text(r, v, sizeof v);
		const char *arrows = "\xE2\x97\x80 \xE2\x96\xB6";
		int aw = font_width(14, arrows);
		font_draw_r(FW_REG, ST_X + ST_W - 28, ty + 3, sel ? COL_TEXT2 : COL_DIM, 14, arrows);
		font_draw_r(FW_MED, ST_X + ST_W - 40 - aw, ty, sel ? theme_accent() : COL_TEXT2, 18, v);
	}
	char ver[48];
	snprintf(ver, sizeof ver, "Sumvita %s", SUMVITA_VERSION);
	font_draw_c(FW_REG, ST_X + ST_W / 2, ST_Y + ST_H - 26, COL_DIM, 13, ver);
}
