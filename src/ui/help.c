/* help.c - Help overlay: a titled page of short text, "n/N" at the top. L/R and
 * the d-pad left/right change page; d-pad up/down and touch drag scroll a long
 * page; Circle closes. The words live in help_text.c. */
#include "ui/help.h"

#include "ui/app.h"
#include "ui/draw.h"
#include "ui/font.h"
#include "ui/theme.h"

#include <stdio.h>

#define HP_W 800
#define HP_X ((SCR_W - HP_W) / 2)
#define HP_Y (BODY_Y + 8)
#define HP_H (BODY_H - 16)
#define HP_HEAD 62
#define HP_LIST_Y (HP_Y + HP_HEAD)
#define HP_LIST_H (HP_H - HP_HEAD - 10)
#define HP_PAD 34
#define HP_PX 17
#define HP_LINE (HP_PX + 8)
#define HP_HEAD_PX 18
#define ARW_R (HP_X + HP_W - HP_PAD - 16)   /* page arrows, left of the margin */
#define ARW_L (ARW_R - 96)

static int g_page;
static float g_scroll;

/* height of one line of page text */
static int line_h(const char *s)
{
	if (!s[0])
		return HP_LINE / 2;
	if (s[0] == '#')
		return HP_LINE + 6;
	return HP_LINE;
}

static int content_h(int page)
{
	int h = 0;
	for (const char *const *l = g_help_pages[page].lines; *l; l++)
		h += line_h(*l);
	return h;
}

static float max_scroll(void)
{
	float m = (float)(content_h(g_page) - HP_LIST_H + 8);
	return m > 0 ? m : 0;
}

static void set_page(int p)
{
	g_page = (p + g_help_npages) % g_help_npages;
	g_scroll = 0;
}

void help_open(int page)
{
	if (page < 0 || page >= g_help_npages)
		page = 0;
	g_page = page;
	g_scroll = 0;
}

bool help_update(const input_t *in)
{
	static const Hint hints[] = {
		{ -1, "\xE2\x97\x80\xE2\x96\xB6 Page" },
		{ GLYPH_BACK, "Close" },
	};
	app_hints(hints, 2);
	if (in->pressed & (IN_BACK | IN_SELECT))
		return false;
	if (in->tap) {
		if (!in_rect(in->tap_x, in->tap_y, HP_X, HP_Y, HP_W, HP_H))
			return false;
		if (in->tap_y < HP_LIST_Y - 6) {
			if (in_rect(in->tap_x, in->tap_y, ARW_L - 14, HP_Y, 44, HP_HEAD))
				set_page(g_page - 1);
			else if (in_rect(in->tap_x, in->tap_y, ARW_R - 14, HP_Y, 44, HP_HEAD))
				set_page(g_page + 1);
		}
	}
	if (in->pressed & (IN_L | IN_LEFT))
		set_page(g_page - 1);
	if (in->pressed & (IN_R | IN_RIGHT))
		set_page(g_page + 1);
	if (in->pressed & IN_DOWN)
		g_scroll += 44;
	if (in->pressed & IN_UP)
		g_scroll -= 44;
	g_scroll += input_rstick_y(in) * 14.0f;
	if (in->dragging)
		g_scroll -= (float)in->drag_dy;
	if (g_scroll > max_scroll())
		g_scroll = max_scroll();
	if (g_scroll < 0)
		g_scroll = 0;
	return true;
}

void help_draw(void)
{
	const HelpPage *pg = &g_help_pages[g_page];
	draw_rect(0, BODY_Y, SCR_W, BODY_H, COL_SCRIM);
	draw_shadow(HP_X, HP_Y, HP_W, HP_H, 18);
	draw_rrect(HP_X, HP_Y, HP_W, HP_H, 18, COL_BG2);
	draw_rrect_outline(HP_X, HP_Y, HP_W, HP_H, 18, 1, COL_LINE);

	/* header: page arrows, title, n/N */
	font_draw_w(FW_BOLD, HP_X + HP_PAD, HP_Y + 14, COL_TEXT, 24, pg->title);
	char cnt[16];
	snprintf(cnt, sizeof cnt, "%d/%d", g_page + 1, g_help_npages);
	font_draw_c(FW_MED, (ARW_L + ARW_R) / 2 + 8, HP_Y + 20, theme_accent(), 16, cnt);
	font_draw_w(FW_REG, ARW_L, HP_Y + 20, COL_MUTED, 16, "◀");
	font_draw_w(FW_REG, ARW_R, HP_Y + 20, COL_MUTED, 16, "▶");
	draw_rect(HP_X + 20, HP_LIST_Y - 6, HP_W - 40, 1, COL_LINE_SOFT);

	draw_clip(HP_X + 4, HP_LIST_Y, HP_W - 8, HP_LIST_H);
	int y = HP_LIST_Y + 4 - (int)g_scroll;
	for (const char *const *l = pg->lines; *l; l++) {
		const char *s = *l;
		int h = line_h(s);
		if (y + h > HP_LIST_Y && y < HP_LIST_Y + HP_LIST_H) {
			if (s[0] == '#')
				font_draw_w(FW_BOLD, HP_X + HP_PAD, y + 6, theme_accent(), HP_HEAD_PX, s + 2);
			else if (s[0])
				font_draw_w(FW_REG, HP_X + HP_PAD, y, COL_TEXT2, HP_PX, s);
		}
		y += h;
	}
	draw_clip_off();

	float ms = max_scroll();
	if (ms > 0) {
		int total = content_h(g_page) + 8;
		int th = HP_LIST_H * HP_LIST_H / total;
		if (th < 24)
			th = 24;
		int ty = HP_LIST_Y + (int)((float)(HP_LIST_H - th) * g_scroll / ms);
		draw_rrect(HP_X + HP_W - 12, ty, 4, th, 2, COL_LINE);
	}
}
