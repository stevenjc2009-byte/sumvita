/* Per-frame draw counters. Each vita2d draw entry point is wrapped with
 * -Wl,--wrap=NAME so every call is counted without touching the draw code. */
#include "ui/perf.h"

#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

static perf_frame_t g_cur, g_last;
static SceUInt64 g_t0;

void perf_frame_begin(void)
{
	g_cur.draws = g_cur.glyphs = 0;
	g_t0 = sceKernelGetProcessTimeWide();
}

void perf_frame_end(void)
{
	g_cur.cpu_us = (unsigned)(sceKernelGetProcessTimeWide() - g_t0);
	g_last = g_cur;
}

perf_frame_t perf_last(void) { return g_last; }

/* --wrap targets: count, then call the real vita2d function */
void __real_vita2d_draw_line(float, float, float, float, unsigned int);
void __real_vita2d_draw_rectangle(float, float, float, float, unsigned int);
void __real_vita2d_draw_fill_circle(float, float, float, unsigned int);
void __real_vita2d_draw_array(SceGxmPrimitiveType, const vita2d_color_vertex *, size_t);
void __real_vita2d_draw_texture_scale(const vita2d_texture *, float, float, float, float);
void __real_vita2d_draw_texture_tint_part_scale(const vita2d_texture *, float, float, float, float,
						float, float, float, float, unsigned int);

void __wrap_vita2d_draw_line(float x0, float y0, float x1, float y1, unsigned int c)
{
	g_cur.draws++;
	__real_vita2d_draw_line(x0, y0, x1, y1, c);
}

void __wrap_vita2d_draw_rectangle(float x, float y, float w, float h, unsigned int c)
{
	g_cur.draws++;
	__real_vita2d_draw_rectangle(x, y, w, h, c);
}

void __wrap_vita2d_draw_fill_circle(float x, float y, float r, unsigned int c)
{
	g_cur.draws++;
	__real_vita2d_draw_fill_circle(x, y, r, c);
}

void __wrap_vita2d_draw_array(SceGxmPrimitiveType m, const vita2d_color_vertex *v, size_t n)
{
	g_cur.draws++;
	__real_vita2d_draw_array(m, v, n);
}

void __wrap_vita2d_draw_texture_scale(const vita2d_texture *t, float x, float y, float sx, float sy)
{
	g_cur.draws++;
	__real_vita2d_draw_texture_scale(t, x, y, sx, sy);
}

/* the font code draws each glyph with this one (called from inside libvita2d) */
void __wrap_vita2d_draw_texture_tint_part_scale(const vita2d_texture *t, float x, float y, float tx,
						float ty, float tw, float th, float sx, float sy,
						unsigned int c)
{
	g_cur.draws++;
	g_cur.glyphs++;
	__real_vita2d_draw_texture_tint_part_scale(t, x, y, tx, ty, tw, th, sx, sy, c);
}
