/* redraw.h - skip drawing frames in which nothing on screen can have changed. */
#ifndef UI_REDRAW_H
#define UI_REDRAW_H

#include <stdbool.h>

#include "ui/input.h"

/* Call once a frame after tour_input() and app_update(). true = draw this frame;
 * false = the last frame is still right (the caller only waits for vblank). */
bool redraw_needed(const input_t *in);

#endif
