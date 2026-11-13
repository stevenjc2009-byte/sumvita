/* draw_batch.h - collects thick lines and rectangles of one colour as triangles
 * and sends them to the GPU in ONE vita2d_draw_array call. Meant for the graph
 * (hundreds of segments per curve). Call batch_end() before any other drawing
 * whose order matters; a colour change flushes by itself. */
#ifndef UI_DRAW_BATCH_H
#define UI_DRAW_BATCH_H

/* Axis-aligned filled rectangle (float coordinates, same as draw_rect). */
void batch_rect(float x, float y, float w, float h, unsigned int color);

/* Thick segment as a quad. ext0/ext1: extend that end by half the width along
 * the segment, so neighbouring segments overlap at a join (no gap at bends). */
void batch_line(float x0, float y0, float x1, float y1, float w, unsigned int color,
		int ext0, int ext1);

/* Send everything collected so far. Cheap when empty. */
void batch_end(void);

/* Draw calls issued since the last call to this function (for the perf counter). */
int batch_calls_take(void);

#endif
