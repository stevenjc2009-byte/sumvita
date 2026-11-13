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

/* 256 keys: 60 history rows x 2 layouts a frame plus the screen's own fit in.
 * A slot is 28 bytes here plus its malloc'd source copy and MLayout (16 bytes
 * an op, typically 0.1-1 KB). Lookup: hash buckets; eviction: least recently
 * used, from a doubly linked list. Links are slot + 1 so 0 means none. */
#define CACHE_N 256
#define BUCKETS 512 /* power of two */

typedef struct {
	uint32_t hash;
	int px, kind; /* kind: 0 maths, 1+weight plain text */
	char *src;
	MLayout *l;
	int16_t chain;        /* next slot in the same bucket */
	int16_t newer, older; /* LRU list */
} entry_t;

static entry_t g_e[CACHE_N];
static int16_t g_bucket[BUCKETS];
static int16_t g_newest, g_oldest;
static int g_count; /* slots filled so far */
static Arena *g_parse;

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

static void lru_unlink(int i)
{
	entry_t *e = &g_e[i - 1];
	if (e->newer) g_e[e->newer - 1].older = e->older; else g_newest = e->older;
	if (e->older) g_e[e->older - 1].newer = e->newer; else g_oldest = e->newer;
	e->newer = e->older = 0;
}

static void lru_push(int i)
{
	entry_t *e = &g_e[i - 1];
	e->older = g_newest;
	e->newer = 0;
	if (g_newest) g_e[g_newest - 1].newer = (int16_t)i; else g_oldest = (int16_t)i;
	g_newest = (int16_t)i;
}

static void bucket_unlink(int i)
{
	int16_t *pp = &g_bucket[g_e[i - 1].hash & (BUCKETS - 1)];
	while (*pp && *pp != i)
		pp = &g_e[*pp - 1].chain;
	if (*pp)
		*pp = g_e[i - 1].chain;
	g_e[i - 1].chain = 0;
}

static const MLayout *lookup(const char *src, int px, int kind)
{
	if (!src)
		src = "";
	uint32_t h = fnv(src, px, kind);
	int16_t *head = &g_bucket[h & (BUCKETS - 1)];
	for (int i = *head; i; i = g_e[i - 1].chain) {
		entry_t *e = &g_e[i - 1];
		if (e->hash == h && e->px == px && e->kind == kind && !strcmp(e->src, src)) {
			if (g_newest != i) {
				lru_unlink(i);
				lru_push(i);
			}
			return e->l;
		}
	}
	MLayout *l = kind == 0 ? build(src, px) : plain(src, px, kind - 1, MR_NORMAL);
	if (!l)
		return NULL;
	char *copy = strdup(src);
	if (!copy) {
		/* cannot cache it */
		ml_free(l);
		return NULL;
	}
	int v;
	if (g_count < CACHE_N) {
		v = ++g_count;
	} else {
		v = g_oldest;
		lru_unlink(v);
		bucket_unlink(v);
		ml_free(g_e[v - 1].l);
		free(g_e[v - 1].src);
	}
	entry_t *e = &g_e[v - 1];
	e->hash = h;
	e->px = px;
	e->kind = kind;
	e->src = copy;
	e->l = l;
	e->chain = *head;
	*head = (int16_t)v;
	lru_push(v);
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

/* Recency is kept per lookup (the LRU list), so frames need no bookkeeping. */
void mlc_new_frame(void) {}

void mlc_flush(void)
{
	for (int i = 0; i < g_count; i++) {
		ml_free(g_e[i].l);
		free(g_e[i].src);
	}
	memset(g_e, 0, sizeof g_e);
	memset(g_bucket, 0, sizeof g_bucket);
	g_newest = g_oldest = 0;
	g_count = 0;
}
