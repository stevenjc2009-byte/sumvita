/* tour_input.c - virtual input for the scripted tour. Events are queued by the
 * script and merged into the frame's input_t right after input_poll(), so the
 * screens see them exactly as real buttons and touches. Inert (nothing queued)
 * unless a tour is running. */
#include "ui/app.h"
#include "ui/tour_int.h"

#include <string.h>

enum { EV_PRESS = 1, EV_TAP, EV_DRAG, EV_WAIT };

typedef struct {
	uint8_t kind;
	uint32_t bits;
	int16_t x, y, dy, frames;
} TEv;

#define TQ_MAX 512
#define PRESS_FRAMES 4   /* 1 down, 3 up: no accidental key repeat */
#define TAP_FRAMES 4

static TEv g_q[TQ_MAX];
static int g_head, g_len, g_phase;

static const struct {
	const char *name;
	uint32_t bit;
} k_names[] = {
	{ "UP", IN_UP }, { "DOWN", IN_DOWN }, { "LEFT", IN_LEFT }, { "RIGHT", IN_RIGHT },
	{ "CROSS", IN_CROSS }, { "CIRCLE", IN_CIRCLE }, { "SQUARE", IN_SQUARE }, { "TRIANGLE", IN_TRIANGLE },
	{ "L", IN_L }, { "R", IN_R }, { "START", IN_START }, { "SELECT", IN_SELECT },
	{ "CONFIRM", IN_CONFIRM }, { "BACK", IN_BACK },
};

/* the physical button carries the logical one, as input_poll does */
static uint32_t with_logical(uint32_t b)
{
	bool cross = input_confirm_is_cross();
	if (b & IN_CONFIRM) b |= cross ? IN_CROSS : IN_CIRCLE;
	if (b & IN_BACK) b |= cross ? IN_CIRCLE : IN_CROSS;
	if (b & IN_CROSS) b |= cross ? IN_CONFIRM : IN_BACK;
	if (b & IN_CIRCLE) b |= cross ? IN_BACK : IN_CONFIRM;
	return b;
}

uint32_t tq_bits(const char *name)
{
	for (size_t i = 0; i < sizeof k_names / sizeof k_names[0]; i++)
		if (!strcmp(name, k_names[i].name))
			return with_logical(k_names[i].bit);
	return 0;
}

static TEv *push(int kind)
{
	if (g_len >= TQ_MAX)
		return NULL;
	TEv *e = &g_q[(g_head + g_len++) % TQ_MAX];
	memset(e, 0, sizeof *e);
	e->kind = (uint8_t)kind;
	return e;
}

void tq_press(uint32_t bits, int count)
{
	for (int i = 0; i < count; i++) {
		TEv *e = push(EV_PRESS);
		if (e)
			e->bits = bits;
	}
}

void tq_tap(int x, int y)
{
	TEv *e = push(EV_TAP);
	if (e) {
		e->x = (int16_t)x;
		e->y = (int16_t)y;
	}
}

void tq_drag(int x, int y, int dy, int frames)
{
	TEv *e = push(EV_DRAG);
	if (e) {
		e->x = (int16_t)x;
		e->y = (int16_t)y;
		e->dy = (int16_t)dy;
		e->frames = (int16_t)(frames < 2 ? 2 : frames);
	}
}

void tq_wait(int frames)
{
	TEv *e = push(EV_WAIT);
	if (e)
		e->frames = (int16_t)frames;
}

bool tq_idle(void) { return g_len == 0; }

static void finger(input_t *in, int x, int y)
{
	in->touch_count = 1;
	in->touch[0].id = 77;
	in->touch[0].x = x;
	in->touch[0].y = y;
}

/* returns true when the event is finished after this frame */
static bool apply(const TEv *e, int ph, input_t *in)
{
	switch (e->kind) {
	case EV_PRESS:
		if (ph == 0) {
			in->pressed |= e->bits;
			in->held |= e->bits;
		} else if (ph == 1) {
			in->released |= e->bits;
		}
		return ph >= PRESS_FRAMES - 1;
	case EV_TAP:
		if (ph == 0) {
			finger(in, e->x, e->y);
			in->touch_began = 1;
			in->began_x = e->x;
			in->began_y = e->y;
		} else if (ph == 1) {
			in->touch_ended = 1;
			in->tap = 1;
			in->tap_x = e->x;
			in->tap_y = e->y;
		}
		return ph >= TAP_FRAMES - 1;
	case EV_DRAG: {
		int step = e->dy / e->frames;
		in->began_x = e->x;
		in->began_y = e->y;
		if (ph == 0) {
			finger(in, e->x, e->y);
			in->touch_began = 1;
		} else if (ph <= e->frames) {
			finger(in, e->x, e->y + step * ph);
			in->dragging = 1;
			in->drag_dy = step;
			in->drag_total = 20 + (step < 0 ? -step : step) * ph;
		} else {
			in->touch_ended = 1;
			in->drag_total = 99;
		}
		return ph > e->frames;
	}
	case EV_WAIT:
		return ph >= e->frames - 1;
	default:
		return true;
	}
}

void tour_input(input_t *in)
{
	if (g_len == 0)
		return;
	if (apply(&g_q[g_head], g_phase, in)) {
		g_head = (g_head + 1) % TQ_MAX;
		g_len--;
		g_phase = 0;
	} else {
		g_phase++;
	}
}
