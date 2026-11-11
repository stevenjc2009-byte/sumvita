/* The one entry point the UI calls.
 *
 * calc_run(action, input, settings, &result):
 *   - parses `input` (DISPLAY SYNTAX, docs/SYNTAX.md),
 *   - performs `action`,
 *   - fills result.answer (DISPLAY SYNTAX, natural-display renderable),
 *     result.answer_alt (the other form for the S<->D toggle, may be ""),
 *     result.steps (working out), or result.error on failure.
 * All result strings and steps live in the current arena: the caller copies
 * what it keeps, then arena_reset()s.
 */
#ifndef ENGINE_CALC_H
#define ENGINE_CALC_H

#include <stdbool.h>
#include "steps.h"

typedef enum {
    ACT_AUTO = 0,       /* "=": numbers -> evaluate; equation -> solve; expression with
                           variables -> simplify */
    ACT_EVAL,           /* numeric value, exact where possible (fractions, surds, π) */
    ACT_SIMPLIFY,       /* collect like terms, index laws, cancel algebraic fractions */
    ACT_EXPAND,         /* multiply out brackets (any number) */
    ACT_FACTOR,         /* factorise fully (numbers: prime factorisation) */
    ACT_SOLVE,          /* solve equation / inequality / simultaneous system for its unknown(s) */
    ACT_COMPLETE_SQ,    /* write quadratic as a(x+p)^2+q, give turning point */
    ACT_REARRANGE,      /* make `target_var` the subject */
    ACT_TO_FRACTION,    /* decimal / % / recurring -> fraction in simplest form */
    ACT_TO_DECIMAL,
    ACT_TO_PERCENT,
    ACT_TO_MIXED,       /* improper fraction <-> mixed number */
    ACT_STD_FORM,       /* standard form a×10^n */
    ACT_SURD,           /* simplify surds / rationalise the denominator */
    ACT_COUNT
} CalcAction;

typedef struct {
    bool degrees;           /* angle mode (default true: GCSE uses degrees) */
    int  dp;                /* decimal places for decimal answers; -1 = auto (10 sig fig) */
    int  sf;                /* significant figures; 0 = off (dp wins when both set) */
    bool prefer_exact;      /* answer shows exact (fraction/surd); answer_alt shows decimal */
    char target_var[12];    /* ACT_REARRANGE / ACT_SOLVE override; "" = automatic */
    double ans;             /* value of Ans */
    bool ans_set;
} CalcSettings;

typedef struct {
    bool ok;
    const char *input_echo;   /* input as the engine understood it (display syntax) */
    const char *answer;       /* main answer */
    const char *answer_alt;   /* alternative form (decimal <-> exact), "" if none */
    double value;             /* numeric value when the answer is a single number */
    bool has_value;
    Steps steps;
    char error[160];
    int error_pos;            /* byte offset into input, -1 if none */
} CalcResult;

void calc_settings_default(CalcSettings *s);
void calc_run(CalcAction action, const char *input, const CalcSettings *s, CalcResult *out);

/* Short label for menus: "Expand", "Factorise", ... */
const char *calc_action_name(CalcAction a);

#endif
