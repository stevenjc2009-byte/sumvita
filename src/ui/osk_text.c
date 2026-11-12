/* osk_text.c - see osk_text.h. */
#include "ui/osk_text.h"

#include <stdbool.h>
#include <string.h>

static bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }

/* bytes of the space at s (0 = not a space): ASCII blanks, U+00A0, U+3000 */
static int space_len(const char *s)
{
	if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		return 1;
	if ((unsigned char)s[0] == 0xC2 && (unsigned char)s[1] == 0xA0)
		return 2;
	if ((unsigned char)s[0] == 0xE3 && (unsigned char)s[1] == 0x80 && (unsigned char)s[2] == 0x80)
		return 3;
	return 0;
}

/* out[0..w) ends in a number with a decimal point ("0.3", "12.") */
static bool ends_in_decimal(const char *out, int w)
{
	bool dot = false;
	int i = w;
	while (i > 0 && (is_digit(out[i - 1]) || out[i - 1] == '.')) {
		if (out[--i] == '.')
			dot = true;
	}
	return dot && i < w;
}

static bool keep_space(const char *out, int w, char next)
{
	if (w == 0 || !next)
		return false;
	char prev = out[w - 1];
	if (is_letter(prev) && is_letter(next))
		return true;
	return next == '(' && ends_in_decimal(out, w);
}

static void drop_spaces(char *s)
{
	int r = 0, w = 0;
	bool quoted = false;
	while (s[r]) {
		if (s[r] == '"')
			quoted = !quoted;
		int n = quoted ? 0 : space_len(s + r);
		if (!n) {
			s[w++] = s[r++];
			continue;
		}
		while ((n = space_len(s + r)) > 0)
			r += n;
		if (keep_space(s, w, s[r]))
			s[w++] = ' ';
	}
	s[w] = 0;
}

/* a letter at i with no letter either side, outside quotes */
static bool lone_letter(const char *s, int i)
{
	return is_letter(s[i]) && !(i > 0 && is_letter(s[i - 1])) && !is_letter(s[i + 1]);
}

static void fold_capitals(char *s)
{
	bool lower[26] = { false };
	bool quoted = false;
	for (int i = 0; s[i]; i++) {
		if (s[i] == '"')
			quoted = !quoted;
		else if (!quoted && s[i] >= 'a' && s[i] <= 'z' && lone_letter(s, i))
			lower[s[i] - 'a'] = true;
	}
	quoted = false;
	for (int i = 0; s[i]; i++) {
		if (s[i] == '"')
			quoted = !quoted;
		else if (!quoted && s[i] >= 'A' && s[i] <= 'Z' && s[i] != 'E' && lone_letter(s, i) &&
			 lower[s[i] - 'A'])
			s[i] = (char)(s[i] - 'A' + 'a');
	}
}

void osk_text_tidy(char *s)
{
	if (!s)
		return;
	drop_spaces(s);
	fold_capitals(s);
}
