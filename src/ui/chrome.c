/* chrome.c - top bar (logo, mode tabs, angle chip, settings gear), bottom hint
 * bar, toast, buttons and panel titles shared by every screen. */
#include "ui/app.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/settings.h"
#include "ui/theme.h"

#include <string.h>

#define TAB_X0 176
#define TAB_W 116
#define TAB_GAP 4
#define CHIP_X 798
#define CHIP_W 70
#define GEAR_X 880
#define GEAR_W 68

static void *g_logo;
static int g_logo_tried;

void *chrome_logo(void);
void *chrome_logo(void)
{
	if (!g_logo_tried) {
		g_logo_tried = 1;
		g_logo = draw_load_png("app0:img/logo.png");
	}
	return g_logo;
}

void topbar_draw(int mode)
{
	draw_rect(0, 0, SCR_W, TOP_H, COL_BG2);
	draw_rect(0, TOP_H - 1, SCR_W, 1, COL_LINE_SOFT);
	void *logo = chrome_logo();
	if (logo) {
		draw_texture_fit(logo, 12, 7, 32, 32);
	} else {
		draw_rrect(12, 7, 32, 32, 8, theme_accent_dim());
		font_draw_c(FW_BOLD, 28, 11, theme_on_accent(), 20, "\xCE\xA3");
	}
	font_draw_w(FW_BOLD, 52, 12, COL_TEXT, 20, "Sumvita");

	/* L / R hints either side of the tabs */
	draw_glyph(GLYPH_L, TAB_X0 - 26, (TOP_H - GLYPH_SIZE) / 2);
	int tabs_end = TAB_X0 + MODE_COUNT * (TAB_W + TAB_GAP) - TAB_GAP;
	draw_glyph(GLYPH_R, tabs_end + 8, (TOP_H - GLYPH_SIZE) / 2);
	for (int i = 0; i < MODE_COUNT; i++) {
		int x = TAB_X0 + i * (TAB_W + TAB_GAP);
		bool on = i == mode;
		if (on) {
			draw_rrect(x, 6, TAB_W, TOP_H - 12, 9, theme_accent_soft());
			draw_rect(x + 16, TOP_H - 4, TAB_W - 32, 3, theme_accent());
		}
		font_draw_c(on ? FW_BOLD : FW_MED, x + TAB_W / 2, 13, on ? theme_accent() : COL_MUTED, 16, app_mode_name(i));
	}

	/* angle chip */
	draw_rrect_ring(CHIP_X, 9, CHIP_W, TOP_H - 18, 8, 1, COL_LINE, COL_PANEL);
	font_draw_c(FW_BOLD, CHIP_X + CHIP_W / 2, 14, g_set.degrees ? COL_TEXT2 : COL_WARN, 14,
		    g_set.degrees ? "DEG" : "RAD");
	/* settings gear (DejaVu fallback glyph) + Select hint */
	font_draw_w(FW_REG, GEAR_X + 4, 9, COL_TEXT2, 22, "\xE2\x9A\x99");
	draw_glyph(GLYPH_SELECT, GEAR_X + 34, (TOP_H - GLYPH_SIZE) / 2);
}

int topbar_hit(int tx, int ty)
{
	if (ty >= TOP_H)
		return -1;
	for (int i = 0; i < MODE_COUNT; i++) {
		int x = TAB_X0 + i * (TAB_W + TAB_GAP);
		if (tx >= x && tx < x + TAB_W)
			return i;
	}
	if (tx >= CHIP_X && tx < CHIP_X + CHIP_W)
		return MODE_COUNT + 1;
	if (tx >= GEAR_X)
		return MODE_COUNT;
	return -1;
}

void hintbar_draw(const Hint *h, int n)
{
	int y = SCR_H - HINT_H;
	draw_rect(0, y, SCR_W, HINT_H, COL_BG2);
	draw_rect(0, y, SCR_W, 1, COL_LINE_SOFT);
	int x = 14;
	for (int i = 0; i < n; i++) {
		if (h[i].glyph >= 0)
			x += draw_glyph((glyph_t)h[i].glyph, x, y + (HINT_H - GLYPH_SIZE) / 2) + 6;
		font_draw(x, y + 7, COL_MUTED, 14, h[i].text);
		x += font_width(14, h[i].text) + 20;
		if (x > SCR_W - 40)
			break;
	}
}

void toast_draw(const char *msg, int alpha)
{
	if (!msg || !*msg || alpha <= 0)
		return;
	int w = font_width_w(FW_MED, 16, msg) + 40;
	int x = (SCR_W - w) / 2, y = SCR_H - HINT_H - 58;
	draw_shadow(x, y, w, 40, 12);
	draw_rrect(x, y, w, 40, 12, col_alpha(COL_PANEL3, alpha));
	draw_rect(x + 12, y + 39, w - 24, 1, col_alpha(theme_accent(), alpha));
	font_draw_c(FW_MED, SCR_W / 2, y + 10, col_alpha(COL_TEXT, alpha), 16, msg);
}

void panel_title(int x, int y, const char *title, const char *sub)
{
	font_draw_w(FW_BOLD, x, y, COL_TEXT, 22, title);
	if (sub && *sub)
		font_draw(x + font_width_w(FW_BOLD, 22, title) + 12, y + 6, COL_MUTED, 15, sub);
}

int button_draw(int x, int y, int w, int h, const char *label, int style, bool focus, bool pressed)
{
	unsigned int fill = style == 1 ? theme_accent() : style == 2 ? (unsigned int)COL_PANEL2 : (unsigned int)COL_PANEL3;
	unsigned int tc = style == 1 ? theme_on_accent() : COL_TEXT;
	if (pressed)
		fill = col_mix(fill, theme_accent(), 120);
	int r = h / 2 < 10 ? h / 2 : 10;
	draw_rrect(x, y + 2, w, h, r, COL_KEY_EDGE);
	draw_rrect(x, y, w, h, r, fill);
	if (focus)
		draw_rrect_outline(x - 2, y - 2, w + 4, h + 4, r + 2, 2, theme_accent());
	int px = h >= 40 ? 17 : h >= 30 ? 15 : 13;
	while (font_width_w(FW_MED, px, label) > w - 10 && px > 11)
		px--;
	font_draw_c(FW_MED, x + w / 2, y + (h - font_height(px)) / 2 + 1, tc, px, label);
	return w;
}
