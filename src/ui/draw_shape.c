/* draw_shape.c - filled shapes as ONE vita2d_draw_array call each: rectangles,
 * rounded rectangles (a fan of triangles from the centre over the outline,
 * 4-8 segments per corner), rounded outlines (a ring of quads), shadows and
 * thick lines. The triangles of one shape never overlap, so a translucent
 * colour blends exactly once and shared edges leave no seams.
 * A shape run (draw_shapes_begin/end) merges every shape drawn inside it into
 * one call; triangles keep their order, so the picture is the same. */
#include "ui/draw.h"

#include <math.h>
#include <string.h>
#include <vita2d.h>

#define RUN_VERTS 6144 /* vertices a shape run holds before it flushes (96 KiB) */
#define SEG_MAX   8    /* segments per corner, largest radius */
#define PTS_MAX   (4 * (SEG_MAX + 1))

static vita2d_color_vertex g_run[RUN_VERTS];
static int g_run_n, g_run_depth;

static void run_flush(void)
{
	if (g_run_n <= 0)
		return;
	vita2d_color_vertex *v = vita2d_pool_memalign((unsigned int)(g_run_n * (int)sizeof *v), sizeof *v);
	if (v) {
		memcpy(v, g_run, (size_t)g_run_n * sizeof *v);
		vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, v, (size_t)g_run_n);
	}
	g_run_n = 0;
}

void draw_shapes_begin(void) { g_run_depth++; }

void draw_shapes_end(void)
{
	if (g_run_depth > 0 && --g_run_depth == 0)
		run_flush();
}

/* Room for n vertices: the run buffer inside a run, else GPU pool memory. */
static vita2d_color_vertex *reserve(int n)
{
	if (g_run_depth > 0) {
		if (g_run_n + n > RUN_VERTS)
			run_flush();
		if (n <= RUN_VERTS) {
			vita2d_color_vertex *v = g_run + g_run_n;
			g_run_n += n;
			return v;
		}
	}
	return vita2d_pool_memalign((unsigned int)(n * (int)sizeof(vita2d_color_vertex)), sizeof(vita2d_color_vertex));
}

static void submit(const vita2d_color_vertex *v, int n)
{
	if (v && (g_run_depth == 0 || n > RUN_VERTS))
		vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, v, (size_t)n);
}

static void vtx(vita2d_color_vertex *v, float x, float y, unsigned int color)
{
	v->x = x;
	v->y = y;
	v->z = 0.5f;
	v->color = color;
}

static void quad(vita2d_color_vertex *v, float ax, float ay, float bx, float by,
		 float cx, float cy, float dx, float dy, unsigned int color)
{
	vtx(v + 0, ax, ay, color);
	vtx(v + 1, bx, by, color);
	vtx(v + 2, cx, cy, color);
	vtx(v + 3, ax, ay, color);
	vtx(v + 4, cx, cy, color);
	vtx(v + 5, dx, dy, color);
}

void draw_rect(int x, int y, int w, int h, unsigned int color)
{
	if (w <= 0 || h <= 0)
		return;
	if (g_run_depth == 0) {
		vita2d_draw_rectangle((float)x, (float)y, (float)w, (float)h, color);
		return;
	}
	vita2d_color_vertex *v = reserve(6);
	float x0 = (float)x, y0 = (float)y, x1 = (float)(x + w), y1 = (float)(y + h);
	quad(v, x0, y0, x1, y0, x1, y1, x0, y1, color);
}

/* ---- rounded outline points --------------------------------------------------- */

static int segs_for(float r) { return r <= 8.0f ? 4 : r <= 20.0f ? 6 : SEG_MAX; }

/* cos / sin of k * 90deg / s for s = 4, 6, 8 */
static float g_cos[SEG_MAX + 1][SEG_MAX + 1], g_sin[SEG_MAX + 1][SEG_MAX + 1];
static int g_trig_ready;

static void trig_init(void)
{
	for (int s = 4; s <= SEG_MAX; s += 2)
		for (int k = 0; k <= s; k++) {
			float a = (float)k * 1.57079633f / (float)s;
			g_cos[s][k] = k == s ? 0.0f : cosf(a);
			g_sin[s][k] = k == 0 ? 0.0f : k == s ? 1.0f : sinf(a);
		}
	g_trig_ready = 1;
}

/* 4 * (s + 1) points clockwise round a rounded rectangle, from the left end of
 * the top-left corner; every corner arc is centred r in from both edges. */
static int rr_points(float x, float y, float w, float h, float r, int s, float *px, float *py)
{
	if (!g_trig_ready)
		trig_init();
	const float *c = g_cos[s], *sn = g_sin[s];
	float l = x + r, t = y + r, rt = x + w - r, b = y + h - r;
	int n = 0;
	for (int k = 0; k <= s; k++, n++) { px[n] = l - r * c[k];   py[n] = t - r * sn[k]; }
	for (int k = 0; k <= s; k++, n++) { px[n] = rt + r * sn[k]; py[n] = t - r * c[k]; }
	for (int k = 0; k <= s; k++, n++) { px[n] = rt + r * c[k];  py[n] = b + r * sn[k]; }
	for (int k = 0; k <= s; k++, n++) { px[n] = l - r * sn[k];  py[n] = b + r * c[k]; }
	return n;
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
	float px[PTS_MAX], py[PTS_MAX];
	int n = rr_points((float)x, (float)y, (float)w, (float)h, (float)r, segs_for((float)r), px, py);
	vita2d_color_vertex *v = reserve(3 * n);
	if (!v)
		return;
	float cx = (float)x + (float)w * 0.5f, cy = (float)y + (float)h * 0.5f;
	for (int i = 0; i < n; i++) {
		int j = i + 1 == n ? 0 : i + 1;
		vtx(v + 3 * i + 0, cx, cy, color);
		vtx(v + 3 * i + 1, px[i], py[i], color);
		vtx(v + 3 * i + 2, px[j], py[j], color);
	}
	submit(v, 3 * n);
}

void draw_rrect_ring(int x, int y, int w, int h, int r, int t, unsigned int ring, unsigned int inner)
{
	draw_shapes_begin();
	draw_rrect(x, y, w, h, r, ring);
	if (inner)
		draw_rrect(x + t, y + t, w - 2 * t, h - 2 * t, r > t ? r - t : 0, inner);
	draw_shapes_end();
}

void draw_rrect_outline(int x, int y, int w, int h, int r, int t, unsigned int color)
{
	if (w <= 0 || h <= 0 || t <= 0)
		return;
	if (r * 2 > h) r = h / 2;
	if (r * 2 > w) r = w / 2;
	if (r < t) r = t;
	if (w <= 2 * t || h <= 2 * t) {
		draw_rrect(x, y, w, h, r, color);
		return;
	}
	/* outer and inner outlines share corner centres and angles: point i of one
	 * pairs with point i of the other, so the quads between them never overlap */
	int s = segs_for((float)r);
	float ox[PTS_MAX], oy[PTS_MAX], ix[PTS_MAX], iy[PTS_MAX];
	int n = rr_points((float)x, (float)y, (float)w, (float)h, (float)r, s, ox, oy);
	rr_points((float)(x + t), (float)(y + t), (float)(w - 2 * t), (float)(h - 2 * t), (float)(r - t), s, ix, iy);
	vita2d_color_vertex *v = reserve(6 * n);
	if (!v)
		return;
	for (int i = 0; i < n; i++) {
		int j = i + 1 == n ? 0 : i + 1;
		quad(v + 6 * i, ox[i], oy[i], ox[j], oy[j], ix[j], iy[j], ix[i], iy[i], color);
	}
	submit(v, 6 * n);
}

void draw_shadow(int x, int y, int w, int h, int r)
{
	draw_shapes_begin();
	draw_rrect(x - 1, y + 2, w + 2, h + 3, r + 1, RGBA8(0, 0, 0, 40));
	draw_rrect(x, y + 1, w, h + 2, r, RGBA8(0, 0, 0, 60));
	draw_shapes_end();
}

void draw_line_w(float x0, float y0, float x1, float y1, float w, unsigned int color)
{
	float dx = x1 - x0, dy = y1 - y0;
	float len = sqrtf(dx * dx + dy * dy);
	if (w <= 1.25f && g_run_depth == 0) {
		vita2d_draw_line(x0, y0, x1, y1, color);
		return;
	}
	if (len < 0.01f) {
		run_flush(); /* keeps the order inside a shape run */
		vita2d_draw_fill_circle(x0, y0, w * 0.5f, color);
		return;
	}
	/* one quad as wide as the n parallel 1 px lines it replaces */
	int n = (int)(w + 0.5f);
	float hw = (float)(n < 1 ? 1 : n) * 0.5f;
	float nx = -dy / len * hw, ny = dx / len * hw;
	vita2d_color_vertex *v = reserve(6);
	if (!v)
		return;
	quad(v, x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, color);
	submit(v, 6);
}
