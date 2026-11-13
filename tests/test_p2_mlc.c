/* mlayout_cache (src/ui/mlayout_cache.c) with stub layouts: 200 keys used every
 * frame must all stay cached, eviction must take the least recently used key,
 * every hit must be the right key's layout, and flushing frees everything. */
#include "testlib.h"

#include "ui/mlayout_cache.c"

#include <stdio.h>

static int g_built, g_built_total, g_freed;

static uint32_t key_hash(const char *s)
{
	uint32_t h = 5381;
	for (; *s; s++)
		h = h * 33u + (unsigned char)*s;
	return h & 0x3FFFFFFF;
}

void ml_reset(void) {}
int ml_quant(int px) { return px; }

Frag ml_text(const char *s, int px, int weight, int role)
{
	(void)role;
	Frag f = { (int)key_hash(s), px, weight, 0, 0 };
	return f;
}

Frag ml_expr(const struct Expr *e, int px)
{
	(void)e;
	Frag f = { -1, px, 0, 0, 0 };
	return f;
}

MLayout *ml_finish(Frag f)
{
	MLayout *l = calloc(1, sizeof *l);
	if (!l)
		return NULL;
	l->w = f.w;
	l->asc = f.asc;
	l->desc = f.desc;
	g_built++;
	g_built_total++;
	return l;
}

void ml_free(MLayout *l)
{
	if (l)
		g_freed++;
	free(l);
}

static char g_k[64];
static const char *key(const char *pre, int i)
{
	snprintf(g_k, sizeof g_k, "%s%d", pre, i);
	return g_k;
}

static int right(const MLayout *l, const char *s, int px, int weight)
{
	return l && l->w == (int)key_hash(s) && l->asc == px && l->desc == weight;
}

int main(void)
{
	/* 200 keys a frame (60 history rows x 2 plus the screen): all hits after one frame */
	int bad = 0;
	for (int frame = 0; frame < 3; frame++) {
		mlc_new_frame();
		for (int i = 0; i < 200; i++)
			if (!right(mlc_text(key("row", i), 18, 0), key("row", i), 18, 0))
				bad++;
	}
	CHECK(bad == 0, "%d wrong layouts", bad);
	CHECK(g_built == 200, "200 keys over 3 frames built %d layouts (want 200)", g_built);

	/* least recently used goes first */
	mlc_flush();
	g_built = 0;
	for (int i = 0; i < 256; i++)
		mlc_text(key("k", i), 20, 1);
	mlc_text(key("k", 0), 20, 1);        /* k0 is now the newest */
	mlc_text("new one", 20, 1);          /* evicts k1, the oldest */
	int before = g_built;
	const MLayout *l0 = mlc_text(key("k", 0), 20, 1);
	CHECK(g_built == before, "k0 was evicted although just used");
	CHECK(right(l0, "k0", 20, 1), "k0 layout wrong");
	mlc_text(key("k", 2), 20, 1);
	CHECK(g_built == before, "k2 was evicted instead of k1");
	mlc_text(key("k", 1), 20, 1);
	CHECK(g_built == before + 1, "k1 should have been evicted and rebuilt");

	/* size and weight are part of the key */
	const MLayout *a = mlc_text("same", 20, 0), *b = mlc_text("same", 22, 0), *c = mlc_text("same", 20, 2);
	CHECK(a != b && a != c && b != c, "px / weight not part of the key");
	CHECK(right(a, "same", 20, 0) && right(b, "same", 22, 0) && right(c, "same", 20, 2), "px / weight layouts wrong");
	CHECK(mlc_text("same", 20, 0) == a, "repeat lookup not a hit");

	/* churn: thousands of keys through the buckets, every answer the right one */
	bad = 0;
	unsigned int seed = 99;
	for (int i = 0; i < 20000; i++) {
		seed = seed * 1103515245u + 12345u;
		int k = (int)((seed >> 8) % 700), px = 14 + (int)((seed >> 4) % 3) * 2, w = (int)(seed % 3);
		if (!right(mlc_text(key("c", k), px, w), key("c", k), px, w))
			bad++;
	}
	CHECK(bad == 0, "%d wrong layouts under churn", bad);

	/* maths keys go through the parser path */
	const MLayout *m = mlc_get("x^2+1", 18);
	CHECK(m && m->asc == 18, "mlc_get gave no layout");
	CHECK(mlc_get("x^2+1", 18) == m, "mlc_get repeat not a hit");
	CHECK(mlc_text("x^2+1", 18, 0) != m, "maths and plain text share a key");

	mlc_flush();
	CHECK(g_built_total == g_freed, "flush left %d layouts allocated", g_built_total - g_freed);
	return test_finish();
}
