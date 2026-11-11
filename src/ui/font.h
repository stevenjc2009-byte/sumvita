/* font.h - UTF-8 text with the packed TTFs (app0:fonts/).
 * Ubuntu (Regular / Medium / Bold) is the main face; any code point Ubuntu
 * lacks (⁻, ∴, ⚙, ...) is drawn from DejaVu Sans of the same weight. */
#ifndef UI_FONT_H
#define UI_FONT_H

typedef enum { FW_REG = 0, FW_MED, FW_BOLD, FW_COUNT } fweight_t;

/* Call after vita2d_init(). Fonts are never freed (process exits). */
void font_init(void);
/* 1 when at least the regular face loaded. */
int font_ok(void);

/* Top-left placement (y = top of the line box). Regular weight. */
void font_draw(int x, int y, unsigned int color, int px, const char *utf8);
void font_draw_w(fweight_t w, int x, int y, unsigned int color, int px, const char *utf8);
/* Baseline placement. */
void font_draw_base(fweight_t w, int x, int baseline, unsigned int color, int px, const char *utf8);
/* Centred horizontally on cx, top at y. */
void font_draw_c(fweight_t w, int cx, int y, unsigned int color, int px, const char *utf8);
/* Right-aligned at x (right edge), top at y. */
void font_draw_r(fweight_t w, int xr, int y, unsigned int color, int px, const char *utf8);

int font_width(int px, const char *utf8);
int font_width_w(fweight_t w, int px, const char *utf8);
int font_height(int px);   /* line box height */
int font_ascent(int px);   /* baseline distance from the line-box top */

/* UTF-8 decode helper shared by text code: returns bytes used (>= 1). */
int utf8_decode(const char *s, unsigned int *cp);

#endif
