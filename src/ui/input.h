/* input.h - buttons, d-pad key repeat, analog sticks, front touch with tap /
 * drag / pinch. Adapted from Noctopage src/ui/input.h. Screens use the logical
 * IN_CONFIRM / IN_BACK so the system Cross/Circle enter setting is honoured. */
#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdint.h>

enum {
	IN_UP       = 1u << 0,
	IN_DOWN     = 1u << 1,
	IN_LEFT     = 1u << 2,
	IN_RIGHT    = 1u << 3,
	IN_CROSS    = 1u << 4,
	IN_CIRCLE   = 1u << 5,
	IN_SQUARE   = 1u << 6,
	IN_TRIANGLE = 1u << 7,
	IN_L        = 1u << 8,
	IN_R        = 1u << 9,
	IN_START    = 1u << 10,
	IN_SELECT   = 1u << 11,
	IN_CONFIRM  = 1u << 12,
	IN_BACK     = 1u << 13,
};

#define IN_DIRS (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT)
#define INPUT_MAX_TOUCH 6

typedef struct {
	int id;
	int x, y; /* screen coordinates, 960x544 */
} touch_pt_t;

typedef struct {
	uint32_t pressed;  /* went down this frame; d-pad bits also fire on key repeat */
	uint32_t held;
	uint32_t released;
	int lx, ly, rx, ry; /* sticks, 0..255, centre 128 */
	int touch_count;
	touch_pt_t touch[INPUT_MAX_TOUCH];
	int touch_began;      /* first finger landed this frame ... */
	int began_x, began_y; /* ... here */
	int touch_ended;      /* last finger lifted this frame */
	int tap;              /* ... and it was a tap (short, did not move) */
	int tap_x, tap_y;
	int dragging;         /* one finger down and it has moved past the slop */
	int drag_dx, drag_dy; /* primary finger movement since last frame */
	int drag_total;       /* max distance from the start point, px */
	float pinch;          /* two-finger distance ratio this frame (1 = none) */
	int pinch_cx, pinch_cy;
	uint64_t now_ms;
} input_t;

void input_init(void);
void input_poll(input_t *in);
int input_confirm_is_cross(void);

/* Right stick vertical / horizontal as -1..1 with a dead zone. */
float input_rstick_y(const input_t *in);
float input_rstick_x(const input_t *in);
float input_lstick_x(const input_t *in);
float input_lstick_y(const input_t *in);

/* Point-in-rect helper. */
static inline int in_rect(int px, int py, int x, int y, int w, int h)
{
	return px >= x && py >= y && px < x + w && py < y + h;
}

#endif
