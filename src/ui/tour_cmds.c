/* tour_cmds.c - the commands of a tour script (tour.txt), one per line:
 *   mode calc|sci|alg|graph|gcse     close overlays, switch tab
 *   input TEXT / eval ACTION         set the editor (display syntax) / run an action
 *   rearrange VAR                    rearrange for VAR
 *   steps / close                    open the working of the current answer / close overlays
 *   focus editor|keypad|history|actions,  alt 0|1 (show the S<->D form)
 *   press BTN [N], key INDEX         virtual buttons; key = d-pad to keypad INDEX, then confirm
 *   tap X Y, drag X Y DY FRAMES, wait FRAMES
 *   shot NAME                        screenshot (PC side captures, then acks)
 *   set deg|dp|sf|exact V            settings (restored when the tour ends)
 *   graph I TEXT, gview 0-3, gsel I  graph functions / plot, analysis, table, trace
 *   tool ID, field I TEXT, run, gcat I   GCSE tools
 *   hist_clear, mem LABEL, stress N, log TEXT, end */
#include "ui/app.h"

#include "ui/calcview_int.h"
#include "ui/graph_int.h"
#include "ui/history.h"
#include "ui/log.h"
#include "ui/settings.h"
#include "ui/tools_view_int.h"
#include "ui/tour_int.h"

#include <malloc.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
	const char *name;
	int act;
} k_acts[] = {
	{ "auto", ACT_AUTO }, { "eval", ACT_EVAL }, { "simplify", ACT_SIMPLIFY }, { "expand", ACT_EXPAND },
	{ "factor", ACT_FACTOR }, { "solve", ACT_SOLVE }, { "csq", ACT_COMPLETE_SQ }, { "rearrange", ACT_REARRANGE },
	{ "frac", ACT_TO_FRACTION }, { "dec", ACT_TO_DECIMAL }, { "pct", ACT_TO_PERCENT }, { "mixed", ACT_TO_MIXED },
	{ "std", ACT_STD_FORM }, { "surd", ACT_SURD },
};

static const char *k_modes[] = { "calc", "sci", "alg", "graph", "gcse" };

static int find_act(const char *s)
{
	for (size_t i = 0; i < sizeof k_acts / sizeof k_acts[0]; i++)
		if (!strcmp(s, k_acts[i].name))
			return k_acts[i].act;
	return -1;
}

static int calc_mode(void)
{
	int m = app_mode();
	return m <= MODE_ALG ? m : -1;
}

/* ---- stress: many calculations through the calc screens' own path ---- */
static const struct {
	int mode, act;
	const char *in, *subj;
} k_stress[] = {
	{ MODE_CALC, ACT_AUTO, "3/4+5/6", NULL },        { MODE_CALC, ACT_AUTO, "2^100", NULL },
	{ MODE_CALC, ACT_AUTO, "30!", NULL },            { MODE_CALC, ACT_TO_FRACTION, "recur(0.3(18))", NULL },
	{ MODE_SCI, ACT_AUTO, "sin(150)", NULL },        { MODE_SCI, ACT_AUTO, "√(48)+√(12)", NULL },
	{ MODE_SCI, ACT_STD_FORM, "123456789", NULL },   { MODE_SCI, ACT_TO_PERCENT, "3/8", NULL },
	{ MODE_ALG, ACT_FACTOR, "y(2x+4)", NULL },       { MODE_ALG, ACT_FACTOR, "2x^2y-8y+3x^2-12", NULL },
	{ MODE_ALG, ACT_EXPAND, "(x+1)(x+2)(x+3)", NULL }, { MODE_ALG, ACT_SOLVE, "x^2-3x+1=0", NULL },
	{ MODE_ALG, ACT_SOLVE, "2x+3y=12, x-y=1", NULL }, { MODE_ALG, ACT_SOLVE, "-3<2x+1≤7", NULL },
	{ MODE_ALG, ACT_COMPLETE_SQ, "2x^2+8x+3", NULL }, { MODE_ALG, ACT_REARRANGE, "y=(x+1)/(x-2)", "x" },
	{ MODE_ALG, ACT_SIMPLIFY, "(x^2-4)/(x+2)", NULL }, { MODE_ALG, ACT_SOLVE, "x^3-6x^2+11x-6=0", NULL },
	{ MODE_CALC, ACT_AUTO, "20% of 300", NULL },     { MODE_CALC, ACT_AUTO, "1/0", NULL },
};
#define N_STRESS ((int)(sizeof k_stress / sizeof k_stress[0]))

static int g_left, g_done_n;
static uint64_t g_t0;
static uint32_t g_max_us[N_STRESS];

static int stress(const char *arg)
{
	if (g_left == 0 && g_done_n == 0) {
		g_left = atoi(arg);
		memset(g_max_us, 0, sizeof g_max_us);
		g_t0 = sceKernelGetProcessTimeWide();
		log_printf("STRESS begin %d", g_left);
	}
	for (int k = 0; k < 4 && g_left > 0; k++, g_left--, g_done_n++) {
		int i = g_done_n % N_STRESS;
		uint64_t t = sceKernelGetProcessTimeWide();
		calc_screen_set_input(k_stress[i].mode, k_stress[i].in);
		cv_run(k_stress[i].mode, (CalcAction)k_stress[i].act, k_stress[i].subj);
		uint32_t us = (uint32_t)(sceKernelGetProcessTimeWide() - t);
		if (us > g_max_us[i])
			g_max_us[i] = us;
	}
	if (g_left > 0)
		return TC_AGAIN;
	log_printf("STRESS end %d calcs in %llu ms", g_done_n,
		   (unsigned long long)((sceKernelGetProcessTimeWide() - g_t0) / 1000));
	for (int i = 0; i < N_STRESS; i++)
		log_printf("STRESS max %6lu us  act %d  %s", (unsigned long)g_max_us[i], k_stress[i].act, k_stress[i].in);
	g_done_n = 0;
	return TC_YIELD;
}

static void mem(const char *label)
{
	struct mallinfo mi = mallinfo();
	SceKernelFreeMemorySizeInfo fm;
	memset(&fm, 0, sizeof fm);
	fm.size = sizeof fm;
	sceKernelGetFreeMemorySize(&fm);
	log_printf("MEM %s: heap in use %d B, heap from system %d B, free blocks %d B, history %d, sys free user %d B",
		   label, mi.uordblks, mi.arena, mi.fordblks, history_count(), fm.size_user);
}

/* d-pad from the current keypad key to idx, then confirm */
static void key_to(int mode, int idx)
{
	CalcState *s = &g_cs[mode];
	const Keyset *ks = keyset_get(cv_keyset(mode));
	s->focus = FOC_KEYPAD;
	int dr = idx / ks->cols - s->key_sel / ks->cols, dc = idx % ks->cols - s->key_sel % ks->cols;
	tq_press(tq_bits(dr < 0 ? "UP" : "DOWN"), abs(dr));
	tq_press(tq_bits(dc < 0 ? "LEFT" : "RIGHT"), abs(dc));
	tq_press(tq_bits("CONFIRM"), 1);
}

static int focus_id(const char *a)
{
	return !strcmp(a, "editor") ? FOC_EDITOR : !strcmp(a, "history") ? FOC_HISTORY
		: !strcmp(a, "actions") ? FOC_ACTIONS : FOC_KEYPAD;
}

static int calc_cmd(const char *cmd, const char *arg, int m)
{
	if (!strcmp(cmd, "input")) {
		calc_screen_set_input(m, arg);
	} else if (!strcmp(cmd, "eval")) {
		int a = find_act(arg);
		if (a < 0) log_printf("TOUR bad action '%s'", arg);
		else calc_screen_eval(m, a);
	} else if (!strcmp(cmd, "rearrange")) {
		cv_run(m, ACT_REARRANGE, arg);
	} else if (!strcmp(cmd, "focus")) {
		g_cs[m].focus = focus_id(arg);
		if (g_cs[m].focus == FOC_HISTORY) g_cs[m].hist_sel = history_count() - 1;
	} else if (!strcmp(cmd, "alt")) {
		g_cs[m].show_alt = atoi(arg) != 0;
	} else if (!strcmp(cmd, "key")) {
		key_to(m, atoi(arg));
		return TC_YIELD;
	} else {
		return -1;
	}
	return TC_NEXT;
}

static int other_cmd(const char *cmd, const char *arg)
{
	if (!strcmp(cmd, "mode")) {
		app_close_overlays();
		for (int i = 0; i < MODE_COUNT; i++)
			if (!strcmp(arg, k_modes[i])) app_set_mode(i);
	} else if (!strcmp(cmd, "steps")) {
		int m = calc_mode();
		if (m >= 0 && g_cs[m].has_cur) app_open_steps(&g_cs[m].cur);
		else if (app_mode() == MODE_GCSE && g_tv.has_res) app_open_steps(&g_tv.res);
	} else if (!strcmp(cmd, "close")) {
		app_close_overlays();
	} else if (!strcmp(cmd, "set")) {
		char k[8] = { 0 };
		int v = 0;
		if (sscanf(arg, "%7s %d", k, &v) == 2) {
			if (!strcmp(k, "deg")) g_set.degrees = v != 0;
			else if (!strcmp(k, "dp")) g_set.dp = v;
			else if (!strcmp(k, "sf")) g_set.sf = v;
			else if (!strcmp(k, "exact")) g_set.prefer_exact = v != 0;
		}
	} else if (!strcmp(cmd, "graph")) {
		int i = atoi(arg);
		const char *t = strchr(arg, ' ');
		graph_set_fn(i, t ? t + 1 : "");
	} else if (!strcmp(cmd, "gview")) {
		graph_tour(atoi(arg));
	} else if (!strcmp(cmd, "gsel")) {
		g_gs.sel = atoi(arg);
	} else if (!strcmp(cmd, "tool")) {
		if (!gcse_open_tool_id(arg)) log_printf("TOUR no tool '%s'", arg);
	} else if (!strcmp(cmd, "field")) {
		int i = atoi(arg);
		const char *t = strchr(arg, ' ');
		gcse_set_field(i, t ? t + 1 : "");
	} else if (!strcmp(cmd, "run")) {
		gcse_run();
	} else if (!strcmp(cmd, "gcat")) {
		gcse_show_cat(atoi(arg));
	} else if (!strcmp(cmd, "hist_clear")) {
		history_clear();
	} else if (!strcmp(cmd, "mem")) {
		mem(arg);
	} else if (!strcmp(cmd, "log")) {
		log_printf("TOUR note %s", arg);
	} else {
		return -1;
	}
	return TC_NEXT;
}

int tc_exec(const char *cmd, const char *arg)
{
	if (!strcmp(cmd, "shot")) {
		tour_shot_begin(arg);
		return TC_YIELD;
	}
	if (!strcmp(cmd, "press")) {
		char b[16] = { 0 };
		int n = 1;
		sscanf(arg, "%15s %d", b, &n);
		uint32_t bits = tq_bits(b);
		if (!bits) log_printf("TOUR bad button '%s'", b);
		tq_press(bits, n);
		return TC_YIELD;
	}
	if (!strcmp(cmd, "tap") || !strcmp(cmd, "drag") || !strcmp(cmd, "wait")) {
		int a = 0, b = 0, c = 0, d = 0;
		sscanf(arg, "%d %d %d %d", &a, &b, &c, &d);
		if (cmd[0] == 't') tq_tap(a, b);
		else if (cmd[0] == 'd') tq_drag(a, b, c, d);
		else tq_wait(a);
		return TC_YIELD;
	}
	if (!strcmp(cmd, "stress"))
		return stress(arg);
	if (!strcmp(cmd, "end"))
		return TC_END;
	int m = calc_mode();
	int r = m >= 0 ? calc_cmd(cmd, arg, m) : -1;
	if (r < 0)
		r = other_cmd(cmd, arg);
	if (r < 0) {
		log_printf("TOUR unknown command '%s'", cmd);
		r = TC_NEXT;
	}
	return r;
}
