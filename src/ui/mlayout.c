/* mlayout.c - op-buffer builders for natural display (see mlayout.h). */
#include "ui/mlayout.h"

#include "ui/font.h"

#include <stdlib.h>
#include <string.h>

#define POOL_BYTES (64 * 1024)

static MOp *g_ops;
static int g_n, g_cap;
static char g_pool[POOL_BYTES];
static int g_pool_used;

void ml_reset(void)
{
	g_n = 0;
	g_pool_used = 0;
}

int ml_mark(void) { return g_n; }

static int push(MOp op)
{
	if (g_n == g_cap) {
		int nc = g_cap ? g_cap * 2 : 256;
		MOp *nv = realloc(g_ops, sizeof(MOp) * (size_t)nc);
		if (!nv)
			return -1;
		g_ops = nv;
		g_cap = nc;
	}
	g_ops[g_n++] = op;
	return 0;
}

static const char *pool_dup(const char *s)
{
	size_t n = strlen(s) + 1;
	if (g_pool_used + (int)n > POOL_BYTES)
		return "";
	char *d = g_pool + g_pool_used;
	memcpy(d, s, n);
	g_pool_used += (int)n;
	return d;
}

/* ---- typography ---- */

int ml_quant(int px)
{
	static const int k_sizes[] = { 12, 14, 16, 18, 20, 22, 24, 28, 32, 36 };
	int best = k_sizes[0];
	for (unsigned i = 0; i < sizeof k_sizes / sizeof k_sizes[0]; i++)
		if (k_sizes[i] <= px)
			best = k_sizes[i];
	return best;
}

int ml_script_px(int px) { return ml_quant(px * 7 / 10 < 12 ? 12 : px * 7 / 10); }
int ml_axis(int px) { return (px * 30 + 50) / 100; }
int ml_text_asc(int px) { return (px * 76 + 50) / 100; }
int ml_text_desc(int px) { return (px * 22 + 50) / 100; }

static int maxi(int a, int b) { return a > b ? a : b; }

/* Union of two op ranges (either may be empty; they must be adjacent). */
static Frag span(Frag a, Frag b)
{
	Frag f = a;
	if (a.n == 0 && b.n == 0) {
		f.start = g_n;
		f.n = 0;
		return f;
	}
	if (a.n == 0) {
		f.start = b.start;
		f.n = b.n;
		return f;
	}
	if (b.n == 0)
		return f;
	int s0 = a.start < b.start ? a.start : b.start;
	int e0 = maxi(a.start + a.n, b.start + b.n);
	f.start = s0;
	f.n = e0 - s0;
	return f;
}

/* ---- fragments ---- */

Frag ml_empty(void)
{
	Frag f = { 0, 0, 0, g_n, 0 };
	return f;
}

Frag ml_text(const char *s, int px, int weight, int role)
{
	Frag f = ml_empty();
	if (!s || !*s)
		return f;
	MOp op;
	memset(&op, 0, sizeof op);
	op.kind = MO_TEXT;
	op.px = (uint8_t)px;
	op.weight = (uint8_t)weight;
	op.role = (uint8_t)role;
	op.text = pool_dup(s);
	if (push(op) == 0)
		f.n = 1;
	f.w = font_width_w((fweight_t)weight, px, s);
	f.asc = ml_text_asc(px);
	f.desc = ml_text_desc(px);
	return f;
}

Frag ml_space(int w, int px)
{
	Frag f = ml_empty();
	f.w = w;
	f.asc = ml_text_asc(px) / 2;
	f.desc = 0;
	return f;
}

void ml_shift(Frag f, int dx, int dy)
{
	for (int i = f.start; i < f.start + f.n && i < g_n; i++) {
		MOp *o = &g_ops[i];
		o->x = (int16_t)(o->x + dx);
		o->y = (int16_t)(o->y + dy);
		if (o->kind == MO_LINE) {
			o->x2 = (int16_t)(o->x2 + dx);
			o->y2 = (int16_t)(o->y2 + dy);
		}
	}
}

Frag ml_cat(Frag a, Frag b)
{
	ml_shift(b, a.w, 0);
	Frag f = span(a, b);
	f.w = a.w + b.w;
	f.asc = maxi(a.asc, b.asc);
	f.desc = maxi(a.desc, b.desc);
	return f;
}

Frag ml_add_op(Frag f, MOp op)
{
	if (f.n == 0)
		f.start = g_n;
	if (push(op) == 0)
		f.n = g_n - f.start;
	return f;
}

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

Frag ml_frac(Frag num, Frag den, int px)
{
	int t = px >= 30 ? 2 : 1;
	int pad = maxi(3, px / 6);
	int gap = maxi(2, px / 9);
	int W = maxi(num.w, den.w) + 2 * pad;
	int bar_top = -ml_axis(px) - t / 2;
	int nb = bar_top - gap - num.desc;
	int db = bar_top + t + gap + den.asc;
	ml_shift(num, (W - num.w) / 2, nb);
	ml_shift(den, (W - den.w) / 2, db);
	Frag f = span(num, den);
	f = ml_add_op(f, mk_rect(1, bar_top, W - 2, t));
	f.w = W;
	f.asc = num.asc - nb;
	f.desc = db + den.desc;
	return f;
}

Frag ml_sup(Frag base, Frag exp, int px)
{
	int rise = base.asc - exp.asc / 2;
	int lo = px * 42 / 100;
	if (rise < lo)
		rise = lo;
	ml_shift(exp, base.w + 1, -rise);
	Frag f = span(base, exp);
	f.w = base.w + 1 + exp.w;
	f.asc = maxi(base.asc, rise + exp.asc);
	f.desc = maxi(base.desc, exp.desc - rise);
	return f;
}

Frag ml_sub(Frag base, Frag sub, int px)
{
	int drop = px * 22 / 100;
	ml_shift(sub, base.w + 1, drop);
	Frag f = span(base, sub);
	f.w = base.w + 1 + sub.w;
	f.asc = maxi(base.asc, sub.asc - drop);
	f.desc = maxi(base.desc, drop + sub.desc);
	return f;
}

Frag ml_root(Frag index, Frag rad, int px)
{
	int gap = maxi(2, px / 9);
	int lw = line_w(px);
	int y_top = -(rad.asc + gap + lw);
	int y_bot = rad.desc;
	if (y_bot < ml_text_desc(px) / 2)
		y_bot = ml_text_desc(px) / 2;
	int H = y_bot - y_top;
	float S = (float)px * 0.62f;
	if (S > (float)H * 0.75f)
		S = (float)H * 0.75f;
	if (S < 9.0f)
		S = 9.0f;
	/* index sits above the tick, right edge at 0.55 S */
	int off = 0;
	int ix = (int)(S * 0.55f) - index.w;
	if (ix < 0) {
		off = -ix;
		ix = 0;
	}
	float p0x = 0, p0y = (float)y_top + (float)H * 0.60f;
	float p1x = S * 0.30f, p1y = (float)y_top + (float)H * 0.52f;
	float p2x = S * 0.60f, p2y = (float)y_bot;
	float p3x = S, p3y = (float)y_top + (float)lw * 0.5f;
	if (index.n) {
		int ib = (int)p1y - 3 - index.desc;
		ml_shift(index, ix, ib);
	}
	ml_shift(rad, off + (int)S + 3, 0);
	Frag f = span(index, rad);
	float o = (float)off;
	f = ml_add_op(f, mk_line(o + p0x, p0y, o + p1x, p1y, lw));
	f = ml_add_op(f, mk_line(o + p1x, p1y, o + p2x, p2y, lw + 1));
	f = ml_add_op(f, mk_line(o + p2x, p2y, o + p3x, p3y, lw));
	int bar_end = off + (int)S + 3 + rad.w + 3;
	f = ml_add_op(f, mk_rect(off + (int)S, y_top, bar_end - off - (int)S, lw));
	f.w = bar_end + 1;
	f.asc = -y_top;
	if (index.n) {
		int ia = -((int)p1y - 3 - index.desc) + index.asc;
		f.asc = maxi(f.asc, ia);
	}
	f.desc = maxi(y_bot, rad.desc);
	return f;
}

Frag ml_overbar(Frag f, int px)
{
	int t = px >= 30 ? 2 : 1;
	int y = -f.asc - 3;
	f = ml_add_op(f, mk_rect(0, y - t, f.w, t));
	f.asc += 3 + t;
	return f;
}

Frag ml_dot_above(Frag f, int x, int px)
{
	MOp op;
	memset(&op, 0, sizeof op);
	op.kind = MO_DOT;
	op.px = (uint8_t)px;
	op.x = (int16_t)x;
	/* +1: at history size (16 px) the dot sat on the digit's top edge */
	op.y = (int16_t)(-ml_text_asc(px) - px / 7 - 1);
	f = ml_add_op(f, op);
	int need = ml_text_asc(px) + px / 7 + 1 + px / 10 + 1;
	if (f.asc < need)
		f.asc = need;
	return f;
}

Frag ml_slot(int px)
{
	Frag f = ml_empty();
	int a = ml_text_asc(px), d = ml_text_desc(px);
	int w = maxi(8, px * 55 / 100);
	MOp op;
	memset(&op, 0, sizeof op);
	op.kind = MO_SLOT;
	op.x = 2;
	op.y = (int16_t)(-a + 1);
	op.x2 = (int16_t)w;
	op.y2 = (int16_t)(a + d - 2);
	f = ml_add_op(f, op);
	f.w = w + 4;
	f.asc = a;
	f.desc = d;
	return f;
}

Frag ml_caret(int seq, int pos, int px)
{
	Frag f = ml_empty();
	MOp op;
	memset(&op, 0, sizeof op);
	op.kind = MO_CARET;
	op.x2 = (int16_t)seq;
	op.y2 = (int16_t)pos;
	int a = ml_text_asc(px) + 2, d = ml_text_desc(px) + 1;
	op.px = (uint8_t)(a > 255 ? 255 : a);
	op.weight = (uint8_t)(d > 255 ? 255 : d);
	f = ml_add_op(f, op);
	return f;
}

MLayout *ml_finish(Frag f)
{
	size_t sbytes = 0;
	for (int i = f.start; i < f.start + f.n; i++)
		if (g_ops[i].kind == MO_TEXT && g_ops[i].text)
			sbytes += strlen(g_ops[i].text) + 1;
	size_t total = sizeof(MLayout) + sizeof(MOp) * (size_t)(f.n > 0 ? f.n : 0) + sbytes;
	char *mem = malloc(total);
	if (!mem)
		return NULL;
	MLayout *l = (MLayout *)mem;
	l->w = f.w;
	l->asc = f.asc;
	l->desc = f.desc;
	l->nops = f.n > 0 ? f.n : 0;
	l->ops = (MOp *)(mem + sizeof(MLayout));
	char *s = (char *)(l->ops + l->nops);
	for (int i = 0; i < l->nops; i++) {
		l->ops[i] = g_ops[f.start + i];
		if (l->ops[i].kind == MO_TEXT && l->ops[i].text) {
			size_t n = strlen(l->ops[i].text) + 1;
			memcpy(s, l->ops[i].text, n);
			l->ops[i].text = s;
			s += n;
		}
	}
	return l;
}

void ml_free(MLayout *l) { free(l); }
