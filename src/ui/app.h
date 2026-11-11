/* app.h - the app shell: mode tabs, overlays, hint bar, and the per-screen
 * entry points every screen file implements. Screen size 960x544. */
#ifndef UI_APP_H
#define UI_APP_H

#include <stdbool.h>

#include "ui/input.h"
#include "ui/result.h"

#define SCR_W 960
#define SCR_H 544
#define TOP_H 46
#define HINT_H 30
#define BODY_Y TOP_H
#define BODY_H (SCR_H - TOP_H - HINT_H)

enum { MODE_CALC = 0, MODE_SCI, MODE_ALG, MODE_GRAPH, MODE_GCSE, MODE_COUNT };

/* hint bar: glyph is a glyph_t (draw.h) or -1 for text only */
typedef struct {
	int glyph;
	const char *text;
} Hint;

void app_init(void);
void app_update(const input_t *in);
void app_draw(void);
bool app_quit_requested(void);

int  app_mode(void);
void app_set_mode(int mode);
const char *app_mode_name(int mode);
void app_hints(const Hint *h, int n);   /* the active screen calls this every frame */
void app_toast(const char *msg);
uint64_t app_now(void);

/* overlays (drawn over the current screen, they take the input) */
void app_open_steps(const Res *r);
typedef void (*sheet_done_fn)(const char *text, void *ctx);
void app_open_sheet(const char *title, const char *hint, const char *initial, sheet_done_fn done, void *ctx);
void app_open_settings(void);
void app_open_about(void);
bool app_overlay_open(void);
void app_close_overlays(void);
void app_request_quit(void);

/* chrome.c */
void topbar_draw(int mode);
int  topbar_hit(int tx, int ty);        /* mode tab, MODE_COUNT = settings gear, -1 none */
void hintbar_draw(const Hint *h, int n);
void toast_draw(const char *msg, int alpha);
void panel_title(int x, int y, const char *title, const char *sub);
/* A rounded pill button; returns its width. style 0 plain, 1 accent, 2 subtle */
int  button_draw(int x, int y, int w, int h, const char *label, int style, bool focus, bool pressed);

/* screens: calc modes (calcview*.c) */
void calc_screen_init(void);
void calc_screen_update(int mode, const input_t *in);
void calc_screen_draw(int mode);
void calc_screen_set_input(int mode, const char *display);
void calc_screen_eval(int mode, int action);
void calc_screen_key(int mode, int key_index);

/* graph (graph*.c) */
void graph_screen_init(void);
void graph_screen_update(const input_t *in);
void graph_screen_draw(void);
void graph_set_fn(int i, const char *display);
void graph_tour(int what);              /* 0 plot, 1 analysis menu, 2 table, 3 trace */
bool graph_claims_lr(void);             /* plot focus: L/R zoom instead of switching tabs */

/* GCSE tools (tools_view*.c) */
void gcse_screen_init(void);
void gcse_screen_update(const input_t *in);
void gcse_screen_draw(void);
void gcse_tour(int what);               /* 0 list, 1 form, 2 result */

/* overlays */
void steps_view_open(const Res *r);
bool steps_view_update(const input_t *in);   /* false = closed */
void steps_view_draw(void);
void sheet_open(const char *title, const char *hint, const char *initial, sheet_done_fn done, void *ctx);
bool sheet_update(const input_t *in);
void sheet_draw(void);
void settings_view_open(void);
bool settings_view_update(const input_t *in);
void settings_view_draw(void);
void about_open(void);
bool about_update(const input_t *in);
void about_draw(void);

/* tour.c: scripted screenshots when ux0:data/Sumvita/tour.txt exists */
void tour_start_if_requested(void);
bool tour_active(void);
void tour_step(void);
void tour_input(input_t *in);           /* merges scripted presses/touches; inert without a tour */
bool tour_osk_frame(int frame);         /* osk.c, each keyboard frame: true = close it now */

#endif
