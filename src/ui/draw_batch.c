/* draw_batch.c - see draw_batch.h. Vertices are collected in a static buffer and
 * copied into vita2d's per-frame pool at flush time, so the pool only pays for
 * what was really used and stays valid until the frame is submitted. */
#include "ui/draw_batch.h"

#include <math.h>
#include <string.h>
#include <vita2d.h>

#define BATCH_MAX 8190		/* vertices per call; a multiple of 6 */
#define POOL_RESERVE 65536	/* never take the last 64 KB of the pool */

static vita2d_color_vertex buf[BATCH_MAX];
static int nv;
static unsigned int cur;
static int calls;

void batch_end(void)
{
	if (nv == 0)
		return;
	unsigned int bytes = (unsigned int)nv * sizeof buf[0];
	if (vita2d_pool_free_space() > bytes + POOL_RESERVE) {
		vita2d_color_vertex *v = vita2d_pool_memalign(bytes, sizeof buf[0]);
		if (v) {
			memcpy(v, buf, bytes);
			vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, v, (size_t)nv);
			calls++;
		}
	}
	nv = 0;
}

int batch_calls_take(void)
{
	int c = calls;
	calls = 0;
	return c;
}

static void put(float x, float y, unsigned int color)
{
	buf[nv].x = x;
	buf[nv].y = y;
	buf[nv].z = 0.5f;
	buf[nv].color = color;
	nv++;
}

static void quad(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy,
		 unsigned int color)
{
	if (color != cur) {
		batch_end();
		cur = color;
	}
	if (nv + 6 > BATCH_MAX)
		batch_end();
	put(ax, ay, color);
	put(bx, by, color);
	put(cx, cy, color);
	put(ax, ay, color);
	put(cx, cy, color);
	put(dx, dy, color);
}

void batch_rect(float x, float y, float w, float h, unsigned int color)
{
	if (w <= 0 || h <= 0)
		return;
	quad(x, y, x + w, y, x + w, y + h, x, y + h, color);
}

void batch_line(float x0, float y0, float x1, float y1, float w, unsigned int color,
		int ext0, int ext1)
{
	float dx = x1 - x0, dy = y1 - y0;
	float len = sqrtf(dx * dx + dy * dy);
	float hw = w * 0.5f;
	if (len < 0.01f) {
		/* a dot: a square of the pen width */
		quad(x0 - hw, y0 - hw, x0 + hw, y0 - hw, x0 + hw, y0 + hw, x0 - hw, y0 + hw, color);
		return;
	}
	float ux = dx / len, uy = dy / len;
	float nx = -uy * hw, ny = ux * hw;
	if (ext0) { x0 -= ux * hw; y0 -= uy * hw; }
	if (ext1) { x1 += ux * hw; y1 += uy * hw; }
	quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, color);
}
