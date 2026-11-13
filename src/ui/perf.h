/* Per-frame draw counters for measuring render cost (logged with each tour shot).
 * The vita2d draw functions are wrapped at link time (-Wl,--wrap, see Makefile). */
#ifndef SUMVITA_PERF_H
#define SUMVITA_PERF_H

typedef struct {
	unsigned draws;   /* vita2d draw calls (each one is a GPU draw) */
	unsigned glyphs;  /* glyph quads drawn by the font code */
	unsigned cpu_us;  /* CPU time spent in app_draw */
} perf_frame_t;

void perf_frame_begin(void);
void perf_frame_end(void);
/* the last finished frame's counts */
perf_frame_t perf_last(void);

#endif
