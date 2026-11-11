/* result.h - an owned (malloc'd) copy of a CalcResult, and the wrappers that
 * run the engine in the UI's own arena and copy the result out at once. */
#ifndef UI_RESULT_H
#define UI_RESULT_H

#include <stdbool.h>

#include "engine/calc.h"
#include "tools/tools.h"

typedef struct {
	char *title, *note, *math;
	int depth;
} RStep;

typedef struct {
	bool ok;
	int action;          /* CalcAction, or -1 for a GCSE tool */
	char *label;         /* "Expand", tool name, ... ("" for "=") */
	char *input;         /* what was sent (display syntax) */
	char *echo;          /* engine's input echo */
	char *answer;
	char *alt;           /* "" when none */
	double value;
	bool has_value;
	char error[160];
	int error_pos;
	RStep *steps;
	int nsteps;
} Res;

void res_init(Res *r);
void res_free(Res *r);
void res_move(Res *dst, Res *src);       /* dst takes ownership; src emptied */
void res_clone(Res *dst, const Res *src);     /* deep copy */

void res_run(Res *out, CalcAction a, const char *input, const CalcSettings *s);
void res_run_tool(Res *out, const Tool *t, const char *const *vals, const CalcSettings *s);

#endif
