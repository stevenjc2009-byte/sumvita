/* mlayout_wrap.c - brackets round a fragment: font glyphs at text height,
 * drawn curves and lines when the contents are taller. Split from mlayout.c. */
#include "ui/mlayout.h"

#include "ui/font.h"

#include <string.h>

static int maxi(int a, int b) { return a > b ? a : b; }

static MOp mk_rect(int x, int y, int w, int h)
{
	MOp op;
	memset(&op, 0, sizeof op);
	op.kind = MO_RECT;
	op.x = (int16_t)x; op.y = (int16_t)y; op.x2 = (int16_t)w; op.y2 = (int16_t)h;
	return op;
}

static MOp mk_line(float x0, float y0, float x1, float y1, int thick)
{
	MOp op;
	memset(&op, 0, sizeof op);
	op.kind = MO_LINE;
	op.x = (int16_t)(x0 + 0.5f); op.y = (int16_t)(y0 + 0.5f);
	op.x2 = (int16_t)(x1 + 0.5f); op.y2 = (int16_t)(y1 + 0.5f);
	op.px = (uint8_t)thick;
	return op;
}

static int line_w(int px) { return px >= 26 ? 2 : 1; }

/* One bracket drawn with lines into [x, x + bw] x [yt, yb]. */
static Frag vec_bracket(Frag f, const char *b, int x, int bw, int yt, int yb, int lw, int left)
{
	float xa = (float)x, xb = (float)(x + bw);
	if (!strcmp(b, "(") || !strcmp(b, ")")) {
		/* quadratic curve from (outer, top) through (inner, mid) to (outer, bottom) */
		float out = left ? xb - (float)bw * 0.15f : xa + (float)bw * 0.15f;
		float in = left ? xa + (float)bw * 0.15f : xb - (float)bw * 0.15f;
		const int N = 10;
		float px0 = out, py0 = (float)yt;
		for (int i = 1; i <= N; i++) {
			float t = (float)i / (float)N, u = 1.0f - t;
			float cx = u * u * out + 2 * u * t * (in - (out - in) * 0.35f) + t * t * out;
			float cy = u * u * (float)yt + 2 * u * t * (float)(yt + yb) * 0.5f + t * t * (float)yb;
			f = ml_add_op(f, mk_line(px0, py0, cx, cy, lw + (i > 2 && i < N - 1 ? 1 : 0)));
			px0 = cx;
			py0 = cy;
		}
		return f;
	}
	float mid = (xa + xb) * 0.5f;
	if (!strcmp(b, "|")) {
		f = ml_add_op(f, mk_rect((int)mid, yt, lw, yb - yt));
		return f;
	}
	int top = strcmp(b, "⌊") && strcmp(b, "⌋");   /* floor has no top serif */
	int bot = strcmp(b, "⌈") && strcmp(b, "⌉");   /* ceil has no bottom serif */
	int vx = left ? (int)(xa + (float)bw * 0.35f) : (int)(xb - (float)bw * 0.35f) - lw;
	int sx = left ? vx : (int)(xa + (float)bw * 0.2f);
	int sw = (int)((float)bw * 0.45f);
	f = ml_add_op(f, mk_rect(vx, yt, lw, yb - yt));
	if (top)
		f = ml_add_op(f, mk_rect(sx, yt, sw, lw));
	if (bot)
		f = ml_add_op(f, mk_rect(sx, yb - lw, sw, lw));
	return f;
}

Frag ml_wrap(Frag inner, const char *l, const char *r, int px, int role)
{
	int ta = ml_text_asc(px), td = ml_text_desc(px);
	int small = inner.asc <= ta + ta / 8 && inner.desc <= td + td / 3;
	if (small) {
		int lw = l ? font_width_w(FW_REG, px, l) : 0;
		int rw = r ? font_width_w(FW_REG, px, r) : 0;
		if (!strcmp(l ? l : "", "|")) lw += 2;
		ml_shift(inner, lw + 1, 0);
		Frag f = inner;
		if (f.n == 0)
			f.start = ml_mark();
		if (l) {
			Frag t = ml_text(l, px, FW_REG, role);
			f.n = t.start + t.n - f.start;
		}
		if (r) {
			Frag t = ml_text(r, px, FW_REG, role);
			ml_shift(t, lw + 1 + inner.w + 1, 0);
			f.n = t.start + t.n - f.start;
		}
		f.w = lw + 1 + inner.w + 1 + rw;
		f.asc = maxi(inner.asc, ta);
		f.desc = maxi(inner.desc, td);
		return f;
	}
	int yt = -inner.asc - 2, yb = inner.desc + 2;
	int h = yb - yt;
	int bw = maxi(px * 36 / 100, h / 7);
	if (bw > px * 7 / 10)
		bw = px * 7 / 10;
	int lw = line_w(px);
	int lwid = l ? bw : 0, rwid = r ? bw : 0;
	ml_shift(inner, lwid + 2, 0);
	Frag f = inner;
	if (f.n == 0)
		f.start = ml_mark();
	if (l)
		f = vec_bracket(f, l, 0, bw, yt, yb, lw, 1);
	if (r)
		f = vec_bracket(f, r, lwid + 2 + inner.w + 2, bw, yt, yb, lw, 0);
	f.w = lwid + 2 + inner.w + 2 + rwid;
	f.asc = -yt;
	f.desc = yb;
	return f;
}
