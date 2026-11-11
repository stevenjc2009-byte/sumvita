/* graph_table.c - table of values for every visible function, drawn in the
 * plot area. Up/down scrolls x, left/right changes the step. */
#include "ui/graph_int.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/theme.h"

#include <math.h>
#include <stdio.h>

#define TB_HEAD 64
#define TB_ROW_H 30
#define TB_ROWS ((G_PH - TB_HEAD - 12) / TB_ROW_H)
#define TB_XW 120

static const double k_steps[] = { 0.01, 0.1, 0.25, 0.5, 1, 2, 5, 10, 100 };
#define N_STEPS ((int)(sizeof k_steps / sizeof k_steps[0]))

static float g_drag;

void g_table_open(void)
{
	if (g_gs.tab_step_i < 0 || g_gs.tab_step_i >= N_STEPS)
		g_gs.tab_step_i = 4;
	g_gs.tab_step = k_steps[g_gs.tab_step_i];
	g_gs.tab_x0 = round(g_gs.cx / g_gs.tab_step) * g_gs.tab_step - 5 * g_gs.tab_step;
	g_drag = 0;
}

void g_table_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_DPAD, "Scroll / step" }, { GLYPH_TOUCH, "Drag" }, { GLYPH_BACK, "Back to graph" },
		{ GLYPH_LR, "Mode" },
	};
	app_hints(hints, 4);
	if (in->pressed & (IN_BACK | IN_START)) {
		g_gs.mode = GM_NONE;
		return;
	}
	double st = g_gs.tab_step;
	if (in->pressed & IN_DOWN) g_gs.tab_x0 += st;
	if (in->pressed & IN_UP) g_gs.tab_x0 -= st;
	g_drag += input_rstick_y(in) * 0.3f + input_lstick_y(in) * 0.3f;
	if (in->dragging)
		g_drag -= (float)in->drag_dy / TB_ROW_H;
	while (g_drag >= 1) { g_gs.tab_x0 += st; g_drag -= 1; }
	while (g_drag <= -1) { g_gs.tab_x0 -= st; g_drag += 1; }
	int d = (in->pressed & IN_RIGHT) ? 1 : (in->pressed & IN_LEFT) ? -1 : 0;
	if (d && g_gs.tab_step_i + d >= 0 && g_gs.tab_step_i + d < N_STEPS) {
		double mid = g_gs.tab_x0 + 5 * st;
		g_gs.tab_step_i += d;
		g_gs.tab_step = k_steps[g_gs.tab_step_i];
		g_gs.tab_x0 = round(mid / g_gs.tab_step) * g_gs.tab_step - 5 * g_gs.tab_step;
	}
}

void g_table_draw(void)
{
	int cols[G_NFN], nc = 0;
	for (int i = 0; i < G_NFN; i++)
		if (g_fn_visible(i))
			cols[nc++] = i;
	int x = G_PX + 12;
	int cw = nc ? (G_PW - 24 - TB_XW) / nc : 0;
	font_draw_w(FW_BOLD, x + 6, G_PY + 12, COL_TEXT, 18, "Table of values");
	char sb[48], v[24];
	g_fmt(g_gs.tab_step, v, sizeof v);
	snprintf(sb, sizeof sb, "step %s", v);
	int sw = font_width_w(FW_MED, 15, sb) + 60;
	int sx = G_PX + G_PW - 14 - sw;
	draw_rrect(sx, G_PY + 10, sw, 28, 14, COL_PANEL2);
	draw_glyph(GLYPH_DPAD, sx + 10, G_PY + 15);
	font_draw_w(FW_MED, sx + 36, G_PY + 15, COL_TEXT2, 15, sb);

	int hy = G_PY + TB_HEAD - 22;
	draw_rect(x, G_PY + TB_HEAD - 2, G_PW - 24, 1, COL_LINE);
	font_draw_c(FW_BOLD, x + TB_XW / 2, hy, COL_MUTED, 16, "x");
	for (int c = 0; c < nc; c++) {
		char h[16];
		snprintf(h, sizeof h, "y%d", cols[c] + 1);
		int cx = x + TB_XW + c * cw + cw / 2;
		draw_rrect(cx - 26, hy + 6, 8, 8, 2, theme_graph_colour(cols[c]));
		font_draw_c(FW_BOLD, cx + 4, hy, COL_TEXT2, 16, h);
	}
	if (!nc) {
		font_draw_c(FW_REG, G_PX + G_PW / 2, G_PY + G_PH / 2, COL_DIM, 16, "No functions to tabulate");
		return;
	}
	for (int r = 0; r < TB_ROWS; r++) {
		double xv = g_gs.tab_x0 + r * g_gs.tab_step;
		if (fabs(xv) < g_gs.tab_step * 1e-6)
			xv = 0;
		int y = G_PY + TB_HEAD + r * TB_ROW_H;
		if (r & 1)
			draw_rect(x, y, G_PW - 24, TB_ROW_H, RGBA8(255, 255, 255, 6));
		g_fmt(xv, v, sizeof v);
		font_draw_c(FW_MED, x + TB_XW / 2, y + 6, COL_TEXT2, 16, v);
		for (int c = 0; c < nc; c++) {
			double yv;
			bool ok = g_eval(cols[c], xv, &yv);
			if (ok)
				g_fmt(yv, v, sizeof v);
			font_draw_c(FW_REG, x + TB_XW + c * cw + cw / 2, y + 6, ok ? COL_TEXT : COL_DIM, 16,
				    ok ? v : "undefined");
		}
	}
	draw_rect(x + TB_XW - 1, G_PY + TB_HEAD, 1, TB_ROWS * TB_ROW_H, COL_LINE_SOFT);
	draw_rrect_outline(G_PX, G_PY, G_PW, G_PH, 14, 2, theme_accent());
}
