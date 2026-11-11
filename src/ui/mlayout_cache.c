/* mlayout_cache.c - parse + lay out display syntax once, reuse every frame.
 * Parsing runs in the cache's own arena so the engine's current arena (where
 * calc_run results live until the caller copies them) is never touched. */
#include "ui/mlayout.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "ui/font.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_N 128

typedef struct {
	uint32_t hash;
	int px, kind; /* kind: 0 maths, 1+weight plain text */
	char *src;
	MLayout *l;
	uint32_t used;
} entry_t;

static entry_t g_e[CACHE_N];
static Arena *g_parse;
static uint32_t g_tick = 1;

static uint32_t fnv(const char *s, int px, int kind)
{
	uint32_t h = 2166136261u;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++)
		h = (h ^ *p) * 16777619u;
	h = (h ^ (uint32_t)px) * 16777619u;
	return (h ^ (uint32_t)kind) * 16777619u;
}

static MLayout *plain(const char *s, int px, int weight, int role)
{
	ml_reset();
	return ml_finish(ml_text(s, px, weight, role));
}

static MLayout *build(const char *src, int px)
{
	if (!g_parse)
		g_parse = arena_new(64 * 1024);
	if (!g_parse)
		return plain(src, px, FW_REG, MR_NORMAL);
	Arena *prev = arena_use(g_parse);
	arena_reset(g_parse);
	char err[96];
	int pos = -1;
	Expr *e = *src ? ex_parse(src, err, (int)sizeof err, &pos) : NULL;
	MLayout *l;
	if (e) {
		ml_reset();
		l = ml_finish(ml_expr(e, px));
	} else {
		/* Unparsed text is shown as typed, but the "..." text-token quotes are
		 * syntax, never something to show ('"HCF"=6' reads HCF=6). */
		char buf[512];
		size_t n = 0;
		const char *p = src;
		for (; *p && n + 1 < sizeof buf; p++)
			if (*p != '"')
				buf[n++] = *p;
		buf[n] = '\0';
		l = plain(*p ? src : buf, px, FW_REG, MR_NORMAL);
	}
	arena_reset(g_parse);
	arena_use(prev);
	return l;
}

static const MLayout *lookup(const char *src, int px, int kind)
{
	if (!src)
		src = "";
	uint32_t h = fnv(src, px, kind);
	int victim = 0;
	uint32_t oldest = 0xFFFFFFFFu;
	for (int i = 0; i < CACHE_N; i++) {
		entry_t *e = &g_e[i];
		if (e->l && e->hash == h && e->px == px && e->kind == kind && !strcmp(e->src, src)) {
			e->used = g_tick;
			return e->l;
		}
		uint32_t u = e->l ? e->used : 0;
		if (u < oldest) {
			oldest = u;
			victim = i;
		}
	}
	MLayout *l = kind == 0 ? build(src, px) : plain(src, px, kind - 1, MR_NORMAL);
	if (!l)
		return NULL;
	char *copy = strdup(src);
	if (!copy) {
		/* cannot cache: keep it in the victim slot anyway without a key */
		ml_free(l);
		return NULL;
	}
	entry_t *v = &g_e[victim];
	if (v->l) {
		ml_free(v->l);
		free(v->src);
	}
	v->hash = h;
	v->px = px;
	v->kind = kind;
	v->src = copy;
	v->l = l;
	v->used = g_tick;
	return l;
}

static const MLayout k_empty = { 0, 0, 0, 0, NULL };

const MLayout *mlc_get(const char *src, int px)
{
	const MLayout *l = lookup(src, ml_quant(px), 0);
	return l ? l : &k_empty;
}

const MLayout *mlc_text(const char *s, int px, int weight)
{
	const MLayout *l = lookup(s, px, 1 + weight);
	return l ? l : &k_empty;
}

const MLayout *mlc_fit(const char *src, int px, int min_px, int max_w)
{
	px = ml_quant(px);
	min_px = ml_quant(min_px);
	const MLayout *l = mlc_get(src, px);
	while (l->w > max_w && px > min_px) {
		px = ml_quant(px - 1);
		l = mlc_get(src, px);
	}
	return l;
}

void mlc_new_frame(void) { g_tick++; }

void mlc_flush(void)
{
	for (int i = 0; i < CACHE_N; i++) {
		if (g_e[i].l) {
			ml_free(g_e[i].l);
			free(g_e[i].src);
		}
		memset(&g_e[i], 0, sizeof g_e[i]);
	}
}
