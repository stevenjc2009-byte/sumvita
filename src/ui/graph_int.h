/* graph_int.h - state shared by the graph files: graph.c (functions, view,
 * input), graph_draw.c (grid, axes, curves, list), graph_analysis.c (roots,
 * intercepts, turning points, intersections) and graph_table.c. */
#ifndef UI_GRAPH_INT_H
#define UI_GRAPH_INT_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/expr.h"
#include "ui/app.h"

#define G_NFN 6
#define G_SRC_MAX 256

/* layout */
#define G_LIST_X 10
#define G_LIST_Y (BODY_Y + 8)
#define G_LIST_W 268
#define G_LIST_H (BODY_H - 16)
#define G_LIST_ROW_Y (G_LIST_Y + 46)
#define G_LIST_ROW_H 58
#define G_PX 288
#define G_PY (BODY_Y + 8)
#define G_PW (SCR_W - G_PX - 10)
#define G_PH (BODY_H - 16)

enum { GF_LIST = 0, GF_PLOT };
enum { GM_NONE = 0, GM_TRACE, GM_ANALYSIS, GM_TABLE };

typedef struct {
	char src[G_SRC_MAX];      /* display syntax as typed (no "y=") */
	Arena *ar;
	Expr *e;                  /* NULL when empty or invalid */
	char err[64];
	bool on;
	float *ys;                /* one sample per plot column, NAN = undefined */
	uint32_t ys_ver;          /* view version the samples belong to */
} GFn;

typedef enum { GP_ROOT = 0, GP_YINT, GP_MIN, GP_MAX, GP_ISECT } GPKind;

typedef struct {
	GPKind kind;
	int fn, other;            /* other: intersecting function (GP_ISECT) */
	double x, y;
} GPoint;

#define G_MAX_POINTS 40

typedef struct {
	GFn fn[G_NFN];
	int focus;                /* GF_LIST / GF_PLOT */
	int mode;                 /* GM_* in plot focus */
	int sel;                  /* selected function (list, trace, analysis) */
	double cx, cy;            /* view centre (maths units) */
	double scale;             /* pixels per unit */
	uint32_t ver;             /* bumps on any view or function change */
	double trace_x;
	GPoint pts[G_MAX_POINTS];
	int npts;
	int pt_sel;
	int pt_scroll;
	double tab_x0, tab_step;
	int tab_step_i;
} GraphState;

extern GraphState g_gs;

/* graph.c */
bool g_eval(int fn, double x, double *y);
double g_px_to_x(double px);      /* plot-local px -> x */
double g_x_to_px(double x);
double g_y_to_py(double y);
double g_py_to_y(double py);
void g_fmt(double v, char *buf, int cap);     /* tidy number, U+2212 minus */
void g_view_changed(void);
void g_samples(int fn);           /* refresh fn's column samples if stale */
bool g_fn_visible(int fn);

/* graph_draw.c */
void g_draw_list(void);
void g_draw_plot(void);

/* graph_analysis.c */
void g_analyse(int fn);
const char *g_point_name(const GPoint *p, char *buf, int cap);
void g_analysis_update(const input_t *in);
void g_analysis_draw(void);
void g_points_draw(void);

/* graph_table.c */
void g_table_open(void);
void g_table_update(const input_t *in);
void g_table_draw(void);

#endif
