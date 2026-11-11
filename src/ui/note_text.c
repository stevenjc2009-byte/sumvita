/* note_text.c - ASCII maths in step titles and notes -> what the maths line
 * shows (note_text.h). */
#include "ui/note_text.h"

#include <stdlib.h>
#include <string.h>

/* UTF-8 superscript for one character, or NULL if it has none we can show
 * (Ubuntu has ¹²³, DejaVu the rest). */
static const char *sup(char c)
{
	static const char *const DIG[10] = {
		"\xE2\x81\xB0", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4",
		"\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9",
	};
	if (c >= '0' && c <= '9')
		return DIG[c - '0'];
	if (c == '-')
		return "\xE2\x81\xBB";
	if (c == '+')
		return "\xE2\x81\xBA";
	if (c == 'n')
		return "\xE2\x81\xBF";
	return NULL;
}

static int is_alnum(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Length of the exponent after a '^' at p that can be shown raised: digits,
 * "-digits", "n", or a bracket "(...)" of those characters only. *skip_br is
 * set when the brackets are dropped. 0 = leave the '^' as it is. */
static int exponent_len(const char *p, int *skip_br)
{
	*skip_br = 0;
	if (*p == '(') {
		int n = 1;
		while (p[n] && p[n] != ')' && sup(p[n]))
			n++;
		if (p[n] != ')' || n == 1)
			return 0;
		*skip_br = 1;
		return n + 1;
	}
	int n = *p == '-' ? 1 : 0;
	if (p[n] == 'n' && !is_alnum(p[n + 1]))
		return n + 1;
	int d = n;
	while (p[d] >= '0' && p[d] <= '9')
		d++;
	return d > n ? d : 0;
}

char *note_text(const char *s)
{
	if (!s)
		s = "";
	size_t len = strlen(s);
	char *out = malloc(len * 3 + 1);
	if (!out)
		return NULL;
	char *o = out;
	for (const char *p = s; *p;) {
		if (*p == '^') {
			int br;
			int n = exponent_len(p + 1, &br);
			if (n) {
				for (int i = br; i < n - br; i++) {
					const char *u = sup(p[1 + i]);
					size_t ul = strlen(u);
					memcpy(o, u, ul);
					o += ul;
				}
				p += 1 + n;
				continue;
			}
		} else if (*p == '*') {
			memcpy(o, "\xC3\x97", 2);
			o += 2;
			p++;
			continue;
		} else if (strncmp(p, "sqrt(", 5) == 0 && (p == s || !is_alnum(p[-1]))) {
			memcpy(o, "\xE2\x88\x9A(", 4);
			o += 4;
			p += 5;
			continue;
		}
		*o++ = *p++;
	}
	*o = '\0';
	return out;
}
