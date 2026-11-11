/* app.c - the shell: mode switching, overlay stack, hints, toast. */
#include "ui/app.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/log.h"
#include "ui/osk.h"
#include "ui/settings.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

enum { OV_NONE = 0, OV_STEPS, OV_SHEET, OV_SETTINGS, OV_ABOUT };

#define OV_MAX 4

static int g_mode = MODE_CALC;
static int g_ov[OV_MAX];
static int g_nov;
static Hint g_hints[10];
static int g_nhints;
static char g_toast[96];
static uint64_t g_toast_until;
static uint64_t g_now;

static const char *k_names[MODE_COUNT] = { "Calc", "Scientific", "Algebra", "Graph", "GCSE" };

const char *app_mode_name(int mode) { return mode >= 0 && mode < MODE_COUNT ? k_names[mode] : "?"; }
int app_mode(void) { return g_mode; }
uint64_t app_now(void) { return g_now; }
static bool g_quit;

bool app_quit_requested(void) { return g_quit; }
void app_request_quit(void) { g_quit = true; }
bool app_overlay_open(void) { return g_nov > 0; }
void app_close_overlays(void) { g_nov = 0; }

void app_set_mode(int mode)
{
	if (mode < 0 || mode >= MODE_COUNT || mode == g_mode)
		return;
	g_mode = mode;
	log_printf("mode %s", k_names[mode]);
}

void app_hints(const Hint *h, int n)
{
	if (n > 10)
		n = 10;
	memcpy(g_hints, h, sizeof(Hint) * (size_t)n);
	g_nhints = n;
}

void app_toast(const char *msg)
{
	snprintf(g_toast, sizeof g_toast, "%s", msg);
	g_toast_until = g_now + 1800;
}

static void push(int ov)
{
	if (g_nov < OV_MAX)
		g_ov[g_nov++] = ov;
}

void app_open_steps(const Res *r)
{
	steps_view_open(r);
	push(OV_STEPS);
}

void app_open_sheet(const char *title, const char *hint, const char *initial, sheet_done_fn done, void *ctx)
{
	sheet_open(title, hint, initial, done, ctx);
	push(OV_SHEET);
}

void app_open_settings(void)
{
	settings_view_open();
	push(OV_SETTINGS);
}

void app_open_about(void)
{
	about_open();
	push(OV_ABOUT);
}

static void draw_screen(void)
{
	switch (g_mode) {
	case MODE_GRAPH: graph_screen_draw(); break;
	case MODE_GCSE: gcse_screen_draw(); break;
	default: calc_screen_draw(g_mode); break;
	}
}

static void draw_overlay(int ov)
{
	switch (ov) {
	case OV_STEPS: steps_view_draw(); break;
	case OV_SHEET: sheet_draw(); break;
	case OV_SETTINGS: settings_view_draw(); break;
	case OV_ABOUT: about_draw(); break;
	default: break;
	}
}

/* backdrop under the system keyboard: the current screen, frozen */
static void osk_backdrop(void)
{
	draw_rect(0, 0, SCR_W, SCR_H, COL_BG);
	topbar_draw(g_mode);
	draw_screen();
	for (int i = 0; i < g_nov; i++)
		draw_overlay(g_ov[i]);
}

void app_init(void)
{
	calc_screen_init();
	graph_screen_init();
	gcse_screen_init();
	osk_set_backdrop(osk_backdrop);
	tour_start_if_requested();
}

void app_update(const input_t *in)
{
	g_now = in->now_ms;
	if (tour_active())
		tour_step();
	g_nhints = 0;
	if (g_nov > 0) {
		int top = g_ov[g_nov - 1];
		bool open = true;
		switch (top) {
		case OV_STEPS: open = steps_view_update(in); break;
		case OV_SHEET: open = sheet_update(in); break;
		case OV_SETTINGS: open = settings_view_update(in); break;
		case OV_ABOUT: open = about_update(in); break;
		default: open = false; break;
		}
		/* an overlay may open another one on top (settings -> about) */
		if (!open) {
			for (int i = g_nov - 1; i >= 0; i--) {
				if (g_ov[i] == top) {
					memmove(&g_ov[i], &g_ov[i + 1], sizeof(int) * (size_t)(g_nov - i - 1));
					g_nov--;
					break;
				}
			}
		}
		return;
	}
	bool lr_free = !(g_mode == MODE_GRAPH && graph_claims_lr());
	if (lr_free && (in->pressed & IN_L)) {
		app_set_mode((g_mode + MODE_COUNT - 1) % MODE_COUNT);
		return;
	}
	if (lr_free && (in->pressed & IN_R)) {
		app_set_mode((g_mode + 1) % MODE_COUNT);
		return;
	}
	if (in->pressed & IN_SELECT) {
		app_open_settings();
		return;
	}
	if (in->tap && in->tap_y < TOP_H) {
		int t = topbar_hit(in->tap_x, in->tap_y);
		if (t >= 0 && t < MODE_COUNT)
			app_set_mode(t);
		else if (t == MODE_COUNT)
			app_open_settings();
		else if (t == MODE_COUNT + 1) {
			g_set.degrees = !g_set.degrees;
			settings_save();
			app_toast(g_set.degrees ? "Angles in degrees" : "Angles in radians");
		}
		return;
	}
	switch (g_mode) {
	case MODE_GRAPH: graph_screen_update(in); break;
	case MODE_GCSE: gcse_screen_update(in); break;
	default: calc_screen_update(g_mode, in); break;
	}
}

void app_draw(void)
{
	draw_rect(0, 0, SCR_W, SCR_H, COL_BG);
	draw_screen();
	topbar_draw(g_mode);
	for (int i = 0; i < g_nov; i++)
		draw_overlay(g_ov[i]);
	hintbar_draw(g_hints, g_nhints);
	if (g_now < g_toast_until) {
		int64_t left = (int64_t)(g_toast_until - g_now);
		int a = left > 300 ? 255 : (int)(left * 255 / 300);
		toast_draw(g_toast, a);
	}
}
