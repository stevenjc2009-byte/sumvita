#include "ui/theme.h"

typedef struct {
	const char *name;
	unsigned int c;
} accent_t;

static const accent_t k_accents[] = {
	{ "Teal",   RGBA8(64, 206, 186, 255) },
	{ "Blue",   RGBA8(96, 160, 255, 255) },
	{ "Violet", RGBA8(168, 132, 255, 255) },
	{ "Amber",  RGBA8(255, 184, 76, 255) },
	{ "Rose",   RGBA8(255, 116, 150, 255) },
	{ "Lime",   RGBA8(156, 214, 92, 255) },
};
#define N_ACCENTS ((int)(sizeof k_accents / sizeof k_accents[0]))

static const unsigned int k_graph[GRAPH_COLOURS] = {
	RGBA8(255, 107, 107, 255), RGBA8(92, 168, 255, 255), RGBA8(110, 214, 150, 255),
	RGBA8(255, 190, 80, 255),  RGBA8(196, 140, 255, 255), RGBA8(80, 214, 220, 255),
};

static int g_accent;

unsigned int col_alpha(unsigned int c, int a)
{
	if (a < 0) a = 0;
	if (a > 255) a = 255;
	return (c & 0x00FFFFFFu) | ((unsigned int)a << 24);
}

unsigned int col_mix(unsigned int a, unsigned int b, int t)
{
	unsigned int out = 0;
	for (int s = 0; s < 32; s += 8) {
		int ca = (int)((a >> s) & 0xFF), cb = (int)((b >> s) & 0xFF);
		out |= (unsigned int)((ca * (256 - t) + cb * t) >> 8) << s;
	}
	return out;
}

unsigned int theme_graph_colour(int i)
{
	if (i < 0) i = 0;
	return k_graph[i % GRAPH_COLOURS];
}

int theme_accent_count(void) { return N_ACCENTS; }

const char *theme_accent_name(int i)
{
	return (i >= 0 && i < N_ACCENTS) ? k_accents[i].name : "";
}

unsigned int theme_accent_swatch(int i)
{
	return (i >= 0 && i < N_ACCENTS) ? k_accents[i].c : k_accents[0].c;
}

void theme_set_accent(int i)
{
	g_accent = (i >= 0 && i < N_ACCENTS) ? i : 0;
}

int theme_get_accent(void) { return g_accent; }

unsigned int theme_accent(void) { return k_accents[g_accent].c; }

unsigned int theme_accent_dim(void)
{
	return col_mix(k_accents[g_accent].c, COL_PANEL, 150);
}

unsigned int theme_accent_soft(void)
{
	return col_alpha(k_accents[g_accent].c, 46);
}

unsigned int theme_on_accent(void)
{
	return RGBA8(10, 14, 20, 255);
}
