/* draw.h - primitives and code-drawn button glyphs (from Noctopage, extended). */
#ifndef UI_DRAW_H
#define UI_DRAW_H

#include <stdint.h>

typedef enum {
	GLYPH_CROSS,
	GLYPH_CIRCLE,
	GLYPH_SQUARE,
	GLYPH_TRIANGLE,
	GLYPH_L,
	GLYPH_R,
	GLYPH_LR,
	GLYPH_START,
	GLYPH_SELECT,
	GLYPH_DPAD,
	GLYPH_STICK_L,
	GLYPH_STICK_R,
	GLYPH_TOUCH,
	GLYPH_CONFIRM, /* Cross or Circle, whichever the system enter button is */
	GLYPH_BACK,    /* the other one */
} glyph_t;

#define GLYPH_SIZE 18

void draw_rect(int x, int y, int w, int h, unsigned int color);
void draw_rrect(int x, int y, int w, int h, int r, unsigned int color);
/* Rects, rrects, outlines, shadows and thick lines: one draw call each (draw_shape.c). */
/* Rounded outline of thickness t (drawn as two filled rrects: needs a fill colour
 * for the inside; pass inner = 0 to draw only the ring over an existing fill). */
void draw_rrect_ring(int x, int y, int w, int h, int r, int t, unsigned int ring, unsigned int inner);
/* Rounded outline only (inside left untouched), safe for translucent colours. */
void draw_rrect_outline(int x, int y, int w, int h, int r, int t, unsigned int color);
void draw_gradient_v(int x, int y, int w, int h, unsigned int top, unsigned int bottom);
void draw_border(int x, int y, int w, int h, int thickness, unsigned int color);
/* Soft drop shadow under a rounded panel. */
void draw_shadow(int x, int y, int w, int h, int r);
/* Line with thickness (in px, >= 1). */
void draw_line_w(float x0, float y0, float x1, float y1, float w, unsigned int color);
void draw_disc(float cx, float cy, float r, unsigned int color);
void draw_ring(float cx, float cy, float r, float t, unsigned int color, unsigned int inner);
/* Dashed rectangle outline (empty slot placeholder). */
void draw_dashed_rect(int x, int y, int w, int h, unsigned int color);

/* Shape run: draw_rect / draw_rrect / draw_rrect_ring / draw_rrect_outline /
 * draw_shadow / draw_line_w / draw_gradient_v / draw_border / draw_dashed_rect
 * between begin and end go to the GPU as one draw call, in order. Nothing
 * else (text, discs, glyphs, textures, clipping) may be drawn inside a run.
 * Runs nest; the outermost end draws. (draw_shape.c) */
void draw_shapes_begin(void);
void draw_shapes_end(void);

/* Scissor to a rectangle; draw_clip_off() restores full-screen drawing. */
void draw_clip(int x, int y, int w, int h);
void draw_clip_off(void);

int draw_glyph(glyph_t g, int x, int y);
int draw_glyph_width(glyph_t g);

/* Loads app0:img/<name> once; NULL if absent. */
void *draw_load_png(const char *path);
void draw_texture_fit(void *tex, int x, int y, int w, int h);

#endif
