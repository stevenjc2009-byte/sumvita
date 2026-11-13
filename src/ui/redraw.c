/* redraw.c - the idle redraw skip. The screen only changes after input, while
 * a timed effect from that input runs, when the caret blinks, or during a tour.
 * Anything doubtful draws. */
#include "ui/redraw.h"

#include "ui/app.h"

#include <stdint.h>

/* Covers every timed effect started by input: the toast (up to 3500 ms with its
 * fade, app.c app_toast) and the key / button press flash (120 ms, calcview.c,
 * calcview_actions.c, sheet.c). */
#define AFTER_INPUT_MS 4000
#define BLINK_MS       530 /* caret blink period, calcview_draw.c and sheet.c */
#define BACKSTOP_MS    500 /* draw at least this often whatever happens */
#define STICK_DEAD     40  /* = ANALOG_DEAD in input.c: smaller deflections do nothing */

static bool stick_live(int v) { return v - 128 >= STICK_DEAD || 128 - v >= STICK_DEAD; }

static bool input_live(const input_t *in)
{
	/* held buttons cover d-pad key repeat and the left stick used as a d-pad */
	return in->held || in->pressed || in->released || in->touch_count || in->touch_began ||
	       in->touch_ended || in->tap || stick_live(in->lx) || stick_live(in->ly) ||
	       stick_live(in->rx) || stick_live(in->ry);
}

bool redraw_needed(const input_t *in)
{
	static bool started;
	static uint64_t last_input, last_draw, blink;
	uint64_t now = in->now_ms;
	uint64_t phase = now / BLINK_MS;
	if (!started) {
		started = true;
		last_input = now;
	}
	if (input_live(in))
		last_input = now;
	bool draw = tour_active() || now - last_input < AFTER_INPUT_MS || phase != blink ||
		    now - last_draw >= BACKSTOP_MS || now < last_draw;
	if (draw) {
		last_draw = now;
		blink = phase;
	}
	return draw;
}
