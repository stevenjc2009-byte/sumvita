/* Input clean-up in front of the parser (parse_norm.c): typed command words,
 * Unicode maths characters, and X typed for x. Internal to parse.c and calc.c. */
#ifndef ENGINE_PARSE_NORM_H
#define ENGINE_PARSE_NORM_H

#include "calc.h"
#include "parse_int.h"

/* Rewrites ² ³ (as ^2 ^3), ½ ¼ ¾ (as (1/2) ..., or 2_1/2 after a digit), – — (as -) and
 * ** (as ^). Returns src itself (*map = NULL) when nothing changes; else a new arena
 * string and *map, where map[i] is the byte offset in src of byte i of the result
 * (map has strlen(result)+1 entries) so an error position can be mapped back. */
const char *pn_normalise(const char *src, int **map);

/* Leading command word: solve simplify expand factorise factorize factor rearrange (any
 * case), then optional ':' and spaces. Returns the offset of the maths after it and sets
 * *act; returns 0 and leaves *act alone when there is no command word. */
int pn_command(const char *s, CalcAction *act);

/* "x^2+5X+6": when both X and x are used, X means x (an input with only X is left alone). */
void pn_unify_x(Tok *toks, int n);

#endif
