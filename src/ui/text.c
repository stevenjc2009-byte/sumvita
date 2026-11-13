#include "ui/text.h"

#include "ui/font.h"

#include <string.h>

#define ELLIPSIS "\xE2\x80\xA6"

static int cp_len(const char *s, unsigned int *cp)
{
	const unsigned char *u = (const unsigned char *)s;
	if (u[0] < 0x80) { *cp = u[0]; return 1; }
	int n = (u[0] & 0xE0) == 0xC0 ? 2 : (u[0] & 0xF0) == 0xE0 ? 3 : (u[0] & 0xF8) == 0xF0 ? 4 : 1;
	unsigned int c = n == 2 ? (u[0] & 0x1Fu) : n == 3 ? (u[0] & 0x0Fu) : (u[0] & 0x07u);
	for (int i = 1; i < n; i++) {
		if ((u[i] & 0xC0) != 0x80) { *cp = 0xFFFD; return 1; }
		c = (c << 6) | (u[i] & 0x3Fu);
	}
	*cp = n == 1 ? 0xFFFD : c;
	return n;
}

static int is_cjk(unsigned int cp) { return cp >= 0x2E80; }

static void copy_span(char *dst, const char *a, const char *b)
{
	size_t n = (size_t)(b - a);
	if (n > TEXT_LINE_MAX - 1) n = TEXT_LINE_MAX - 1;
	memcpy(dst, a, n);
	dst[n] = '\0';
}

/* Removes trailing code points until text + ellipsis fits max_w, then appends it. */
static void add_ellipsis(int px, char *line, int max_w)
{
	size_t len = strlen(line);
	for (;;) {
		while (len > 0 && line[len - 1] == ' ') len--;
		line[len] = '\0';
		if (len + sizeof ELLIPSIS <= TEXT_LINE_MAX) {
			char tmp[TEXT_LINE_MAX];
			memcpy(tmp, line, len);
			memcpy(tmp + len, ELLIPSIS, sizeof ELLIPSIS);
			if (len == 0 || font_width(px, tmp) <= max_w) {
				memcpy(line, tmp, len + sizeof ELLIPSIS);
				return;
			}
		}
		/* drop the last code point */
		do { len--; } while (len > 0 && ((unsigned char)line[len] & 0xC0) == 0x80);
	}
}

/* Width of [a, b): one word or the start of one (no spaces in it). */
static int span_w(int px, const char *a, const char *b)
{
	char buf[TEXT_LINE_MAX];
	copy_span(buf, a, b);
	return font_width(px, buf);
}

/* The first character from `from` (before `limit`) whose prefix [p, c + len)
 * is wider than max_w, never p itself: the per-character rule, run only over
 * the part of the current word not yet known to fit. ws is the word's start
 * and wdone the width of [p, ws). */
static const char *first_over(int px, const char *p, const char *ws, int wdone, int max_w,
			      const char *from, const char *limit)
{
	for (const char *c = from; c < limit;) {
		unsigned int cp;
		int len = cp_len(c, &cp);
		if (c > p && wdone + span_w(px, ws, c + len) > max_w)
			return c;
		c += len;
	}
	return limit;
}

/* A line breaks before the first character whose prefix no longer fits, at the
 * last break point before it. font_width measures text between spaces on its
 * own and adds a fixed width per space, so the prefix width is the width of the
 * whole words and spaces so far (wdone) plus the current part word: each word
 * is measured at its break points, and character by character only where a
 * line overflows. Widths only grow as text is appended, so the breaks are the
 * same as measuring every prefix. */
int text_wrap(int px, const char *utf8, int max_w, int max_lines, char lines[][TEXT_LINE_MAX])
{
	int n = 0;
	const char *p = utf8 ? utf8 : "";
	int sp = font_width(px, " ");
	while (*p && n < max_lines) {
		while (*p == ' ') p++;
		if (!*p) break;
		const char *q = p, *brk = NULL;
		const char *ws = p;    /* start of the current word */
		const char *known = p; /* everything before this fits */
		int wdone = 0;         /* width of [p, ws) */
		while (*q) {
			unsigned int cp, next = 0;
			int len = cp_len(q, &cp);
			if (q + len - p >= TEXT_LINE_MAX - 4) {
				if (ws < q && wdone + span_w(px, ws, q) > max_w)
					q = first_over(px, p, ws, wdone, max_w, known, q);
				break;
			}
			if (q[len]) cp_len(q + len, &next);
			int cand = q[len] == ' ' || is_cjk(cp) || (q[len] && is_cjk(next));
			if (cp == ' ') {
				if (wdone + sp > max_w && q > p)
					break;
				wdone += sp;
				ws = known = q + len;
			} else if (cand || !q[len]) {
				int w = wdone + span_w(px, ws, q + len);
				if (w > max_w && q > p) {
					q = first_over(px, p, ws, wdone, max_w, known, q + len);
					break;
				}
				known = q + len;
				if (q[len] == ' ') {
					wdone = w;
					ws = q + len;
				}
			}
			if (cand)
				brk = q + len;
			q += len;
		}
		const char *end = !*q ? q : (brk && brk > p) ? brk : q;
		if (end == p) { /* nothing fits: take one code point anyway */
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

void text_ellipsize(int px, const char *utf8, int max_w, char out[TEXT_LINE_MAX])
{
	char one[1][TEXT_LINE_MAX];
	const char *s = utf8 ? utf8 : "";
	/* Treat the whole string as one "word" run so it is cut, not wrapped. */
	copy_span(out, s, s + strlen(s));
	if (font_width(px, out) <= max_w && strlen(s) < TEXT_LINE_MAX)
		return;
	if (text_wrap(px, s, max_w, 1, one) == 0) {
		out[0] = '\0';
		return;
	}
	memcpy(out, one[0], TEXT_LINE_MAX);
	if (strstr(out, ELLIPSIS) == NULL)
		add_ellipsis(px, out, max_w);
}
