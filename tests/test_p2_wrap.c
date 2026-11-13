/* text_wrap (src/ui/text.c) with a stub font: the word-by-word wrap must give
 * exactly the lines of the v1.1 per-character wrap (kept below as old_wrap) on
 * many random strings, and must measure far fewer bytes. */
#include "testlib.h"

#include "ui/text.c"

#include <stdio.h>

static int g_stub;            /* 0: 8 px a byte; 1: 3..13 px depending on the byte */
static long g_measured;       /* bytes passed to font_width */

/* Shaped like font.c runs(): a fixed width per space, and each stretch between
 * spaces measured on its own, scaled with rounding (so within a word widths do
 * not simply add up, as with vita2d's scaled advances). */
int font_width(int px, const char *s)
{
	int w = 0, word = 0;
	for (const unsigned char *u = (const unsigned char *)s;; u++) {
		if (*u == ' ' || !*u) {
			w += word * px / 16;
			word = 0;
			if (!*u)
				break;
			w += (px * 5 + 8) / 16;
		} else {
			word += g_stub == 0 ? 8 : 3 + (int)((*u * 7u) % 11u);
		}
		g_measured++;
	}
	return w;
}

/* v1.1.0 text_wrap, unchanged apart from its name */
static int old_wrap(int px, const char *utf8, int max_w, int max_lines, char lines[][TEXT_LINE_MAX])
{
	int n = 0;
	const char *p = utf8 ? utf8 : "";
	char buf[TEXT_LINE_MAX];
	while (*p && n < max_lines) {
		while (*p == ' ') p++;
		if (!*p) break;
		const char *q = p, *brk = NULL;
		while (*q) {
			unsigned int cp, next = 0;
			int len = cp_len(q, &cp);
			if (q + len - p >= TEXT_LINE_MAX - 4) break;
			copy_span(buf, p, q + len);
			if (font_width(px, buf) > max_w && q > p) break;
			if (q[len]) cp_len(q + len, &next);
			if (q[len] == ' ' || is_cjk(cp) || (q[len] && is_cjk(next)))
				brk = q + len;
			q += len;
		}
		const char *end = !*q ? q : (brk && brk > p) ? brk : q;
		if (end == p) {
			unsigned int cp;
			end = p + cp_len(p, &cp);
		}
		copy_span(lines[n], p, end);
		size_t l = strlen(lines[n]);
		while (l > 0 && lines[n][l - 1] == ' ') lines[n][--l] = '\0';
		n++;
		p = end;
	}
	while (*p == ' ') p++;
	if (*p && n > 0)
		add_ellipsis(px, lines[n - 1], max_w);
	return n;
}

static unsigned int g_seed = 12345;
static unsigned int rnd(unsigned int n)
{
	g_seed = g_seed * 1103515245u + 12345u;
	return (g_seed >> 8) % n;
}

static const char *k_bits[] = {
	"a", "b", "e", "x", "7", "=", "(", ")", "-", "+", ".", ",",
	"\xC3\xA9",         /* é */
	"\xC3\x97",         /* × */
	"\xE2\x88\x9A",     /* √ */
	"\xE2\x81\xBB",     /* superscript minus */
	"\xE4\xB8\xAD",     /* CJK */
	"\xE3\x81\x82",     /* hiragana */
	"\xF0\x9F\x98\x80", /* 4-byte */
};

static void random_text(char *out, int cap)
{
	int len = 0, target = (int)rnd(400);
	while (len < target && len + 8 < cap) {
		int r = (int)rnd(100);
		if (r < 18) {
			int sp = r < 2 ? 3 : r < 4 ? 2 : 1;
			while (sp-- && len + 1 < cap) out[len++] = ' ';
			continue;
		}
		/* mostly ASCII words, some long ones, some non-ASCII */
		int wl = r < 22 ? 30 + (int)rnd(200) : 1 + (int)rnd(9);
		for (int i = 0; i < wl && len + 8 < cap; i++) {
			const char *b = rnd(10) < 8 ? k_bits[rnd(12)] : k_bits[rnd(sizeof k_bits / sizeof *k_bits)];
			size_t bl = strlen(b);
			memcpy(out + len, b, bl);
			len += (int)bl;
		}
	}
	out[len] = '\0';
}

static char g_a[12][TEXT_LINE_MAX], g_b[12][TEXT_LINE_MAX];

static int same(int px, const char *s, int max_w, int max_lines)
{
	memset(g_a, 0x5A, sizeof g_a);
	memset(g_b, 0x5A, sizeof g_b);
	int na = old_wrap(px, s, max_w, max_lines, g_a);
	int nb = text_wrap(px, s, max_w, max_lines, g_b);
	if (na != nb)
		return 0;
	for (int i = 0; i < na; i++)
		if (strcmp(g_a[i], g_b[i]) != 0)
			return 0;
	return 1;
}

int main(void)
{
	static const int k_w[] = { 0, 1, 7, 8, 9, 15, 40, 63, 64, 65, 120, 300, 333, 700, 1600, 5000 };
	static const char *k_fixed[] = {
		"", " ", "   ", "a", "a b", "  lead and trail  ", "double  space", "x=2 or x=-3",
		"Supercalifragilisticexpialidocious is a long word indeed",
		"\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD abc \xE4\xB8\xAD", "end with space ",
	};
	char text[2048];
	int bad = 0, cases = 0;
	for (int stub = 0; stub < 2; stub++) {
		g_stub = stub;
		for (size_t f = 0; f < sizeof k_fixed / sizeof *k_fixed; f++)
			for (size_t w = 0; w < sizeof k_w / sizeof *k_w; w++)
				for (int ml = 1; ml <= 5; ml += 2) {
					cases++;
					if (!same(16, k_fixed[f], k_w[w], ml)) {
						bad++;
						if (bad < 6)
							printf("FAIL fixed stub %d \"%s\" w %d lines %d\n", stub, k_fixed[f], k_w[w], ml);
					}
				}
		for (int i = 0; i < 20000; i++) {
			random_text(text, (int)sizeof text);
			int px = 12 + (int)rnd(12);
			int w = (int)rnd(10) < 2 ? k_w[rnd(sizeof k_w / sizeof *k_w)] : (int)rnd(900);
			int ml = 1 + (int)rnd(12);
			cases++;
			if (!same(px, text, w, ml)) {
				bad++;
				if (bad < 6)
					printf("FAIL random stub %d case %d px %d w %d lines %d: \"%.80s\"\n", stub, i, px, w, ml, text);
			}
		}
	}
	CHECK(bad == 0, "%d of %d wraps differ from the v1.1 wrap", bad, cases);

	/* cost: a 2000-byte paragraph at about 37 characters a line */
	char para[2100];
	int pl = 0;
	static const char *k_words[] = { "the", "answer", "is", "x", "=", "4", "because", "we", "divide", "both", "sides" };
	while (pl < 2000) {
		const char *wd = k_words[rnd(sizeof k_words / sizeof *k_words)];
		size_t wl = strlen(wd);
		memcpy(para + pl, wd, wl);
		pl += (int)wl;
		para[pl++] = ' ';
	}
	para[pl] = '\0';
	static char lines[100][TEXT_LINE_MAX];
	g_stub = 0;
	g_measured = 0;
	int n = text_wrap(16, para, 300, 100, lines);
	long now = g_measured;
	g_measured = 0;
	int n_old = old_wrap(16, para, 300, 100, lines);
	long old = g_measured;
	printf("2000-byte paragraph: %d lines, %ld bytes measured (v1.1 wrap: %d lines, %ld)\n", n, now, n_old, old);
	CHECK(n == n_old, "line count %d vs %d", n, n_old);
	CHECK(now <= 6L * pl, "measured %ld bytes for a %d-byte paragraph (want <= %d)", now, pl, 6 * pl);
	return test_finish();
}
