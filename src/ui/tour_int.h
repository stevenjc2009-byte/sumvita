/* tour_int.h - shared by the scripted tour files: tour.c (start, shots with a
 * PC acknowledgement, the built-in screenshot walk), tour_input.c (virtual
 * button presses and touches fed into the same input_t the UI reads) and
 * tour_script.c / tour_cmds.c (the line-based script in tour.txt). */
#ifndef UI_TOUR_INT_H
#define UI_TOUR_INT_H

#include <stdbool.h>
#include <stdint.h>

/* tour.c */
void tour_shot_begin(const char *name);  /* settle, log "TOUR shot NAME", wait for the PC's ack */
void tour_finish(void);                  /* "TOUR done", restore settings, quit */

/* tour_input.c: queued virtual input, one event at a time */
uint32_t tq_bits(const char *name);      /* "CROSS", "UP", "CONFIRM" ... -> IN_* bits, 0 unknown */
void tq_press(uint32_t bits, int count);
void tq_tap(int x, int y);
void tq_drag(int x, int y, int dy, int frames);
void tq_wait(int frames);
bool tq_idle(void);

/* tour_script.c */
bool ts_load(char *text);                /* takes ownership of a malloc'd buffer */
bool ts_step(void);                      /* false once the script has ended */

/* tour_cmds.c: one command; returns TC_NEXT (run the next line now),
 * TC_YIELD (next line next frame) or TC_AGAIN (run this line again next frame) */
enum { TC_NEXT = 0, TC_YIELD, TC_AGAIN, TC_END };
int tc_exec(const char *cmd, const char *arg);

#endif
