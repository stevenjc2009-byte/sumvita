/* theme.h - Sumvita palette: deep slate panels, high-contrast text, one calm
 * accent colour the user picks in Settings. */
#ifndef UI_THEME_H
#define UI_THEME_H

#include <vita2d.h>

#define COL_BG        RGBA8(13, 16, 22, 255)
#define COL_BG2       RGBA8(18, 22, 30, 255)
#define COL_PANEL     RGBA8(24, 29, 39, 255)
#define COL_PANEL2    RGBA8(31, 37, 49, 255)
#define COL_PANEL3    RGBA8(40, 47, 62, 255)
#define COL_LINE      RGBA8(50, 58, 75, 255)
#define COL_LINE_SOFT RGBA8(36, 42, 55, 255)
#define COL_TEXT      RGBA8(238, 241, 246, 255)
#define COL_TEXT2     RGBA8(200, 207, 218, 255)
#define COL_MUTED     RGBA8(142, 153, 171, 255)
#define COL_DIM       RGBA8(96, 106, 124, 255)
#define COL_SHADOW    RGBA8(0, 0, 0, 110)
#define COL_SCRIM     RGBA8(5, 7, 10, 200)

#define COL_KEY_NUM   RGBA8(52, 60, 77, 255)
#define COL_KEY_OP    RGBA8(40, 47, 62, 255)
#define COL_KEY_FN    RGBA8(32, 38, 50, 255)
#define COL_KEY_DEL   RGBA8(92, 46, 52, 255)
#define COL_KEY_EDGE  RGBA8(0, 0, 0, 90)

#define COL_ERR       RGBA8(255, 112, 112, 255)
#define COL_OK        RGBA8(110, 214, 150, 255)
#define COL_WARN      RGBA8(255, 196, 92, 255)

/* Graph function colours (y1..y6). */
#define GRAPH_COLOURS 6
unsigned int theme_graph_colour(int i);

/* Accent: picked in Settings, persisted. */
int  theme_accent_count(void);
const char *theme_accent_name(int i);
unsigned int theme_accent_swatch(int i);
void theme_set_accent(int i);
int  theme_get_accent(void);
unsigned int theme_accent(void);       /* full colour */
unsigned int theme_accent_dim(void);   /* darker, for filled backgrounds */
unsigned int theme_accent_soft(void);  /* translucent wash */
unsigned int theme_on_accent(void);    /* text drawn on the accent colour */

/* Blend helpers. */
unsigned int col_alpha(unsigned int c, int a);
unsigned int col_mix(unsigned int a, unsigned int b, int t256);

#endif
