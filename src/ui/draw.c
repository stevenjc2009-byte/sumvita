#include "ui/draw.h"

#include "ui/font.h"
#include "ui/input.h"
#include "ui/theme.h"

#include <math.h>
#include <psp2/io/stat.h>
#include <vita2d.h>

void draw_rect(int x, int y, int w, int h, unsigned int color)
{
	if (w <= 0 || h <= 0)
		return;
	vita2d_draw_rectangle((float)x, (float)y, (float)w, (float)h, color);
}

void draw_rrect(int x, int y, int w, int h, int r, unsigned int color)
{
	if (w <= 0 || h <= 0)
		return;
	if (r * 2 > h) r = h / 2;
	if (r * 2 > w) r = w / 2;
	if (r <= 1) {
		draw_rect(x, y, w, h, color);
		return;
	}
	/* Translucent colours must not overlap themselves: centre column, two side
	 * strips, and four quarter discs drawn as clipped circles would overlap, so
	 * opaque and translucent take the same path but corner discs only cover the
	 * corner squares (the strips leave those empty). */
	draw_rect(x + r, y, w - 2 * r, h, color);
	draw_rect(x, y + r, r, h - 2 * r, color);
	draw_rect(x + w - r, y + r, r, h - 2 * r, color);
	unsigned int a = color >> 24;
	if (a == 255) {
		vita2d_draw_fill_circle((float)(x + r), (float)(y + r), (float)r, color);
		vita2d_draw_fill_circle((float)(x + w - r), (float)(y + r), (float)r, color);
		vita2d_draw_fill_circle((float)(x + r), (float)(y + h - r), (float)r, color);
		vita2d_draw_fill_circle((float)(x + w - r), (float)(y + h - r), (float)r, color);
		return;
	}
	/* Translucent: draw corners row by row so nothing is painted twice. */
	for (int i = 0; i < r; i++) {
		float dy = (float)r - (float)i - 0.5f;
		int span = (int)(sqrtf((float)(r * r) - dy * dy) + 0.5f);
		draw_rect(x + r - span, y + i, span, 1, color);
		draw_rect(x + w - r, y + i, span, 1, color);
		draw_rect(x + r - span, y + h - 1 - i, span, 1, color);
		draw_rect(x + w - r, y + h - 1 - i, span, 1, color);
	}
}

void draw_rrect_ring(int x, int y, int w, int h, int r, int t, unsigned int ring, unsigned int inner)
{
	draw_rrect(x, y, w, h, r, ring);
	if (inner)
		draw_rrect(x + t, y + t, w - 2 * t, h - 2 * t, r > t ? r - t : 0, inner);
}

static unsigned int lerp_color(unsigned int a, unsigned int b, int num, int den)
{
	unsigned int out = 0;
	for (int shift = 0; shift < 32; shift += 8) {
		int ca = (int)((a >> shift) & 0xFF);
		int cb = (int)((b >> shift) & 0xFF);
		out |= (unsigned int)(ca + (cb - ca) * num / den) << shift;
	}
	return out;
}

void draw_gradient_v(int x, int y, int w, int h, unsigned int top, unsigned int bottom)
{
	const int step = 2;
	for (int row = 0; row < h; row += step) {
		int hh = (row + step > h) ? h - row : step;
		draw_rect(x, y + row, w, hh, lerp_color(top, bottom, row, h > 1 ? h - 1 : 1));
	}
}

void draw_border(int x, int y, int w, int h, int t, unsigned int color)
{
	draw_rect(x, y, w, t, color);
	draw_rect(x, y + h - t, w, t, color);
	draw_rect(x, y + t, t, h - 2 * t, color);
	draw_rect(x + w - t, y + t, t, h - 2 * t, color);
}

void draw_shadow(int x, int y, int w, int h, int r)
{
	draw_rrect(x - 1, y + 2, w + 2, h + 3, r + 1, RGBA8(0, 0, 0, 40));
	draw_rrect(x, y + 1, w, h + 2, r, RGBA8(0, 0, 0, 60));
}

void draw_line_w(float x0, float y0, float x1, float y1, float w, unsigned int color)
{
	if (w <= 1.25f) {
		vita2d_draw_line(x0, y0, x1, y1, color);
		return;
	}
	float dx = x1 - x0, dy = y1 - y0;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 0.01f) {
		vita2d_draw_fill_circle(x0, y0, w * 0.5f, color);
		return;
	}
	/* Several parallel 1 px lines across the normal: cheap and gap-free. */
	float nx = -dy / len, ny = dx / len;
	int n = (int)(w + 0.5f);
	for (int i = 0; i < n; i++) {
		float o = (float)i - (float)(n - 1) * 0.5f;
		vita2d_draw_line(x0 + nx * o, y0 + ny * o, x1 + nx * o, y1 + ny * o, color);
	}
}

void draw_disc(float cx, float cy, float r, unsigned int color)
{
	vita2d_draw_fill_circle(cx, cy, r, color);
}

void draw_ring(float cx, float cy, float r, float t, unsigned int color, unsigned int inner)
{
	vita2d_draw_fill_circle(cx, cy, r, color);
	vita2d_draw_fill_circle(cx, cy, r - t, inner);
}

void draw_dashed_rect(int x, int y, int w, int h, unsigned int color)
{
	const int dash = 3, gap = 2;
	for (int i = 0; i < w; i += dash + gap) {
		int l = i + dash > w ? w - i : dash;
		draw_rect(x + i, y, l, 1, color);
		draw_rect(x + i, y + h - 1, l, 1, color);
	}
	for (int i = 0; i < h; i += dash + gap) {
		int l = i + dash > h ? h - i : dash;
		draw_rect(x, y + i, 1, l, color);
		draw_rect(x + w - 1, y + i, 1, l, color);
	}
}

void draw_clip(int x, int y, int w, int h)
{
	vita2d_enable_clipping();
	vita2d_set_clip_rectangle(x, y, x + w, y + h);
}

void draw_clip_off(void)
{
	vita2d_disable_clipping();
}

void *draw_load_png(const char *path)
{
	SceIoStat st;
	if (sceIoGetstat(path, &st) < 0)
		return NULL;
	return vita2d_load_PNG_file(path);
}

void draw_texture_fit(void *tex, int x, int y, int w, int h)
{
	vita2d_texture *t = tex;
	if (!t || w <= 0 || h <= 0) return;
	float tw = (float)vita2d_texture_get_width(t), th = (float)vita2d_texture_get_height(t);
	if (tw <= 0 || th <= 0) return;
	float s = (float)w / tw < (float)h / th ? (float)w / tw : (float)h / th;
	vita2d_draw_texture_scale(t, (float)x + ((float)w - tw * s) / 2, (float)y + ((float)h - th * s) / 2, s, s);
}

/* ---- glyphs ------------------------------------------------------------------ */

#define GLYPH_RING   RGBA8(170, 178, 192, 255)
#define GLYPH_DISC   RGBA8(26, 30, 38, 255)
#define GLYPH_PILL   RGBA8(196, 202, 212, 255)
#define GLYPH_INK    RGBA8(24, 28, 36, 255)
#define COL_CROSS    RGBA8(124, 178, 232, 255)
#define COL_CIRCLE   RGBA8(255, 102, 102, 255)
#define COL_SQUARE   RGBA8(255, 105, 248, 255)
#define COL_TRIANGLE RGBA8(64, 226, 160, 255)

static void thick_line(int x0, int y0, int x1, int y1, unsigned int c)
{
	vita2d_draw_line((float)x0, (float)y0, (float)x1, (float)y1, c);
	vita2d_draw_line((float)x0 + 1, (float)y0, (float)x1 + 1, (float)y1, c);
	vita2d_draw_line((float)x0, (float)y0 + 1, (float)x1, (float)y1 + 1, c);
}

static void disc(int x, int y)
{
	float c = GLYPH_SIZE / 2.0f;
	vita2d_draw_fill_circle((float)x + c, (float)y + c, c, GLYPH_RING);
	vita2d_draw_fill_circle((float)x + c, (float)y + c, c - 1.0f, GLYPH_DISC);
}

static int pill(int x, int y, int w, const char *text, int px)
{
	draw_rrect(x, y + 2, w, GLYPH_SIZE - 4, (GLYPH_SIZE - 4) / 2, GLYPH_PILL);
	int tw = font_width_w(FW_BOLD, px, text);
	font_draw_w(FW_BOLD, x + (w - tw) / 2, y + 2 + (GLYPH_SIZE - 4 - font_height(px)) / 2, GLYPH_INK, px, text);
	return w;
}

static glyph_t resolve(glyph_t g)
{
	if (g == GLYPH_CONFIRM)
		return input_confirm_is_cross() ? GLYPH_CROSS : GLYPH_CIRCLE;
	if (g == GLYPH_BACK)
		return input_confirm_is_cross() ? GLYPH_CIRCLE : GLYPH_CROSS;
	return g;
}

int draw_glyph_width(glyph_t g)
{
	switch (resolve(g)) {
	case GLYPH_L:
	case GLYPH_R:      return 24;
	case GLYPH_LR:     return 50;
	case GLYPH_START:  return font_width_w(FW_BOLD, 10, "START") + 12;
	case GLYPH_SELECT: return font_width_w(FW_BOLD, 10, "SELECT") + 12;
	default:           return GLYPH_SIZE;
	}
}

int draw_glyph(glyph_t g, int x, int y)
{
	g = resolve(g);
	int cx = x + GLYPH_SIZE / 2, cy = y + GLYPH_SIZE / 2;
	switch (g) {
	case GLYPH_CROSS:
		disc(x, y);
		thick_line(cx - 4, cy - 4, cx + 4, cy + 4, COL_CROSS);
		thick_line(cx - 4, cy + 4, cx + 4, cy - 4, COL_CROSS);
		break;
	case GLYPH_CIRCLE:
		disc(x, y);
		vita2d_draw_fill_circle((float)cx, (float)cy, 5.5f, COL_CIRCLE);
		vita2d_draw_fill_circle((float)cx, (float)cy, 3.5f, GLYPH_DISC);
		break;
	case GLYPH_SQUARE:
		disc(x, y);
		draw_rect(cx - 4, cy - 4, 9, 9, COL_SQUARE);
		draw_rect(cx - 2, cy - 2, 5, 5, GLYPH_DISC);
		break;
	case GLYPH_TRIANGLE:
		disc(x, y);
		thick_line(cx, cy - 5, cx - 5, cy + 4, COL_TRIANGLE);
		thick_line(cx, cy - 5, cx + 5, cy + 4, COL_TRIANGLE);
		thick_line(cx - 5, cy + 4, cx + 5, cy + 4, COL_TRIANGLE);
		break;
	case GLYPH_L:     return pill(x, y, 24, "L", 11);
	case GLYPH_R:     return pill(x, y, 24, "R", 11);
	case GLYPH_LR:
		pill(x, y, 24, "L", 11);
		pill(x + 26, y, 24, "R", 11);
		return 50;
	case GLYPH_START:  return pill(x, y, draw_glyph_width(g), "START", 10);
	case GLYPH_SELECT: return pill(x, y, draw_glyph_width(g), "SELECT", 10);
	case GLYPH_DPAD:
		draw_rrect(x + 1, cy - 3, GLYPH_SIZE - 2, 6, 2, GLYPH_PILL);
		draw_rrect(cx - 3, y + 1, 6, GLYPH_SIZE - 2, 2, GLYPH_PILL);
		draw_rect(cx - 1, cy - 1, 3, 3, GLYPH_INK);
		break;
	case GLYPH_STICK_L:
	case GLYPH_STICK_R:
		disc(x, y);
		vita2d_draw_fill_circle((float)cx, (float)cy, 5.0f, GLYPH_PILL);
		font_draw_c(FW_BOLD, cx, cy - 6, GLYPH_INK, 8, g == GLYPH_STICK_L ? "L" : "R");
		break;
	case GLYPH_TOUCH:
		vita2d_draw_fill_circle((float)cx, (float)cy, 8.0f, col_alpha(GLYPH_PILL, 70));
		vita2d_draw_fill_circle((float)cx, (float)cy, 4.5f, GLYPH_PILL);
		break;
	default:
		break;
	}
	return GLYPH_SIZE;
}

void draw_rrect_outline(int x, int y, int w, int h, int r, int t, unsigned int color)
{
	if (w <= 0 || h <= 0 || t <= 0)
		return;
	if (r * 2 > h) r = h / 2;
	if (r * 2 > w) r = w / 2;
	if (r < t) r = t;
	int ri = r - t;
	for (int i = 0; i < r; i++) {
		float dy = (float)r - (float)i - 0.5f;
		int so = (int)(sqrtf((float)(r * r) - dy * dy) + 0.5f);
		int si = 0;
		if (i >= t && ri > 0) {
			float di = (float)ri - (float)(i - t) - 0.5f;
			float q = (float)(ri * ri) - di * di;
			si = q > 0 ? (int)(sqrtf(q) + 0.5f) : 0;
		}
		int top = y + i, bot = y + h - 1 - i;
		if (i < t) {
			draw_rect(x + r - so, top, w - 2 * (r - so), 1, color);
			draw_rect(x + r - so, bot, w - 2 * (r - so), 1, color);
			continue;
		}
		int lx = x + r - so;
		int len = so - si;
		if (len < 1) len = 1;
		draw_rect(lx, top, len, 1, color);
		draw_rect(x + w - r + si, top, len, 1, color);
		draw_rect(lx, bot, len, 1, color);
		draw_rect(x + w - r + si, bot, len, 1, color);
	}
	draw_rect(x, y + r, t, h - 2 * r, color);
	draw_rect(x + w - t, y + r, t, h - 2 * r, color);
}
