/* UI text: vita2d_font (FreeType) on the TTFs packed at app0:fonts/.
 * Adapted from Noctopage src/ui/font.c (per-code-point face fallback drawn as
 * runs of one face each). Differences:
 *   - fonts come from files, read once into memory and shared by every
 *     vita2d_font instance made from them;
 *   - three weights, each with a DejaVu fallback;
 *   - one vita2d_font per (face, weight, EXACT pixel size), made the first time
 *     that size is drawn. vita2d keys an atlas glyph by glyph index only: it is
 *     rasterised at the size it is first used at and scaled for every other
 *     size through a point-filtered texture, so a glyph drawn smaller than it
 *     was rasterised loses whole columns ("100" came out as "1CC", "to" as
 *     "lo") and its advance is cut to whole pixels at the first size (spaces
 *     ~2 px wide). One size per font means vita2d never scales a glyph.
 * Spaces are advanced here from the face's own metrics. */
#include "ui/font.h"

#include "ui/log.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vita2d.h>

#define LINE_RATIO   1.30f /* line-box height / px */
#define ASCENT_RATIO 1.00f /* baseline offset from the line-box top / px (Ubuntu asc 0.93 + lead) */
#define RUN_MAX      256
#define PX_MIN       6
#define PX_MAX       64    /* sizes outside are clamped (the largest drawn is 52) */

typedef struct {
	unsigned char *data;
	long size;
	FT_Face face;          /* coverage checks + space metrics */
} ttf_t;

static FT_Library g_ft;
static ttf_t g_main[FW_COUNT], g_fb[FW_COUNT];
static float g_space[FW_COUNT];   /* space advance / px, from the main face */
static int g_fb_tried[FW_COUNT];  /* DejaVu is read on first need (1.47 MB) */
/* [0] main, [1] fallback; NULL until that size is first drawn */
static vita2d_font *g_v[2][FW_COUNT][PX_MAX + 1];
static int g_nfonts;

static const char *k_main_files[FW_COUNT] = {
	"app0:fonts/Ubuntu-R.ttf", "app0:fonts/Ubuntu-M.ttf", "app0:fonts/Ubuntu-B.ttf"
};
static const char *k_fb_files[FW_COUNT] = {
	"app0:fonts/DejaVuSans.ttf", "app0:fonts/DejaVuSans.ttf", "app0:fonts/DejaVuSans-Bold.ttf"
};

static int load_file(const char *path, ttf_t *t)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0) {
		fclose(f);
		return -1;
	}
	t->data = malloc((size_t)n);
	if (!t->data) {
		fclose(f);
		return -1;
	}
	if (fread(t->data, 1, (size_t)n, f) != (size_t)n) {
		fclose(f);
		free(t->data);
		t->data = NULL;
		return -1;
	}
	fclose(f);
	t->size = n;
	if (FT_New_Memory_Face(g_ft, t->data, (FT_Long)n, 0, &t->face) != 0)
		t->face = NULL;
	return 0;
}

static int same_file(int a, int b, const char **files) { return strcmp(files[a], files[b]) == 0; }

static float space_ratio(FT_Face f)
{
	FT_UInt gi = f ? FT_Get_Char_Index(f, ' ') : 0;
	if (!gi || FT_Load_Glyph(f, gi, FT_LOAD_NO_SCALE) != 0 || f->units_per_EM == 0)
		return 0.25f;
	return (float)f->glyph->advance.x / (float)f->units_per_EM;
}

void font_init(void)
{
	if (FT_Init_FreeType(&g_ft) != 0) {
		log_printf("font: FT_Init_FreeType failed");
		return;
	}
	for (int w = 0; w < FW_COUNT; w++)
		if (load_file(k_main_files[w], &g_main[w]) != 0)
			log_printf("font: cannot read %s", k_main_files[w]);
	/* A missing weight falls back to regular. */
	for (int w = 1; w < FW_COUNT; w++)
		if (!g_main[w].data)
			g_main[w] = g_main[FW_REG];
	for (int w = 0; w < FW_COUNT; w++)
		g_space[w] = space_ratio(g_main[w].face);
	log_printf("font: regular %s, fallback on first use", g_main[FW_REG].data ? "ok" : "FAILED");
}

/* Reads weight w's DejaVu file the first time a code point needs it (one try).
 * DejaVu regular serves REG and MED: they share one buffer and face. */
static void fb_load(fweight_t w)
{
	if (g_fb_tried[w])
		return;
	for (int o = 0; o < FW_COUNT; o++)
		if (o != (int)w && g_fb_tried[o] && same_file((int)w, o, k_fb_files)) {
			g_fb[w] = g_fb[o];
			g_fb_tried[w] = 1;
			return;
		}
	g_fb_tried[w] = 1;
	if (load_file(k_fb_files[w], &g_fb[w]) != 0)
		log_printf("font: cannot read %s", k_fb_files[w]);
	else
		log_printf("font: fallback %s loaded", k_fb_files[w]);
}

/* The vita2d_font for one face/weight/size, made on first use. */
static vita2d_font *vfont(int fb, fweight_t w, int px)
{
	if (px < PX_MIN)
		px = PX_MIN;
	if (px > PX_MAX)
		px = PX_MAX;
	vita2d_font **slot = &g_v[fb][w][px];
	if (!*slot) {
		const ttf_t *t = fb ? &g_fb[w] : &g_main[w];
		if (!t->data)
			return NULL;
		*slot = vita2d_load_font_mem(t->data, (unsigned int)t->size);
		if (*slot && ++g_nfonts % 8 == 0)
			log_printf("font: %d size atlases", g_nfonts);
	}
	return *slot;
}

int font_ok(void) { return vfont(0, FW_REG, 16) != NULL; }

int utf8_decode(const char *s, unsigned int *cp)
{
	const unsigned char *u = (const unsigned char *)s;
	if (u[0] < 0x80) { *cp = u[0]; return 1; }
	if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
		*cp = ((u[0] & 0x1Fu) << 6) | (u[1] & 0x3Fu);
		return 2;
	}
	if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
		*cp = ((u[0] & 0x0Fu) << 12) | ((u[1] & 0x3Fu) << 6) | (u[2] & 0x3Fu);
		return 3;
	}
	if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
		*cp = ((u[0] & 0x07u) << 18) | ((u[1] & 0x3Fu) << 12) | ((u[2] & 0x3Fu) << 6) | (u[3] & 0x3Fu);
		return 4;
	}
	*cp = 0xFFFD;
	return 1;
}

/* 0 = main face, 1 = fallback (a code point Ubuntu lacks). */
static int use_fallback(fweight_t w, unsigned int cp)
{
	if (cp < 0x80 || !g_main[w].face || FT_Get_Char_Index(g_main[w].face, cp) != 0)
		return 0;
	fb_load(w);
	return g_fb[w].data != NULL;
}

static int runs(fweight_t w, int x, int baseline, unsigned int color, int px, const char *utf8, int draw)
{
	char run[RUN_MAX];
	int width = 0;
	const char *p = utf8;
	if ((unsigned)w >= (unsigned)FW_COUNT)
		w = FW_REG;
	px = px < PX_MIN ? PX_MIN : px > PX_MAX ? PX_MAX : px;
	while (*p) {
		unsigned int cp;
		utf8_decode(p, &cp);
		if (cp == ' ') {
			width += (int)((float)px * g_space[w] + 0.5f);
			p++;
			continue;
		}
		int fb = use_fallback(w, cp);
		int len = 0;
		while (*p) {
			int n = utf8_decode(p, &cp);
			if (cp == ' ' || use_fallback(w, cp) != fb || len + n >= RUN_MAX)
				break;
			memcpy(run + len, p, (size_t)n);
			len += n;
			p += n;
		}
		run[len] = '\0';
		vita2d_font *f = vfont(fb, w, px);
		if (!f || len == 0)
			continue;
		if (draw)
			vita2d_font_draw_text(f, x + width, baseline, color, (unsigned int)px, run);
		/* drawing only needs the width to place a later run */
		if (!draw || *p)
			width += vita2d_font_text_width(f, (unsigned int)px, run);
	}
	return width;
}

void font_draw_base(fweight_t w, int x, int baseline, unsigned int color, int px, const char *utf8)
{
	if (!utf8 || !*utf8)
		return;
	runs(w, x, baseline, color, px, utf8, 1);
}

void font_draw_w(fweight_t w, int x, int y, unsigned int color, int px, const char *utf8)
{
	font_draw_base(w, x, y + font_ascent(px), color, px, utf8);
}

void font_draw(int x, int y, unsigned int color, int px, const char *utf8)
{
	font_draw_w(FW_REG, x, y, color, px, utf8);
}

void font_draw_c(fweight_t w, int cx, int y, unsigned int color, int px, const char *utf8)
{
	font_draw_w(w, cx - font_width_w(w, px, utf8) / 2, y, color, px, utf8);
}

void font_draw_r(fweight_t w, int xr, int y, unsigned int color, int px, const char *utf8)
{
	font_draw_w(w, xr - font_width_w(w, px, utf8), y, color, px, utf8);
}

int font_width_w(fweight_t w, int px, const char *utf8)
{
	if (!utf8 || !*utf8)
		return 0;
	return runs(w, 0, 0, 0, px, utf8, 0);
}

int font_width(int px, const char *utf8) { return font_width_w(FW_REG, px, utf8); }

int font_height(int px) { return (int)((float)px * LINE_RATIO + 0.5f); }

int font_ascent(int px) { return (int)((float)px * ASCENT_RATIO + 0.5f); }
