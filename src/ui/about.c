/* about.c - About overlay: logo, name, version, what is in the box. */
#include "ui/app.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/theme.h"

#include <stdio.h>

void *chrome_logo(void);

#define AB_W 560
#define AB_H 380
#define AB_X ((SCR_W - AB_W) / 2)
#define AB_Y (BODY_Y + (BODY_H - AB_H) / 2)

static const char *k_lines[] = {
	"Natural-display calculator for PS Vita",
	"Fractions, surds and exact answers, with working",
	"Algebra: simplify, expand, factorise, solve, rearrange",
	"Graphs with roots, turning points and intersections",
	"GCSE tools for every topic, step by step",
};

void about_open(void) {}

bool about_update(const input_t *in)
{
	static const Hint hints[] = { { GLYPH_BACK, "Close" } };
	app_hints(hints, 1);
	if (in->pressed & (IN_BACK | IN_CONFIRM | IN_SELECT))
		return false;
	if (in->tap)
		return false;
	return true;
}

void about_draw(void)
{
	draw_rect(0, BODY_Y, SCR_W, BODY_H, COL_SCRIM);
	draw_shadow(AB_X, AB_Y, AB_W, AB_H, 20);
	draw_rrect(AB_X, AB_Y, AB_W, AB_H, 20, COL_BG2);
	draw_rrect_outline(AB_X, AB_Y, AB_W, AB_H, 20, 1, COL_LINE);
	int cx = AB_X + AB_W / 2;
	void *logo = chrome_logo();
	if (logo) {
		draw_texture_fit(logo, cx - 48, AB_Y + 24, 96, 96);
	} else {
		draw_rrect(cx - 48, AB_Y + 24, 96, 96, 22, theme_accent_dim());
		font_draw_c(FW_BOLD, cx, AB_Y + 46, theme_on_accent(), 52, "\xCE\xA3");
	}
	font_draw_c(FW_BOLD, cx, AB_Y + 132, COL_TEXT, 30, "Sumvita");
	char ver[48];
	snprintf(ver, sizeof ver, "Version %s", SUMVITA_VERSION);
	font_draw_c(FW_MED, cx, AB_Y + 170, theme_accent(), 15, ver);
	int y = AB_Y + 206;
	for (unsigned i = 0; i < sizeof k_lines / sizeof k_lines[0]; i++) {
		font_draw_c(FW_REG, cx, y, i == 0 ? COL_TEXT2 : COL_MUTED, 15, k_lines[i]);
		y += 24;
	}
	font_draw_c(FW_REG, cx, AB_Y + AB_H - 30, COL_DIM, 13, "Title ID SUMV00001");
}
