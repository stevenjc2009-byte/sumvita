/* osk_text.h - tidies text typed on the system keyboard before it goes into the
 * editor. Spaces are ignored the way the engine's lexer ignores them, so
 * "( x + 2 )( x + 3 )" comes in as "(x+2)(x+3)" and "x= -2 and x= -3" as
 * "x=-2and x=-3". A space is kept only where dropping it would change what the
 * lexer reads (docs/SYNTAX.md):
 *   - between two letters ("a sin(30)" is a*sin, "asin(30)" is arcsin; "and x"),
 *   - between a decimal and "(" ("0.3 (18)" is a product, "0.3(18)" recurring).
 * Text inside "quotes" is left exactly as typed. Tabs, newlines, no-break and
 * ideographic spaces count as spaces.
 * A lone capital letter (no letter either side, not E) becomes lower case when
 * the same letter is also typed lower case, so "X = -2 and x = -3" is one
 * letter, not two (the keyboard capitalises the first letter it is given).
 * Pure string code (no Vita calls), so the host tests cover it. */
#ifndef UI_OSK_TEXT_H
#define UI_OSK_TEXT_H

/* In place; the result is never longer than s. NULL is ignored. */
void osk_text_tidy(char *s);

#endif
