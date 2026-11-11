/* graph.c - Graph mode: up to six functions of x, each parsed once into its
 * own arena and sampled per plot column only when the view or the function
 * changes. Pan (d-pad, left stick, drag), zoom (L/R, pinch), reset (Square). */
#include "ui/graph_int.h"

#include "ui/draw.h"
#include "ui/log.h"
#include "ui/settings.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/num.h"

GraphState g_gs;

#define G_SCALE0 32.0

bool g_eval(int fn, double x, double *y)
{
	const GFn *f = &g_gs.fn[fn];
	if (!f->e)
		return false;
	EvalEnv env;
	memset(&env, 0, sizeof env);
	env.degrees = g_set.degrees;
	env.var_names[0] = "x";
	env.var_values[0] = x;
	env.nvars = 1;
	return ex_eval(f->e, &env, y);
}

double g_px_to_x(double px) { return g_gs.cx + (px - G_PW / 2.0) / g_gs.scale; }
double g_x_to_px(double x) { return (x - g_gs.cx) * g_gs.scale + G_PW / 2.0; }
double g_y_to_py(double y) { return G_PH / 2.0 - (y - g_gs.cy) * g_gs.scale; }
double g_py_to_y(double py) { return g_gs.cy - (py - G_PH / 2.0) / g_gs.scale; }

void g_fmt(double v, char *buf, int cap)
{
	if (fabs(v) < 1e-9)
		v = 0;
	char t[48];
	dbl_to_str(v, 6, t, (int)sizeof t);
	if (t[0] == '-')
		snprintf(buf, (size_t)cap, "\xE2\x88\x92%s", t + 1);
	else
		snprintf(buf, (size_t)cap, "%s", t);
}

void g_view_changed(void)
{
	g_gs.ver++;
	if (g_gs.mode == GM_ANALYSIS)
		g_gs.mode = GM_NONE;
}

bool g_fn_visible(int fn)
{
	return g_gs.fn[fn].on && g_gs.fn[fn].e;
}

void g_samples(int fn)
{
	GFn *f = &g_gs.fn[fn];
	if (!f->ys)
		f->ys = malloc(sizeof(float) * (G_PW + 1));
	if (!f->ys || f->ys_ver == g_gs.ver)
		return;
	f->ys_ver = g_gs.ver;
	for (int i = 0; i <= G_PW; i++) {
		double y;
		f->ys[i] = g_eval(fn, g_px_to_x(i), &y) ? (float)y : NAN;
	}
}

static void reset_view(void)
{
	g_gs.cx = 0;
	g_gs.cy = 0;
	g_gs.scale = G_SCALE0;
	g_view_changed();
}

void graph_screen_init(void)
{
	memset(&g_gs, 0, sizeof g_gs);
	for (int i = 0; i < G_NFN; i++) {
		g_gs.fn[i].ar = arena_new(32 * 1024);
		g_gs.fn[i].on = true;
		g_gs.fn[i].ys_ver = 0xFFFFFFFFu;
	}
	g_gs.tab_step = 1;
	g_gs.tab_step_i = 3;
	reset_view();
}

void graph_set_fn(int i, const char *display)
{
	if (i < 0 || i >= G_NFN)
		return;
	GFn *f = &g_gs.fn[i];
	while (*display == ' ')
		display++;
	/* accept "y=..." as typed on most calculators */
	if (display[0] == 'y' && display[1] == '=')
		display += 2;
	snprintf(f->src, sizeof f->src, "%s", display);
	f->e = NULL;
	f->err[0] = 0;
	arena_reset(f->ar);
	if (f->src[0]) {
		Arena *prev = arena_use(f->ar);
		int pos = 0;
		Expr *e = ex_parse(f->src, f->err, (int)sizeof f->err, &pos);
		if (e && e->kind == E_REL) {
			snprintf(f->err, sizeof f->err, "Type just the right-hand side, in x");
			e = NULL;
		}
		if (e) {
			char names[8][12];
			int nv = ex_vars(e, names, 8);
			for (int k = 0; k < nv; k++)
				if (strcmp(names[k], "x") != 0) {
					snprintf(f->err, sizeof f->err, "Only x can be used (found %.11s)", names[k]);
					e = NULL;
					break;
				}
		}
		f->e = e;
		arena_use(prev);
		f->on = true;
	}
	log_printf("graph y%d = %s %s", i + 1, f->src, f->e ? "ok" : f->err);
	g_view_changed();
}

static void edit_done(const char *text, void *ctx)
{
	graph_set_fn((int)(intptr_t)ctx, text);
}

static void edit_fn(int i)
{
	char title[16];
	snprintf(title, sizeof title, "y%d =", i + 1);
	app_open_sheet(title, "A function of x, e.g. x^2 - 3x + 2 or sin(x)", g_gs.fn[i].src, edit_done,
		       (void *)(intptr_t)i);
}

static void zoom_at(double f, double px, double py)
{
	double ns = g_gs.scale * f;
	if (ns < 0.002 || ns > 200000)
		return;
	/* keep the maths point under (px, py) fixed */
	double mx = g_px_to_x(px), my = g_py_to_y(py);
	g_gs.scale = ns;
	g_gs.cx = mx - (px - G_PW / 2.0) / ns;
	g_gs.cy = my + (py - G_PH / 2.0) / ns;
	g_view_changed();
}

static void pan(double dpx, double dpy)
{
	if (dpx == 0 && dpy == 0)
		return;
	g_gs.cx += dpx / g_gs.scale;
	g_gs.cy -= dpy / g_gs.scale;
	g_view_changed();
}

static void list_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_DPAD, "Choose" },     { GLYPH_CONFIRM, "Edit" },   { GLYPH_TRIANGLE, "Show / hide" },
		{ GLYPH_SQUARE, "Clear" },    { GLYPH_START, "To graph" }, { GLYPH_LR, "Mode" },
	};
	app_hints(hints, 6);
	if (in->pressed & IN_UP) g_gs.sel = (g_gs.sel + G_NFN - 1) % G_NFN;
	if (in->pressed & IN_DOWN) g_gs.sel = (g_gs.sel + 1) % G_NFN;
	if (in->pressed & IN_CONFIRM) edit_fn(g_gs.sel);
	if (in->pressed & IN_TRIANGLE) {
		g_gs.fn[g_gs.sel].on = !g_gs.fn[g_gs.sel].on;
		g_view_changed();
	}
	if (in->pressed & IN_SQUARE) graph_set_fn(g_gs.sel, "");
	if (in->pressed & (IN_RIGHT | IN_START)) g_gs.focus = GF_PLOT;
}

static void trace_update(const input_t *in)
{
	double step = 1.0 / g_gs.scale;
	if (in->pressed & IN_LEFT) g_gs.trace_x -= 2 * step;
	if (in->pressed & IN_RIGHT) g_gs.trace_x += 2 * step;
	g_gs.trace_x += input_lstick_x(in) * 4 * step;
	if (in->pressed & (IN_UP | IN_DOWN)) {
		int d = (in->pressed & IN_UP) ? G_NFN - 1 : 1;
		for (int k = 1; k <= G_NFN; k++) {
			int j = (g_gs.sel + d * k) % G_NFN;
			if (g_fn_visible(j)) { g_gs.sel = j; break; }
		}
	}
	double px = g_x_to_px(g_gs.trace_x);
	if (px < 20) pan(px - 20, 0);
	if (px > G_PW - 20) pan(px - (G_PW - 20), 0);
	if (in->tap && in_rect(in->tap_x, in->tap_y, G_PX, G_PY, G_PW, G_PH))
		g_gs.trace_x = g_px_to_x(in->tap_x - G_PX);
}

static int first_visible(void)
{
	if (g_fn_visible(g_gs.sel))
		return g_gs.sel;
	for (int i = 0; i < G_NFN; i++)
		if (g_fn_visible(i))
			return i;
	return -1;
}

static void open_mode(int m)
{
	int f = first_visible();
	if (f < 0) {
		app_toast("Enter a function first");
		return;
	}
	g_gs.sel = f;
	g_gs.mode = m;
	if (m == GM_TRACE) {
		g_gs.trace_x = g_gs.cx;
	} else if (m == GM_ANALYSIS) {
		g_analyse(f);
	} else if (m == GM_TABLE) {
		g_table_open();
	}
}

static void plot_update(const input_t *in)
{
	static const Hint h_plot[] = {
		{ GLYPH_DPAD, "Pan" },         { GLYPH_LR, "Zoom" },      { GLYPH_SQUARE, "Reset" },
		{ GLYPH_CONFIRM, "Analyse" }, { GLYPH_TRIANGLE, "Trace" }, { GLYPH_START, "Table" },
		{ GLYPH_BACK, "Functions" },
	};
	static const Hint h_trace[] = {
		{ GLYPH_DPAD, "Move / switch" }, { GLYPH_TRIANGLE, "Analyse" }, { GLYPH_BACK, "Stop tracing" },
	};
	if (g_gs.mode == GM_ANALYSIS) { g_analysis_update(in); return; }
	if (g_gs.mode == GM_TABLE) { g_table_update(in); return; }
	if (g_gs.mode == GM_TRACE) {
		app_hints(h_trace, 3);
		if (in->pressed & IN_BACK) { g_gs.mode = GM_NONE; return; }
		if (in->pressed & (IN_TRIANGLE | IN_CONFIRM)) { open_mode(GM_ANALYSIS); return; }
		trace_update(in);
	} else {
		app_hints(h_plot, 7);
		if (in->pressed & IN_BACK) { g_gs.focus = GF_LIST; return; }
		if (in->pressed & IN_CONFIRM) { open_mode(GM_ANALYSIS); return; }
		if (in->pressed & IN_TRIANGLE) { open_mode(GM_TRACE); return; }
		if (in->pressed & IN_START) { open_mode(GM_TABLE); return; }
		if (in->pressed & IN_SQUARE) reset_view();
		double dx = 0, dy = 0;
		if (in->pressed & IN_LEFT) dx -= 40;
		if (in->pressed & IN_RIGHT) dx += 40;
		if (in->pressed & IN_UP) dy -= 40;
		if (in->pressed & IN_DOWN) dy += 40;
		dx += input_lstick_x(in) * 9;
		dy += input_lstick_y(in) * 9;
		pan(dx, dy);
		if (in->dragging && in_rect(in->began_x, in->began_y, G_PX, G_PY, G_PW, G_PH))
			pan(-in->drag_dx, -in->drag_dy);
	}
	if (in->pressed & IN_L) zoom_at(1 / 1.5, G_PW / 2.0, G_PH / 2.0);
	if (in->pressed & IN_R) zoom_at(1.5, G_PW / 2.0, G_PH / 2.0);
	float ry = input_rstick_y(in);
	if (ry != 0)
		zoom_at(1.0 - ry * 0.03, G_PW / 2.0, G_PH / 2.0);
	if (in->pinch != 1.0f && in->pinch > 0.2f)
		zoom_at(in->pinch, in->pinch_cx - G_PX, in->pinch_cy - G_PY);
}

void graph_screen_update(const input_t *in)
{
	/* touch: a tap on a list row selects it (a second tap edits) */
	if (in->tap && in_rect(in->tap_x, in->tap_y, G_LIST_X, G_LIST_ROW_Y, G_LIST_W, G_NFN * G_LIST_ROW_H)) {
		int r = (in->tap_y - G_LIST_ROW_Y) / G_LIST_ROW_H;
		if (g_gs.focus == GF_LIST && r == g_gs.sel)
			edit_fn(r);
		g_gs.sel = r;
		g_gs.focus = GF_LIST;
		g_gs.mode = GM_NONE;
		return;
	}
	if (g_gs.focus == GF_LIST && (in->touch_began && in_rect(in->began_x, in->began_y, G_PX, G_PY, G_PW, G_PH)))
		g_gs.focus = GF_PLOT;
	if (g_gs.focus == GF_LIST)
		list_update(in);
	else
		plot_update(in);
}

/* graph mode claims L/R (zoom) while the plot has focus */
bool graph_claims_lr(void)
{
	return g_gs.focus == GF_PLOT && g_gs.mode != GM_TABLE;
}

void graph_screen_draw(void)
{
	g_draw_list();
	g_draw_plot();
}

void graph_tour(int what)
{
	g_gs.focus = GF_PLOT;
	switch (what) {
	case 0: g_gs.mode = GM_NONE; break;
	case 1: open_mode(GM_ANALYSIS); break;
	case 2: open_mode(GM_TABLE); break;
	case 3:
		open_mode(GM_TRACE);
		g_gs.trace_x = 1.5;
		break;
	default: break;
	}
}
