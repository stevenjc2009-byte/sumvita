/* Internal to the parser and printer: tokens of DISPLAY SYNTAX (docs/SYNTAX.md).
 * parse_lex.c turns the text into tokens; parse.c builds the Expr tree;
 * print.c uses lex_keyword_spans() to avoid printing "p" "i" as "pi". */
#ifndef ENGINE_PARSE_INT_H
#define ENGINE_PARSE_INT_H

#include <stdbool.h>
#include "expr.h"

typedef enum {
    TK_END, TK_ATOM, TK_FUNC, TK_PLUS, TK_MINUS, TK_PM, TK_TIMES, TK_DIV, TK_POW,
    TK_BANG, TK_PCT, TK_DEG, TK_LP, TK_RP, TK_BAR, TK_COMMA, TK_REL, TK_TEXT
} TokKind;

/* TK_FUNC variants */
enum { FV_CALL = 0, FV_ROOTSIGN = 1, FV_CBRT = 2 };

typedef struct {
    TokKind k;
    int pos, len;       /* byte span in the source */
    int op;             /* TK_FUNC: F_*; TK_REL: R_*; TK_LP/TK_RP: bracket char */
    int variant;        /* TK_FUNC: FV_* */
    Expr *atom;         /* TK_ATOM: number, variable, constant, mixed, recurring */
    const char *text;   /* TK_TEXT */
} Tok;

/* Tokenise src. On error returns false with message + byte position. */
bool lex_all(const char *src, Tok **out, int *ntok, char *err, int errcap, int *err_pos);

/* True if some keyword (function name, pi, Ans, recur) starts in the first
 * `split` bytes of s and ends after them (s is ASCII letters only). */
bool lex_keyword_spans(const char *s, int split);

#endif
