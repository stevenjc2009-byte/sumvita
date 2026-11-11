/* GCSE toolkit: guided solvers with fill-in fields.
 *
 * Each Tool is a form: a few labelled fields the user fills (numbers or
 * expressions in DISPLAY SYNTAX), and a run() that writes the answer and the
 * working into a CalcResult (engine/calc.h). The UI draws every tool from this
 * table generically: category list -> tool list -> form -> result + steps.
 *
 * Field values arrive as strings exactly as typed. A field may hold a list
 * ("4, 7, 7, 9") where the tool says so in its hint.
 */
#ifndef TOOLS_TOOLS_H
#define TOOLS_TOOLS_H

#include "../engine/calc.h"

#define TOOL_MAX_FIELDS 8

typedef enum {
    TF_NUMBER = 0,   /* single value / expression */
    TF_LIST,         /* comma separated list of values */
    TF_EXPR,         /* algebraic expression or equation */
    TF_CHOICE,       /* one of choices[] (pipe separated: "sin|cos|tan") */
} ToolFieldKind;

typedef struct {
    const char *label;     /* "Original price" */
    const char *hint;      /* "e.g. 240" */
    const char *def;       /* default text, "" for none */
    ToolFieldKind kind;
    const char *choices;   /* TF_CHOICE only */
    bool optional;
} ToolField;

typedef struct Tool {
    const char *id;        /* "pct_change" — stable, unique */
    const char *category;  /* one of the TOOL_CAT_* strings below */
    const char *name;      /* "Percentage change" */
    const char *desc;      /* one line: what it answers */
    int nfields;
    ToolField fields[TOOL_MAX_FIELDS];
    void (*run)(const char *const *vals, const CalcSettings *s, CalcResult *out);
} Tool;

#define TOOL_CAT_NUMBER   "Number"
#define TOOL_CAT_ALGEBRA  "Algebra"
#define TOOL_CAT_RATIO    "Ratio & Proportion"
#define TOOL_CAT_GEOMETRY "Geometry & Trig"
#define TOOL_CAT_PROB     "Probability"
#define TOOL_CAT_STATS    "Statistics"
#define TOOL_CAT_CONVERT  "Conversions"

/* All tools, in menu order. */
const Tool *tools_all(int *count);
/* Distinct categories in menu order. */
int tools_categories(const char **out, int max);
const Tool *tools_find(const char *id);

#endif
