/* graph_analysis.c - key points of one function over the visible x range,
 * found numerically on ex_eval: roots and intersections by sign change +
 * bisection (asymptotes rejected), turning points by golden-section search,
 * the y-intercept directly. Listed in a panel and marked on the plot. */
#include "ui/graph_int.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/mlayout.h"
#include "ui/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define AN_N 600
#define AN_W 262
#define AN_X (G_PX + G_PW - AN_W - 10)
#define AN_Y (G_PY + 10)
#define AN_ROW_H 46
#define AN_HEAD 58
#define AN_ROWS ((G_PH - 20 - AN_HEAD - 8) / AN_ROW_H)

/* f(x) - g(x) where other < 0 means just f */
static bool fdiff(int fn, int other, double x, double *y)
{
	double a, b;
	if (!g_eval(fn, x, &a))
		return false;
	if (other < 0) {
		*y = a;
		return true;
	}
	if (!g_eval(other, x, &b))
		return false;
	*y = a - b;
	return true;
}

static void add(GPKind k, int fn, int other, double x, double y)
{
	double tol = 2.0 / g_gs.scale;
	for (int i = 0; i < g_gs.npts; i++)
		if (fabs(g_gs.pts[i].x - x) < tol && fabs(g_gs.pts[i].y - y) < tol &&
		    (g_gs.pts[i].kind == k || (k == GP_ROOT && g_gs.pts[i].kind == GP_YINT)))
			return;
	if (g_gs.npts >= G_MAX_POINTS)
		return;
	if (fabs(x) < 1e-10) x = 0;
	if (fabs(y) < 1e-10) y = 0;
	g_gs.pts[g_gs.npts++] = (GPoint){ k, fn, other, x, y };
}

/* zeros of f - g (or f) by sign change, refined by bisection */
static void zeros(int fn, int other, GPKind kind, double x0, double x1)
{
	double h = (x1 - x0) / AN_N;
	double xa = x0, fa;
	bool oka = fdiff(fn, other, xa, &fa);
	for (int i = 1; i <= AN_N; i++) {
		double xb = x0 + h * i, fb;
		bool okb = fdiff(fn, other, xb, &fb);
		if (oka && fa == 0) {
			double y;
			if (g_eval(fn, xa, &y))
				add(kind, fn, other, xa, kind == GP_ROOT ? 0 : y);
		} else if (oka && okb && ((fa < 0) != (fb < 0)) && fb != 0) {
			double lo = xa, hi = xb, flo = fa;
			for (int it = 0; it < 64; it++) {
				double m = (lo + hi) / 2, fm;
				if (!fdiff(fn, other, m, &fm))
					break;
				if ((fm < 0) == (flo < 0)) { lo = m; flo = fm; }
				else hi = m;
			}
			double r = (lo + hi) / 2, fr, y;
			/* a sign change through a pole is not a root */
			if (fdiff(fn, other, r, &fr) && fabs(fr) < 1e-6 * (1 + fmax(fabs(fa), fabs(fb))) &&
			    g_eval(fn, r, &y))
				add(kind, fn, other, r, kind == GP_ROOT ? 0 : y);
		}
		xa = xb;
		fa = fb;
		oka = okb;
	}
}

/* golden-section refine of an extremum bracketed by [a, b] */
static double golden(int fn, double a, double b, bool max)
{
	const double gr = 0.6180339887498949;
	double c = b - gr * (b - a), d = a + gr * (b - a);
	for (int it = 0; it < 80 && fabs(b - a) > 1e-12 * (1 + fabs(a)); it++) {
		double fc, fd;
		if (!g_eval(fn, c, &fc) || !g_eval(fn, d, &fd))
			break;
		if (max ? fc > fd : fc < fd) b = d;
		else a = c;
		c = b - gr * (b - a);
		d = a + gr * (b - a);
	}
	return (a + b) / 2;
}

static void turning(int fn, double x0, double x1)
{
	double h = (x1 - x0) / AN_N;
	double f0, f1, f2;
	bool k0 = g_eval(fn, x0, &f0), k1 = g_eval(fn, x0 + h, &f1);
	for (int i = 2; i <= AN_N; i++) {
		double x = x0 + h * i;
		bool k2 = g_eval(fn, x, &f2);
		if (k0 && k1 && k2) {
			bool mx = f1 > f0 && f1 >= f2, mn = f1 < f0 && f1 <= f2;
			if (mx || mn) {
				double xt = golden(fn, x - 2 * h, x, mx), yt;
				/* real turning point, not a cusp at a pole: value stays modest */
				if (g_eval(fn, xt, &yt) && fabs(yt - f1) <= fabs(f1 - f0) + fabs(f2 - f1) + 1e-9) {
					add(mx ? GP_MAX : GP_MIN, fn, -1, xt, yt);
					if (fabs(yt) < 1e-7)
						add(GP_ROOT, fn, -1, xt, 0);
				}
			}
		}
		f0 = f1; k0 = k1;
		f1 = f2; k1 = k2;
	}
}

void g_analyse(int fn)
{
	g_gs.npts = 0;
	g_gs.pt_sel = 0;
	g_gs.pt_scroll = 0;
	double x0 = g_px_to_x(0), x1 = g_px_to_x(G_PW);
	zeros(fn, -1, GP_ROOT, x0, x1);
	double y;
	if (x0 <= 0 && x1 >= 0 && g_eval(fn, 0, &y))
		add(GP_YINT, fn, -1, 0, y);
	turning(fn, x0, x1);
	for (int j = 0; j < G_NFN; j++)
		if (j != fn && g_fn_visible(j))
			zeros(fn, j, GP_ISECT, x0, x1);
	/* keep each group in x order: roots, y-intercept, turning points, intersections */
	for (int i = 1; i < g_gs.npts; i++) {
		GPoint p = g_gs.pts[i];
		int k = i - 1;
		while (k >= 0 && (g_gs.pts[k].kind > p.kind || (g_gs.pts[k].kind == p.kind && g_gs.pts[k].x > p.x))) {
			g_gs.pts[k + 1] = g_gs.pts[k];
			k--;
		}
		g_gs.pts[k + 1] = p;
	}
}

const char *g_point_name(const GPoint *p, char *buf, int cap)
{
	switch (p->kind) {
	case GP_ROOT: snprintf(buf, (size_t)cap, "Root"); break;
	case GP_YINT: snprintf(buf, (size_t)cap, "y-intercept"); break;
	case GP_MIN: snprintf(buf, (size_t)cap, "Minimum"); break;
	case GP_MAX: snprintf(buf, (size_t)cap, "Maximum"); break;
	case GP_ISECT: snprintf(buf, (size_t)cap, "Meets y%d", p->other + 1); break;
	}
	return buf;
}

static void coords(const GPoint *p, char *buf, int cap)
{
	char a[24], b[24];
	g_fmt(p->x, a, sizeof a);
	g_fmt(p->y, b, sizeof b);
	snprintf(buf, (size_t)cap, "(%s, %s)", a, b);
}

void g_analysis_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_DPAD, "Point / function" }, { GLYPH_CONFIRM, "Centre on it" },
		{ GLYPH_TRIANGLE, "Trace" },        { GLYPH_BACK, "Close" },
	};
	app_hints(hints, 4);
	if (in->pressed & IN_BACK) { g_gs.mode = GM_NONE; return; }
	int n = g_gs.npts;
	if (n > 0) {
		if (in->pressed & IN_UP) g_gs.pt_sel = (g_gs.pt_sel + n - 1) % n;
		if (in->pressed & IN_DOWN) g_gs.pt_sel = (g_gs.pt_sel + 1) % n;
	}
	if (in->pressed & (IN_LEFT | IN_RIGHT)) {
		int d = (in->pressed & IN_LEFT) ? G_NFN - 1 : 1;
		for (int k = 1; k < G_NFN; k++) {
			int j = (g_gs.sel + d * k) % G_NFN;
			if (g_fn_visible(j)) {
				g_gs.sel = j;
				g_analyse(j);
				break;
			}
		}
	}
	if (in->tap && in_rect(in->tap_x, in->tap_y, AN_X, AN_Y + AN_HEAD, AN_W, AN_ROWS * AN_ROW_H)) {
		int r = g_gs.pt_scroll + (in->tap_y - AN_Y - AN_HEAD) / AN_ROW_H;
		if (r < n) g_gs.pt_sel = r;
	}
	if (g_gs.pt_sel < g_gs.pt_scroll) g_gs.pt_scroll = g_gs.pt_sel;
	if (g_gs.pt_sel >= g_gs.pt_scroll + AN_ROWS) g_gs.pt_scroll = g_gs.pt_sel - AN_ROWS + 1;
	if (n > 0 && (in->pressed & IN_CONFIRM)) {
		const GPoint *p = &g_gs.pts[g_gs.pt_sel];
		g_gs.cx = p->x;
		g_gs.cy = p->y;
		g_gs.ver++;
	}
	if (in->pressed & IN_TRIANGLE) {
		g_gs.mode = GM_TRACE;
		g_gs.trace_x = n > 0 ? g_gs.pts[g_gs.pt_sel].x : g_gs.cx;
	}
}

void g_points_draw(void)
{
	for (int i = 0; i < g_gs.npts; i++) {
		const GPoint *p = &g_gs.pts[i];
		float px = (float)(G_PX + g_x_to_px(p->x)), py = (float)(G_PY + g_y_to_py(p->y));
		if (px < G_PX - 10 || px > G_PX + G_PW + 10 || py < G_PY - 10 || py > G_PY + G_PH + 10)
			continue;
		bool sel = i == g_gs.pt_sel;
		draw_disc(px, py, sel ? 9 : 6, COL_BG);
		draw_disc(px, py, sel ? 6.5f : 4, sel ? COL_TEXT : theme_graph_colour(p->fn));
		if (sel) {
			char c[64];
			coords(p, c, sizeof c);
			int w = font_width_w(FW_MED, 15, c) + 16;
			int lx = (int)px + 12, ly = (int)py - 34;
			if (lx + w > AN_X - 6) lx = (int)px - 12 - w;
			if (ly < G_PY + 6) ly = (int)py + 12;
			draw_rrect(lx, ly, w, 26, 8, COL_PANEL2);
			font_draw_w(FW_MED, lx + 8, ly + 4, COL_TEXT, 15, c);
		}
	}
}

void g_analysis_draw(void)
{
	int fn = g_gs.sel;
	int h = AN_HEAD + 8 + (g_gs.npts ? (g_gs.npts < AN_ROWS ? g_gs.npts : AN_ROWS) : 1) * AN_ROW_H;
	draw_shadow(AN_X, AN_Y, AN_W, h, 12);
	draw_rrect(AN_X, AN_Y, AN_W, h, 12, COL_BG2);
	draw_rrect_outline(AN_X, AN_Y, AN_W, h, 12, 1, COL_LINE);
	draw_rrect(AN_X + 14, AN_Y + 14, 12, 12, 3, theme_graph_colour(fn));
	char t[24];
	snprintf(t, sizeof t, "y%d key points", fn + 1);
	font_draw_w(FW_BOLD, AN_X + 34, AN_Y + 10, COL_TEXT, 17, t);
	const MLayout *l = mlc_fit(g_gs.fn[fn].src, 15, 12, AN_W - 28);
	mdraw(l, AN_X + 14, AN_Y + 46, COL_MUTED);
	int y = AN_Y + AN_HEAD;
	if (g_gs.npts == 0) {
		font_draw(AN_X + 14, y + 12, COL_DIM, 14, "Nothing in view - pan or zoom out");
		return;
	}
	char name[24], c[64];
	for (int i = g_gs.pt_scroll; i < g_gs.npts && i < g_gs.pt_scroll + AN_ROWS; i++) {
		const GPoint *p = &g_gs.pts[i];
		bool sel = i == g_gs.pt_sel;
		if (sel)
			draw_rrect(AN_X + 6, y + 2, AN_W - 12, AN_ROW_H - 4, 9, theme_accent_soft());
		font_draw_w(FW_MED, AN_X + 16, y + 5, sel ? theme_accent() : COL_MUTED, 13, g_point_name(p, name, 24));
		coords(p, c, sizeof c);
		font_draw_w(FW_MED, AN_X + 16, y + 22, COL_TEXT, 16, c);
		y += AN_ROW_H;
	}
	if (g_gs.npts > AN_ROWS) {
		snprintf(c, sizeof c, "%d / %d", g_gs.pt_sel + 1, g_gs.npts);
		font_draw_r(FW_REG, AN_X + AN_W - 14, AN_Y + 14, COL_DIM, 12, c);
	}
}
