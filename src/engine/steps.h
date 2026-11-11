/* Working-out log.
 *
 * A calculation appends Steps as it goes. Each step is what a student would
 * write on one line of their working:
 *   title  short rule name, e.g. "Take out the common factor 2y"
 *   note   optional plain-English explanation (may be "")
 *   math   the line of maths in DISPLAY SYNTAX (see docs/SYNTAX.md), rendered
 *          by the UI in natural display. May be "" for text-only steps.
 *
 * Steps live in the current arena. depth lets a sub-calculation (e.g. the
 * quadratic factorised inside a bigger factorisation) indent under its parent;
 * the UI shows depth > 0 steps collapsed under the parent until expanded.
 */
#ifndef ENGINE_STEPS_H
#define ENGINE_STEPS_H

typedef struct {
    const char *title;
    const char *note;
    const char *math;
    int depth;
} Step;

typedef struct {
    Step *v;
    int n;
    int cap;
    int depth;          /* current nesting for new steps */
} Steps;

void steps_init(Steps *s);
/* math_fmt is printf-style. Pass NULL or "" for no maths line. */
void steps_add(Steps *s, const char *title, const char *note, const char *math_fmt, ...)
    __attribute__((format(printf, 4, 5)));
void steps_push(Steps *s);      /* following steps are one level deeper */
void steps_pop(Steps *s);
void steps_clear(Steps *s);

#endif
