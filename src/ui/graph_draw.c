/* graph_draw.c - the function list and the plot: grid, axes with tick labels,
 * curves (broken at asymptotes), trace marker. */
#include "ui/graph_int.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/mlayout.h"
#include "ui/theme.h"

#include <math.h>
#include <stdio.h>

/* 1, 2 or 5 x 10^k, at least `raw` */
static double nice_step(double raw)
{
	double m = pow(10, floor(log10(raw)));
	double f = raw / m;
	return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * m;
}

void g_draw_list(void)
{
	bool foc = g_gs.focus == GF_LIST;
	draw_rrect(G_LIST_X, G_LIST_Y, G_LIST_W, G_LIST_H, 14, COL_BG2);
	font_draw_w(FW_BOLD, G_LIST_X + 16, G_LIST_Y + 12, COL_TEXT, 18, "Functions");
	font_draw_r(FW_REG, G_LIST_X + G_LIST_W - 14, G_LIST_Y + 16, COL_DIM, 13, foc ? "editing list" : "");
	for (int i = 0; i < G_NFN; i++) {
		const GFn *f = &g_gs.fn[i];
		int y = G_LIST_ROW_Y + i * G_LIST_ROW_H;
		bool sel = i == g_gs.sel;
		if (sel)
			draw_rrect(G_LIST_X + 6, y + 2, G_LIST_W - 12, G_LIST_ROW_H - 4, 10,
				   foc ? theme_accent_soft() : COL_PANEL);
		if (sel && foc)
			draw_rrect_outline(G_LIST_X + 6, y + 2, G_LIST_W - 12, G_LIST_ROW_H - 4, 10, 2, theme_accent());
		unsigned int c = theme_graph_colour(i);
		bool vis = f->on && f->e;
		int mid = y + G_LIST_ROW_H / 2;
		if (vis)
			draw_rrect(G_LIST_X + 16, mid - 9, 6, 18, 3, c);
		else
			draw_rrect_outline(G_LIST_X + 15, mid - 10, 8, 20, 3, 1, f->src[0] ? c : COL_LINE);
		char lab[8];
		snprintf(lab, sizeof lab, "y%d", i + 1);
		const MLayout *lt = mlc_text(lab, 17, 1);
		mdraw(lt, G_LIST_X + 32, mid + 6, vis ? COL_TEXT2 : COL_DIM);
		font_draw_base(FW_REG, G_LIST_X + 58, mid + 6, COL_DIM, 17, "=");
		int fx = G_LIST_X + 76, fw = G_LIST_W - 76 - 14;
		if (!f->src[0]) {
			font_draw_base(FW_REG, fx, mid + 6, COL_DIM, 15, sel && foc ? "press to enter" : "");
			continue;
		}
		if (!f->e) {
			const MLayout *l = mlc_text(f->src, 15, 0);
			draw_clip(fx, y, fw, G_LIST_ROW_H);
			mdraw(l, fx, mid - 2, COL_MUTED);
			draw_clip_off();
			font_draw(fx, mid + 6, COL_ERR, 12, f->err);
			continue;
		}
		const MLayout *l = mlc_fit(f->src, 20, 12, fw);
		int base = mid + (l->asc - l->desc) / 2;
		draw_clip(fx, y + 2, fw, G_LIST_ROW_H - 4);
		mdraw(l, fx, base, f->on ? COL_TEXT : COL_DIM);
		draw_clip_off();
	}
	/* the view, in numbers */
	char a[24], b[24], line[64];
	g_fmt(g_px_to_x(0), a, sizeof a);
	g_fmt(g_px_to_x(G_PW), b, sizeof b);
	snprintf(line, sizeof line, "x  %s  to  %s", a, b);
	font_draw(G_LIST_X + 16, G_LIST_Y + G_LIST_H - 46, COL_DIM, 13, line);
	g_fmt(g_py_to_y(G_PH), a, sizeof a);
	g_fmt(g_py_to_y(0), b, sizeof b);
	snprintf(line, sizeof line, "y  %s  to  %s", a, b);
	font_draw(G_LIST_X + 16, G_LIST_Y + G_LIST_H - 26, COL_DIM, 13, line);
}

static void grid(void)
{
	double step = nice_step(72.0 / g_gs.scale);
	double minor = step / 5;
	bool show_minor = minor * g_gs.scale >= 9;
	double x0 = g_px_to_x(0), x1 = g_px_to_x(G_PW);
	double y0 = g_py_to_y(G_PH), y1 = g_py_to_y(0);
	if (show_minor) {
		for (double v = floor(x0 / minor) * minor, g_ = 0; v <= x1 && g_ < 300; v += minor, g_++)
			draw_rect(G_PX + (int)lround(g_x_to_px(v)), G_PY, 1, G_PH, RGBA8(255, 255, 255, 9));
		for (double v = floor(y0 / minor) * minor, g_ = 0; v <= y1 && g_ < 300; v += minor, g_++)
			draw_rect(G_PX, G_PY + (int)lround(g_y_to_py(v)), G_PW, 1, RGBA8(255, 255, 255, 9));
	}
	for (double v = floor(x0 / step) * step, g_ = 0; v <= x1 && g_ < 300; v += step, g_++)
		draw_rect(G_PX + (int)lround(g_x_to_px(v)), G_PY, 1, G_PH, RGBA8(255, 255, 255, 22));
	for (double v = floor(y0 / step) * step, g_ = 0; v <= y1 && g_ < 300; v += step, g_++)
		draw_rect(G_PX, G_PY + (int)lround(g_y_to_py(v)), G_PW, 1, RGBA8(255, 255, 255, 22));

	/* axes (pinned to the edge when off screen, for the labels) */
	int ax = (int)lround(g_x_to_px(0)), ay = (int)lround(g_y_to_py(0));
	bool ax_in = ax >= 0 && ax <= G_PW, ay_in = ay >= 0 && ay <= G_PH;
	if (ax_in)
		draw_rect(G_PX + ax, G_PY, 2, G_PH, COL_MUTED);
	if (ay_in)
		draw_rect(G_PX, G_PY + ay, G_PW, 2, COL_MUTED);
	int ly = ay_in ? ay : (ay < 0 ? 0 : G_PH - 20);
	if (ly > G_PH - 20) ly = G_PH - 20;
	int lx = ax_in ? ax : (ax < 0 ? 0 : G_PW - 4);
	char buf[24];
	for (double v = floor(x0 / step) * step, g_ = 0; v <= x1 && g_ < 300; v += step, g_++) {
		double r = round(v / step) * step;
		if (fabs(r) < step / 2)
			continue;
		int px = (int)lround(g_x_to_px(r));
		if (px < 14 || px > G_PW - 14)
			continue;
		if (ay_in)
			draw_rect(G_PX + px, G_PY + ay - 4, 1, 10, COL_MUTED);
		g_fmt(r, buf, sizeof buf);
		font_draw_c(FW_REG, G_PX + px, G_PY + ly + 6, COL_MUTED, 13, buf);
	}
	for (double v = floor(y0 / step) * step, g_ = 0; v <= y1 && g_ < 300; v += step, g_++) {
		double r = round(v / step) * step;
		if (fabs(r) < step / 2)
			continue;
		int py = (int)lround(g_y_to_py(r));
		if (py < 20 || py > G_PH - 12)
			continue;
		if (ax_in)
			draw_rect(G_PX + ax - 4, G_PY + py, 10, 1, COL_MUTED);
		g_fmt(r, buf, sizeof buf);
		if (lx > G_PW - 60)
			font_draw_r(FW_REG, G_PX + lx - 10, G_PY + py - 17, COL_MUTED, 13, buf);
		else
			font_draw(G_PX + lx + 11, G_PY + py - 17, COL_MUTED, 13, buf);
	}
	if (ax_in && ay_in)
		font_draw(G_PX + ax + 6, G_PY + ay + 4, COL_MUTED, 13, "0");
	font_draw_r(FW_MED, G_PX + G_PW - 8, G_PY + (ay_in ? ay : G_PH - 20) - 20, COL_TEXT2, 15, "x");
	font_draw(G_PX + (ax_in ? ax : 0) + 8, G_PY + 4, COL_TEXT2, 15, "y");
}

/* clip a segment to the band top..bot (y grows down); false when outside */
static bool clip_y(double *x0, double *y0, double *x1, double *y1, double top, double bot)
{
	if ((*y0 < top && *y1 < top) || (*y0 > bot && *y1 > bot))
		return false;
	double dx = *x1 - *x0, dy = *y1 - *y0;
	if (*y0 < top) { *x0 += dx * (top - *y0) / dy; *y0 = top; }
	else if (*y0 > bot) { *x0 += dx * (bot - *y0) / dy; *y0 = bot; }
	if (*y1 < top) { *x1 = *x0 + (*x1 - *x0) * (top - *y0) / (*y1 - *y0); *y1 = top; }
	else if (*y1 > bot) { *x1 = *x0 + (*x1 - *x0) * (bot - *y0) / (*y1 - *y0); *y1 = bot; }
	return true;
}

static void curve(int fn)
{
	g_samples(fn);
	const float *ys = g_gs.fn[fn].ys;
	if (!ys)
		return;
	unsigned int c = theme_graph_colour(fn);
	float w = fn == g_gs.sel && g_gs.mode != GM_NONE ? 3.4f : 2.6f;
	for (int i = 0; i < G_PW; i++) {
		if (isnan(ys[i]) || isnan(ys[i + 1]))
			continue;
		double py0 = g_y_to_py(ys[i]), py1 = g_y_to_py(ys[i + 1]);
		if (fabs(py1 - py0) > G_PH / 2.0) {
			/* steep: check the midpoint lies between, else it is an asymptote */
			double ym;
			if (!g_eval(fn, g_px_to_x(i + 0.5), &ym))
				continue;
			double lo = fmin(ys[i], ys[i + 1]), hi = fmax(ys[i], ys[i + 1]);
			if (ym < lo || ym > hi)
				continue;
		}
		double x0 = i, x1 = i + 1;
		if (!clip_y(&x0, &py0, &x1, &py1, -4, G_PH + 4))
			continue;
		draw_line_w((float)(G_PX + x0), (float)(G_PY + py0), (float)(G_PX + x1), (float)(G_PY + py1), w, c);
	}
}

static void trace_marker(void)
{
	int fn = g_gs.sel;
	double x = g_gs.trace_x, y;
	int px = (int)lround(g_x_to_px(x));
	for (int py = 0; py < G_PH; py += 8)
		draw_rect(G_PX + px, G_PY + py, 1, 4, COL_DIM);
	char xs[24], ys[24], line[64];
	g_fmt(x, xs, sizeof xs);
	bool ok = g_eval(fn, x, &y);
	if (ok) {
		g_fmt(y, ys, sizeof ys);
		double py = g_y_to_py(y);
		if (py >= 0 && py <= G_PH) {
			draw_disc((float)(G_PX + px), (float)(G_PY + py), 7, COL_BG);
			draw_disc((float)(G_PX + px), (float)(G_PY + py), 5, theme_graph_colour(fn));
		}
	}
	snprintf(line, sizeof line, "x = %s    y = %s", xs, ok ? ys : "undefined");
	int bw = font_width_w(FW_MED, 16, line) + 56;
	draw_rrect(G_PX + 12, G_PY + 12, bw, 34, 10, COL_PANEL2);
	draw_rrect(G_PX + 22, G_PY + 23, 12, 12, 3, theme_graph_colour(fn));
	font_draw_w(FW_MED, G_PX + 42, G_PY + 20, COL_TEXT, 16, line);
}

void g_draw_plot(void)
{
	bool foc = g_gs.focus == GF_PLOT;
	draw_rrect(G_PX, G_PY, G_PW, G_PH, 14, RGBA8(10, 12, 17, 255));
	if (g_gs.mode == GM_TABLE) {
		g_table_draw();
		return;
	}
	draw_clip(G_PX + 1, G_PY + 1, G_PW - 2, G_PH - 2);
	grid();
	bool any = false;
	for (int i = 0; i < G_NFN; i++)
		if (g_fn_visible(i)) {
			curve(i);
			any = true;
		}
	if (g_gs.mode == GM_ANALYSIS)
		g_points_draw();
	if (g_gs.mode == GM_TRACE)
		trace_marker();
	draw_clip_off();
	if (!any)
		font_draw_c(FW_MED, G_PX + G_PW / 2, G_PY + G_PH / 2 + 30, COL_DIM, 16,
			    "Pick y1 on the left and press the confirm button to enter a function");
	draw_rrect_outline(G_PX, G_PY, G_PW, G_PH, 14, foc ? 2 : 1, foc ? theme_accent() : COL_LINE);
	if (g_gs.mode == GM_ANALYSIS)
		g_analysis_draw();
}
