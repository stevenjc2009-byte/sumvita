/* sumcli - run calc_run from the command line (same engine the Vita app uses).
 *
 *   build/sumcli ACTION "input" [deg|rad]
 *   build/sumcli < cases.txt           one case per line: ACTION|input   (# = comment)
 *
 * ACTION is the action name: auto eval simplify expand factor solve completesq
 * rearrange[:var] fraction decimal percent mixed stdform surd.
 * Prints the answer, the other form, any error, and every step (title / note / math).
 * Build: make -C tests build/sumcli */
#include <stdio.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/calc.h"

static const struct { const char *name; CalcAction a; } k_acts[] = {
    { "auto", ACT_AUTO },         { "eval", ACT_EVAL },          { "simplify", ACT_SIMPLIFY },
    { "expand", ACT_EXPAND },     { "factor", ACT_FACTOR },      { "solve", ACT_SOLVE },
    { "completesq", ACT_COMPLETE_SQ }, { "rearrange", ACT_REARRANGE }, { "fraction", ACT_TO_FRACTION },
    { "decimal", ACT_TO_DECIMAL }, { "percent", ACT_TO_PERCENT }, { "mixed", ACT_TO_MIXED },
    { "stdform", ACT_STD_FORM },  { "surd", ACT_SURD },
};

static int run_one(const char *act, const char *input, int degrees)
{
    char name[32] = "", var[12] = "";
    const char *colon = strchr(act, ':');
    size_t n = colon ? (size_t)(colon - act) : strlen(act);
    if (n >= sizeof name) n = sizeof name - 1;
    memcpy(name, act, n);
    if (colon) snprintf(var, sizeof var, "%s", colon + 1);

    int found = -1;
    for (size_t i = 0; i < sizeof k_acts / sizeof k_acts[0]; i++)
        if (strcmp(k_acts[i].name, name) == 0) found = (int)i;
    if (found < 0) { printf("?? unknown action '%s'\n", name); return 1; }

    CalcSettings s;
    calc_settings_default(&s);
    if (degrees >= 0) s.degrees = degrees != 0;
    snprintf(s.target_var, sizeof s.target_var, "%s", var);
    CalcResult r;
    arena_reset(arena_current());
    calc_run(k_acts[found].a, input, &s, &r);

    printf("=== %s | %s\n", act, input);
    if (!r.ok) {
        printf("ERROR: %s (pos %d)\n", r.error, r.error_pos);
    } else {
        printf("ANSWER: %s\n", r.answer ? r.answer : "(null)");
        if (r.answer_alt && *r.answer_alt) printf("ALT: %s\n", r.answer_alt);
    }
    for (int i = 0; i < r.steps.n; i++) {
        const Step *st = &r.steps.v[i];
        printf("  %*s%d. %s", st->depth * 2, "", i + 1, st->title);
        if (st->note && *st->note) printf(" -- %s", st->note);
        if (st->math && *st->math) printf("  [%s]", st->math);
        printf("\n");
    }
    return r.ok ? 0 : 2;
}

int main(int argc, char **argv)
{
    if (argc >= 3) {
        int deg = argc >= 4 ? (strcmp(argv[3], "rad") != 0) : -1;
        return run_one(argv[1], argv[2], deg) == 1;
    }
    char line[1024];
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        char *bar = strchr(line, '|');
        if (!bar) { printf("?? no '|' in: %s\n", line); continue; }
        *bar = 0;
        run_one(line, bar + 1, -1);
    }
    return 0;
}
