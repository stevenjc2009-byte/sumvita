/* tour.c - scripted walk through the app for screenshots. Runs only when
 * ux0:data/Sumvita/tour.txt exists (the file is deleted so it runs once).
 * An empty tour.txt runs the built-in walk below; otherwise its lines are a
 * script (tour_script.c, commands in tour_cmds.c) that can also press
 * virtual buttons and tap the screen (tour_input.c).
 * Each shot logs "TOUR shot NAME" once the screen has settled; a PC-side
 * script watching log.txt takes the emulator screenshot and then creates
 * tour.ack, which lets the tour go on (or it goes on after HOLD frames).
 * Ends with "TOUR done", the user's settings restored, and a clean exit. */
#include "ui/app.h"

#include "ui/calcview_int.h"
#include "ui/history.h"
#include "ui/log.h"
#include "ui/perf.h"
#include "ui/settings.h"
#include "ui/tour_int.h"
#include "ui/tools_view_int.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOUR_FILE SUMVITA_DATA_DIR "/tour.txt"
#define ACK_FILE SUMVITA_DATA_DIR "/tour.ack"
#define SETTLE 30     /* frames before the shot is announced */
#define HOLD 600      /* most frames the screen waits for the PC's ack */

static bool g_on, g_script, g_done;
static int g_idx, g_frame;
static UiSettings g_saved;
/* shot in progress */
static char g_shot[48];
static int g_shot_t;   /* frames since tour_shot_begin, 0 = none */

static void s_calc(void)
{
	app_close_overlays();
	app_set_mode(MODE_CALC);
	calc_screen_set_input(MODE_CALC, "2^10");
	calc_screen_eval(MODE_CALC, ACT_AUTO);
	calc_screen_set_input(MODE_CALC, "√(48)");
	calc_screen_eval(MODE_CALC, ACT_AUTO);
	calc_screen_set_input(MODE_CALC, "3/4+1/6");
	calc_screen_eval(MODE_CALC, ACT_AUTO);
}

static void s_sci(void)
{
	app_set_mode(MODE_SCI);
	calc_screen_set_input(MODE_SCI, "sin(30)+2^3");
	calc_screen_eval(MODE_SCI, ACT_AUTO);
}

static void s_editor(void)
{
	calc_screen_set_input(MODE_SCI, "(1+√(5))/2+3^(2x)-root(3,27)+abs(-4)+2_1/3");
	g_cs[MODE_SCI].focus = FOC_EDITOR;
}

static void s_alg(void)
{
	app_set_mode(MODE_ALG);
	calc_screen_set_input(MODE_ALG, "x^2+5x+6=0");
	calc_screen_eval(MODE_ALG, ACT_SOLVE);
}

static void s_steps(void)
{
	app_open_steps(&g_cs[MODE_ALG].cur);
}

static void s_graph(void)
{
	app_close_overlays();
	app_set_mode(MODE_GRAPH);
	graph_set_fn(0, "x^2-2x-3");
	graph_set_fn(1, "2x-1");
	graph_tour(0);
}

static void s_trace(void) { graph_tour(3); }
static void s_analysis(void) { graph_tour(1); }
static void s_table(void) { graph_tour(2); }

static void s_sheet(void)
{
	graph_tour(0);
	app_open_sheet("y1 =", "A function of x, e.g. x^2 - 3x + 2 or sin(x)", "x^2-2x-3", NULL, NULL);
}

static void s_gcse_list(void)
{
	app_close_overlays();
	app_set_mode(MODE_GCSE);
	gcse_tour(0);
}

static void s_gcse_form(void) { gcse_tour(1); }
static void s_gcse_result(void) { gcse_tour(2); }

static void s_gcse_steps(void)
{
	if (g_tv.has_res)
		app_open_steps(&g_tv.res);
}

static void s_settings(void)
{
	app_close_overlays();
	app_set_mode(MODE_CALC);
	app_open_settings();
}

static void s_about(void) { app_open_about(); }

static const struct {
	const char *name;
	void (*fn)(void);
} k_stops[] = {
	{ "calc", s_calc },
	{ "scientific", s_sci },
	{ "editor", s_editor },
	{ "algebra", s_alg },
	{ "steps", s_steps },
	{ "graph", s_graph },
	{ "graph_trace", s_trace },
	{ "graph_analysis", s_analysis },
	{ "graph_table", s_table },
	{ "sheet", s_sheet },
	{ "gcse_list", s_gcse_list },
	{ "gcse_form", s_gcse_form },
	{ "gcse_result", s_gcse_result },
	{ "gcse_steps", s_gcse_steps },
	{ "settings", s_settings },
	{ "about", s_about },
};
#define N_STOPS ((int)(sizeof k_stops / sizeof k_stops[0]))

void tour_shot_begin(const char *name)
{
	snprintf(g_shot, sizeof g_shot, "%s", name);
	g_shot_t = 1;
}

static bool ack_seen(void)
{
	SceIoStat st;
	if (sceIoGetstat(ACK_FILE, &st) < 0)
		return false;
	sceIoRemove(ACK_FILE);
	return true;
}

/* true while a shot holds the tour */
static bool shot_tick(void)
{
	if (!g_shot_t)
		return false;
	int t = g_shot_t++;
	if (t == SETTLE) {
		sceIoRemove(ACK_FILE);
		perf_frame_t pf = perf_last();
		log_printf("PERF %s draws=%u glyphs=%u cpu_us=%u", g_shot, pf.draws, pf.glyphs, pf.cpu_us);
		log_printf("TOUR shot %s", g_shot);
	} else if (t > SETTLE && ((t - SETTLE) % 4 == 0 && ack_seen())) {
		g_shot_t = 0;
	} else if (t > SETTLE + HOLD) {
		log_printf("TOUR no ack for %s", g_shot);
		g_shot_t = 0;
	}
	return g_shot_t != 0;
}

static char *read_all(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *b = n >= 0 ? malloc((size_t)n + 1) : NULL;
	if (b) {
		size_t got = fread(b, 1, (size_t)n, f);
		b[got] = 0;
	}
	fclose(f);
	return b;
}

void tour_start_if_requested(void)
{
	SceIoStat st;
	if (sceIoGetstat(TOUR_FILE, &st) < 0)
		return;
	char *text = read_all(TOUR_FILE);
	sceIoRemove(TOUR_FILE);
	sceIoRemove(ACK_FILE);
	g_on = true;
	g_idx = 0;
	g_frame = 0;
	g_saved = g_set;
	g_script = text && strspn(text, " \t\r\n") < strlen(text);
	if (g_script && ts_load(text)) {
		log_printf("TOUR start (script)");
		return;
	}
	free(text);
	g_script = false;
	log_printf("TOUR start (%d stops)", N_STOPS);
}

bool tour_active(void) { return g_on; }

void tour_finish(void)
{
	if (g_done)
		return;
	g_done = true;
	g_frame = 0;
	g_set = g_saved;
	settings_save();
	log_printf("TOUR done");
}

/* called by osk.c every frame the system keyboard is up: one shot of it, then
 * true = close it (the tour cannot type into the system dialog) */
bool tour_osk_frame(int frame)
{
	if (!g_on)
		return false;
	if (frame == 0)
		tour_shot_begin("osk");
	return !shot_tick();
}

void tour_step(void)
{
	if (!g_on)
		return;
	if (g_done) {
		if (++g_frame > 30) {
			g_on = false;
			app_request_quit();
		}
		return;
	}
	if (shot_tick() || !tq_idle())
		return;
	if (g_script) {
		if (!ts_step())
			tour_finish();
		return;
	}
	if (g_idx >= N_STOPS) {
		tour_finish();
		return;
	}
	k_stops[g_idx].fn();
	tour_shot_begin(k_stops[g_idx].name);
	g_idx++;
}
