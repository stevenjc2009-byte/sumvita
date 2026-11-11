/* note_text.h - step titles and notes are plain words with bits of maths in
 * ASCII ("from the x^2 and x terms", "a^2-b^2", "2*3"). note_text() rewrites
 * those bits the way the maths line would show them: powers as superscripts
 * (x^2 -> x², a^-1 -> a⁻¹, r^n -> rⁿ, r^(n-1) -> rⁿ⁻¹), * as ×, sqrt( as √(.
 * Pure string code (no Vita calls), so the host tests cover it. */
#ifndef UI_NOTE_TEXT_H
#define UI_NOTE_TEXT_H

/* Returns a malloc'd copy of s with the maths bits rewritten ("" for NULL);
 * NULL only when out of memory. */
char *note_text(const char *s);

#endif
