/* history.c - fixed-size history; the oldest entry is dropped when full. */
#include "ui/history.h"

#include <string.h>

static HEntry g_h[HISTORY_MAX];
static int g_n;

void history_add(Res *r, int mode)
{
	if (g_n == HISTORY_MAX) {
		res_free(&g_h[0].r);
		memmove(&g_h[0], &g_h[1], sizeof(HEntry) * (HISTORY_MAX - 1));
		g_n--;
	}
	res_move(&g_h[g_n].r, r);
	g_h[g_n].mode = mode;
	g_n++;
}

int history_count(void) { return g_n; }

const HEntry *history_get(int i)
{
	if (i < 0 || i >= g_n)
		return NULL;
	return &g_h[i];
}

void history_clear(void)
{
	for (int i = 0; i < g_n; i++)
		res_free(&g_h[i].r);
	g_n = 0;
}
