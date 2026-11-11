#include "ui/input.h"

#include <math.h>
#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/system_param.h>
#include <psp2/touch.h>
#include <stdlib.h>
#include <string.h>

#define REPEAT_DELAY_MS 380
#define REPEAT_RATE_MS  70
#define STICK_DEADZONE  60  /* left stick: distance from 128 that counts as a d-pad press */
#define ANALOG_DEAD     40  /* analog readings below this are zero */
#define TAP_SLOP        14  /* px a tap may wander */
#define TAP_MAX_MS      450

static int g_confirm_cross = 1;
static uint32_t g_prev_held;
static uint64_t g_repeat_at[4];
static int g_prev_touch_ids[INPUT_MAX_TOUCH];
static int g_prev_touch_n;
/* primary-finger gesture state */
static int g_t_down, g_t_id, g_t_x0, g_t_y0, g_t_lx, g_t_ly, g_t_max;
static uint64_t g_t_start;
static float g_pinch_d;

void input_init(void)
{
	SceAppUtilInitParam init;
	SceAppUtilBootParam boot;
	memset(&init, 0, sizeof init);
	memset(&boot, 0, sizeof boot);
	sceAppUtilInit(&init, &boot);

	int v = 0;
	if (sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &v) >= 0)
		g_confirm_cross = (v == SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS);

	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
}

int input_confirm_is_cross(void) { return g_confirm_cross; }

static float axis(int v)
{
	int d = v - 128;
	if (abs(d) < ANALOG_DEAD)
		return 0.0f;
	float f = (float)(d > 0 ? d - ANALOG_DEAD : d + ANALOG_DEAD) / (float)(127 - ANALOG_DEAD);
	return f < -1.0f ? -1.0f : f > 1.0f ? 1.0f : f;
}

float input_rstick_y(const input_t *in) { return axis(in->ry); }
float input_rstick_x(const input_t *in) { return axis(in->rx); }
float input_lstick_x(const input_t *in) { return axis(in->lx); }
float input_lstick_y(const input_t *in) { return axis(in->ly); }

static uint32_t read_buttons(input_t *in)
{
	SceCtrlData pad;
	memset(&pad, 0, sizeof pad);
	sceCtrlPeekBufferPositive(0, &pad, 1);

	uint32_t m = 0;
	if (pad.buttons & SCE_CTRL_UP)       m |= IN_UP;
	if (pad.buttons & SCE_CTRL_DOWN)     m |= IN_DOWN;
	if (pad.buttons & SCE_CTRL_LEFT)     m |= IN_LEFT;
	if (pad.buttons & SCE_CTRL_RIGHT)    m |= IN_RIGHT;
	if (pad.buttons & SCE_CTRL_CROSS)    m |= IN_CROSS;
	if (pad.buttons & SCE_CTRL_CIRCLE)   m |= IN_CIRCLE;
	if (pad.buttons & SCE_CTRL_SQUARE)   m |= IN_SQUARE;
	if (pad.buttons & SCE_CTRL_TRIANGLE) m |= IN_TRIANGLE;
	if (pad.buttons & (SCE_CTRL_LTRIGGER | SCE_CTRL_L1)) m |= IN_L;
	if (pad.buttons & (SCE_CTRL_RTRIGGER | SCE_CTRL_R1)) m |= IN_R;
	if (pad.buttons & SCE_CTRL_START)    m |= IN_START;
	if (pad.buttons & SCE_CTRL_SELECT)   m |= IN_SELECT;

	in->lx = pad.lx; in->ly = pad.ly;
	in->rx = pad.rx; in->ry = pad.ry;

	/* Left stick also acts as the d-pad. */
	if (pad.lx < 128 - STICK_DEADZONE) m |= IN_LEFT;
	if (pad.lx > 128 + STICK_DEADZONE) m |= IN_RIGHT;
	if (pad.ly < 128 - STICK_DEADZONE) m |= IN_UP;
	if (pad.ly > 128 + STICK_DEADZONE) m |= IN_DOWN;
	return m;
}

static void read_touch(input_t *in)
{
	SceTouchData td;
	memset(&td, 0, sizeof td);
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &td, 1);

	int n = (int)td.reportNum;
	if (n > INPUT_MAX_TOUCH)
		n = INPUT_MAX_TOUCH;
	in->touch_count = n;
	in->pinch = 1.0f;
	for (int i = 0; i < n; i++) {
		in->touch[i].id = td.report[i].id;
		in->touch[i].x = td.report[i].x / 2; /* panel 1920x1088 -> 960x544 */
		in->touch[i].y = td.report[i].y / 2;
		int known = 0;
		for (int j = 0; j < g_prev_touch_n; j++)
			if (g_prev_touch_ids[j] == in->touch[i].id)
				known = 1;
		if (!known && !g_t_down) {
			in->touch_began = 1;
			in->began_x = in->touch[i].x;
			in->began_y = in->touch[i].y;
		}
	}

	if (in->touch_began) {
		g_t_down = 1;
		g_t_id = in->touch[0].id;
		g_t_x0 = g_t_lx = in->began_x;
		g_t_y0 = g_t_ly = in->began_y;
		g_t_max = 0;
		g_t_start = in->now_ms;
	}
	if (g_t_down && n > 0) {
		/* follow the primary finger, or whichever finger is left */
		int k = 0;
		for (int i = 0; i < n; i++)
			if (in->touch[i].id == g_t_id)
				k = i;
		g_t_id = in->touch[k].id;
		int x = in->touch[k].x, y = in->touch[k].y;
		in->drag_dx = x - g_t_lx;
		in->drag_dy = y - g_t_ly;
		g_t_lx = x;
		g_t_ly = y;
		int d = abs(x - g_t_x0) + abs(y - g_t_y0);
		if (d > g_t_max)
			g_t_max = d;
		in->drag_total = g_t_max;
		in->dragging = g_t_max > TAP_SLOP && n == 1;
		if (n >= 2) {
			float dx = (float)(in->touch[0].x - in->touch[1].x), dy = (float)(in->touch[0].y - in->touch[1].y);
			float dist = sqrtf(dx * dx + dy * dy);
			if (g_pinch_d > 1.0f && dist > 1.0f)
				in->pinch = dist / g_pinch_d;
			g_pinch_d = dist;
			in->pinch_cx = (in->touch[0].x + in->touch[1].x) / 2;
			in->pinch_cy = (in->touch[0].y + in->touch[1].y) / 2;
			g_t_max = 1000; /* a pinch is never a tap */
			in->drag_dx = in->drag_dy = 0;
		} else {
			g_pinch_d = 0.0f;
		}
	}
	if (g_t_down && n == 0) {
		g_t_down = 0;
		g_pinch_d = 0.0f;
		in->touch_ended = 1;
		in->drag_total = g_t_max;
		if (g_t_max <= TAP_SLOP && in->now_ms - g_t_start <= TAP_MAX_MS) {
			in->tap = 1;
			in->tap_x = g_t_x0;
			in->tap_y = g_t_y0;
		}
	}
	for (int i = 0; i < n; i++)
		g_prev_touch_ids[i] = in->touch[i].id;
	g_prev_touch_n = n;
}

void input_poll(input_t *in)
{
	memset(in, 0, sizeof *in);
	in->now_ms = sceKernelGetProcessTimeWide() / 1000;

	uint32_t held = read_buttons(in);
	uint32_t enter = g_confirm_cross ? IN_CROSS : IN_CIRCLE;
	uint32_t back  = g_confirm_cross ? IN_CIRCLE : IN_CROSS;
	if (held & enter) held |= IN_CONFIRM;
	if (held & back)  held |= IN_BACK;

	uint32_t pressed  = held & ~g_prev_held;
	uint32_t released = g_prev_held & ~held;

	static const uint32_t k_repeat_bits[4] = { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT };
	for (int i = 0; i < 4; i++) {
		uint32_t bit = k_repeat_bits[i];
		if (!(held & bit))
			continue;
		if (pressed & bit) {
			g_repeat_at[i] = in->now_ms + REPEAT_DELAY_MS;
		} else if (in->now_ms >= g_repeat_at[i]) {
			pressed |= bit;
			g_repeat_at[i] = in->now_ms + REPEAT_RATE_MS;
		}
	}

	in->held = held;
	in->pressed = pressed;
	in->released = released;
	g_prev_held = held;

	read_touch(in);
}
