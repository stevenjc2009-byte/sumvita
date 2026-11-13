/* steps_view.c - the working overlay: one card per step (bold title, muted
 * note, natural-display maths), indented by depth, with the final answer at
 * the end. Scrolls with the d-pad, the right stick and touch drag. */
#include "ui/app.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/mlayout.h"
#include "ui/text.h"
#include "ui/theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SV_X 34
#define SV_Y (BODY_Y + 8)
#define SV_W (SCR_W - 68)
#define SV_H (BODY_H - 16)
#define SV_HEAD 74      /* least header height; a tall echo (a fraction) grows it */
#define SV_ECHO_Y (SV_Y + 44)
#define SV_LIST_Y (SV_Y + g_head)
#define SV_LIST_H (SV_H - g_head - 8)
#define SV_INDENT 28
#define SV_MATH_PX 24
#define SV_NOTE_PX 14
#define SV_TITLE_PX 17
#define SV_GAP 10

static Res g_r;
static bool g_has;
/* Wrapped once when the view opens: text_wrap is O(L^2), too slow per frame. */
typedef struct {
	int h;                          /* card height */
	int nl;                         /* note lines */
	char title[TEXT_LINE_MAX];      /* ellipsized to the card */
	char note[3][TEXT_LINE_MAX];
} SvCard;
static SvCard *g_c;
static int g_total;
static float g_scroll;
static int g_head = SV_HEAD;
static int g_final_h = 92;

static int card_w(int depth)
{
	return SV_W - 70 - depth * SV_INDENT;
}

/* wrap and ellipsize one step's text and measure its card */
static void card_build(const RStep *s, SvCard *c)
{
	int w = card_w(s->depth) - 28;
	int h = 12;
	c->title[0] = 0;
	c->nl = 0;
	if (s->title && s->title[0]) {
		text_ellipsize(SV_TITLE_PX, s->title, w, c->title);
		h += font_height(SV_TITLE_PX) + 2;
	}
	if (s->note && s->note[0])
		c->nl = text_wrap(SV_NOTE_PX, s->note, w, 3, c->note);
	h += c->nl * (font_height(SV_NOTE_PX) + 1);
	if (s->math && s->math[0]) {
		const MLayout *l = mlc_fit(s->math, SV_MATH_PX, 14, w);
		h += 6 + l->asc + l->desc;
	}
	c->h = h + 12;
}

static int final_h(void) { return g_final_h; }

static const MLayout *echo_layout(void)
{
	const char *src = g_r.echo && g_r.echo[0] ? g_r.echo : g_r.input;
	return src && src[0] ? mlc_fit(src, 18, 12, SV_W - 60) : NULL;
}

static const MLayout *final_layout(void) { return mlc_fit(g_r.answer, 30, 16, card_w(0) - 28); }

void steps_view_open(const Res *r)
{
	res_free(&g_r);
	res_clone(&g_r, r);
	g_has = true;
	free(g_c);
	g_c = calloc((size_t)(g_r.nsteps + 1), sizeof(SvCard));
	/* header and answer card sized from their real maths height: a fraction
	 * echo used to cross the header rule, a fraction answer the card edge */
	const MLayout *e = echo_layout();
	g_head = SV_HEAD;
	if (e && SV_ECHO_Y - SV_Y + e->asc + e->desc + 10 > g_head)
		g_head = SV_ECHO_Y - SV_Y + e->asc + e->desc + 10;
	g_final_h = 64;
	if (g_r.ok) {
		const MLayout *a = final_layout();
		g_final_h = 36 + a->asc + a->desc + 14;
		if (g_final_h < 92)
			g_final_h = 92;
	}
	g_total = 0;
	for (int i = 0; i < g_r.nsteps; i++) {
		card_build(&g_r.steps[i], &g_c[i]);
		g_total += g_c[i].h + SV_GAP;
	}
	g_total += final_h();
	g_scroll = 0;
}

static float max_scroll(void)
{
	float m = (float)(g_total - SV_LIST_H + 8);
	return m > 0 ? m : 0;
}

bool steps_view_update(const input_t *in)
{
	static const Hint hints[] = {
		{ -1, "\xE2\x96\xB2\xE2\x96\xBC Scroll" },
		{ GLYPH_BACK, "/" },
		{ GLYPH_TRIANGLE, "Close" },
	};
	app_hints(hints, 3);
	if (in->pressed & (IN_BACK | IN_TRIANGLE))
		return false;
	if (in->tap && !in_rect(in->tap_x, in->tap_y, SV_X, SV_Y, SV_W, SV_H))
		return false;
	if (in->pressed & IN_DOWN)
		g_scroll += 48;
	if (in->pressed & IN_UP)
		g_scroll -= 48;
	if (in->pressed & IN_R)
		g_scroll += SV_LIST_H - 40;
	if (in->pressed & IN_L)
		g_scroll -= SV_LIST_H - 40;
	g_scroll += input_rstick_y(in) * 14.0f + input_lstick_y(in) * 10.0f;
	if (in->dragging)
		g_scroll -= (float)in->drag_dy;
	if (g_scroll > max_scroll())
		g_scroll = max_scroll();
	if (g_scroll < 0)
		g_scroll = 0;
	return true;
}

static void draw_card(const RStep *s, const SvCard *c, int idx, int y)
{
	int x = SV_X + 54 + s->depth * SV_INDENT;
	int w = card_w(s->depth);
	int h = c->h;
	draw_rrect(x, y, w, h, 12, s->depth ? COL_PANEL : COL_PANEL2);
	/* timeline dot + number */
	int cx = SV_X + 28 + s->depth * SV_INDENT;
	if (s->depth == 0) {
		draw_disc((float)cx, (float)(y + 20), 13, theme_accent_dim());
		char nb[16];
		snprintf(nb, sizeof nb, "%d", idx);
		font_draw_c(FW_BOLD, cx, y + 20 - font_height(14) / 2, theme_on_accent(), 14, nb);
	} else {
		draw_disc((float)cx, (float)(y + 18), 5, COL_LINE);
	}
	int ty = y + 12;
	int inner = w - 28;
	if (c->title[0]) {
		font_draw_w(FW_BOLD, x + 14, ty, COL_TEXT, SV_TITLE_PX, c->title);
		ty += font_height(SV_TITLE_PX) + 2;
	}
	for (int i = 0; i < c->nl; i++) {
		font_draw(x + 14, ty, COL_MUTED, SV_NOTE_PX, c->note[i]);
		ty += font_height(SV_NOTE_PX) + 1;
	}
	if (s->math && s->math[0]) {
		const MLayout *l = mlc_fit(s->math, SV_MATH_PX, 14, inner);
		mdraw(l, x + 14, ty + 6 + l->asc, COL_TEXT);
	}
}

static void draw_final(int y)
{
	int x = SV_X + 54, w = card_w(0);
	int h = final_h();
	draw_rrect(x, y, w, h, 12, g_r.ok ? theme_accent_soft() : COL_PANEL2);
	draw_rrect_outline(x, y, w, h, 12, 1, g_r.ok ? theme_accent() : COL_ERR);
	font_draw_w(FW_BOLD, x + 14, y + 10, g_r.ok ? theme_accent() : COL_ERR, 15, g_r.ok ? "Answer" : "Error");
	if (!g_r.ok) {
		font_draw_w(FW_MED, x + 14, y + 34, COL_ERR, 16, g_r.error);
		return;
	}
	const MLayout *l = final_layout();
	mdraw(l, x + 14, y + 36 + (h - 50 + l->asc - l->desc) / 2, COL_TEXT);
}

void steps_view_draw(void)
{
	if (!g_has)
		return;
	draw_rect(0, BODY_Y, SCR_W, BODY_H, COL_SCRIM);
	draw_shadow(SV_X, SV_Y, SV_W, SV_H, 16);
	draw_rrect(SV_X, SV_Y, SV_W, SV_H, 16, COL_BG2);
	draw_rrect_outline(SV_X, SV_Y, SV_W, SV_H, 16, 1, COL_LINE);

	/* header: title, what was asked */
	const char *lab = g_r.label && g_r.label[0] ? g_r.label : "Working";
	font_draw_w(FW_BOLD, SV_X + 22, SV_Y + 12, COL_TEXT, 22, lab);
	char cnt[32];
	snprintf(cnt, sizeof cnt, "%d step%s", g_r.nsteps, g_r.nsteps == 1 ? "" : "s");
	font_draw_r(FW_MED, SV_X + SV_W - 22, SV_Y + 16, COL_DIM, 14, cnt);
	const MLayout *el = echo_layout();
	if (el)
		mdraw(el, SV_X + 22, SV_ECHO_Y + el->asc, COL_MUTED);
	draw_rect(SV_X + 16, SV_LIST_Y - 4, SV_W - 32, 1, COL_LINE_SOFT);

	draw_clip(SV_X + 4, SV_LIST_Y, SV_W - 8, SV_LIST_H);
	int y = SV_LIST_Y + 6 - (int)g_scroll;
	/* timeline spine */
	if (g_r.nsteps > 0)
		draw_rect(SV_X + 27, y + 20, 2, g_total - final_h() - 10, COL_LINE_SOFT);
	int num = 0;   /* numbers count the top-level steps only */
	for (int i = 0; i < g_r.nsteps; i++) {
		int h = g_c[i].h;
		if (g_r.steps[i].depth == 0)
			num++;
		if (y + h > SV_LIST_Y && y < SV_LIST_Y + SV_LIST_H)
			draw_card(&g_r.steps[i], &g_c[i], num, y);
		y += h + SV_GAP;
	}
	if (g_r.nsteps == 0)
		font_draw(SV_X + 54, y, COL_DIM, 15, "No working for this one.");
	draw_final(g_r.nsteps == 0 ? y + 30 : y);
	draw_clip_off();

	float ms = max_scroll();
	if (ms > 0) {
		int th = SV_LIST_H * SV_LIST_H / (g_total + 8);
		if (th < 24)
			th = 24;
		int ty = SV_LIST_Y + (int)((float)(SV_LIST_H - th) * g_scroll / ms);
		draw_rrect(SV_X + SV_W - 9, ty, 4, th, 2, COL_LINE);
	}
}
