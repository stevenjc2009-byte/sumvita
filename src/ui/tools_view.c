/* tools_view.c - GCSE mode: category list -> tool list -> form (each field
 * edited in the maths sheet, choices cycled in place) -> result + steps. */
#include "ui/tools_view_int.h"

#include "ui/draw.h"
#include "ui/keypad.h"
#include "ui/settings.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

ToolsState g_tv;

int tv_choice_count(const char *choices)
{
	if (!choices || !*choices)
		return 0;
	int n = 1;
	for (const char *p = choices; *p; p++)
		if (*p == '|')
			n++;
	return n;
}

void tv_choice(const char *choices, int i, char *out, int cap)
{
	out[0] = 0;
	if (!choices)
		return;
	const char *p = choices;
	while (i > 0 && *p) {
		if (*p++ == '|')
			i--;
	}
	int n = 0;
	while (p[n] && p[n] != '|')
		n++;
	snprintf(out, (size_t)cap, "%.*s", n, p);
}

void tv_load_tools(void)
{
	int n = 0;
	const Tool *all = tools_all(&n);
	g_tv.searching = false;
	g_tv.ntools = 0;
	for (int i = 0; i < n && g_tv.ntools < TV_MAX_TOOLS; i++)
		if (g_tv.ncats > 0 && strcmp(all[i].category, g_tv.cats[g_tv.cat]) == 0)
			g_tv.tools[g_tv.ntools++] = &all[i];
	g_tv.tool_sel = 0;
	g_tv.tool_scroll = 0;
}

void gcse_screen_init(void)
{
	memset(&g_tv, 0, sizeof g_tv);
	res_init(&g_tv.res);
	g_tv.ncats = tools_categories(g_tv.cats, TV_MAX_CATS);
	g_tv.pressed_run = -1;
	tv_load_tools();
}

static void open_tool(const Tool *t, bool examples)
{
	g_tv.t = t;
	g_tv.page = TV_FORM;
	g_tv.field = 0;
	res_free(&g_tv.res);
	g_tv.has_res = false;
	for (int i = 0; i < t->nfields && i < TOOL_MAX_FIELDS; i++) {
		const ToolField *f = &t->fields[i];
		const char *v = f->def ? f->def : "";
		if (f->kind == TF_CHOICE && !*v) {
			tv_choice(f->choices, 0, g_tv.vals[i], TV_VAL_MAX);
			continue;
		}
		/* the tour fills the hint's example ("e.g. 240") so the result is real */
		if (examples && !*v && f->hint && strncmp(f->hint, "e.g. ", 5) == 0)
			v = f->hint + 5;
		snprintf(g_tv.vals[i], TV_VAL_MAX, "%s", v);
	}
}

static void field_done(const char *text, void *ctx)
{
	int i = (int)(intptr_t)ctx;
	if (i >= 0 && i < TOOL_MAX_FIELDS)
		snprintf(g_tv.vals[i], TV_VAL_MAX, "%s", text);
}

static void cycle_choice(int i, int d)
{
	const ToolField *f = &g_tv.t->fields[i];
	int n = tv_choice_count(f->choices);
	if (n == 0)
		return;
	int cur = 0;
	char c[64];
	for (int k = 0; k < n; k++) {
		tv_choice(f->choices, k, c, sizeof c);
		if (strcmp(c, g_tv.vals[i]) == 0)
			cur = k;
	}
	tv_choice(f->choices, (cur + d + n) % n, g_tv.vals[i], TV_VAL_MAX);
}

static void run_tool(void)
{
	const Tool *t = g_tv.t;
	for (int i = 0; i < t->nfields; i++)
		if (!t->fields[i].optional && !g_tv.vals[i][0]) {
			char msg[96];
			snprintf(msg, sizeof msg, "Fill in \"%s\" first", t->fields[i].label);
			app_toast(msg);
			g_tv.field = i;
			return;
		}
	const char *vals[TOOL_MAX_FIELDS];
	for (int i = 0; i < TOOL_MAX_FIELDS; i++)
		vals[i] = g_tv.vals[i];
	CalcSettings cs;
	settings_calc(&cs);
	res_free(&g_tv.res);
	res_run_tool(&g_tv.res, t, vals, &cs);
	g_tv.has_res = true;
	g_tv.field = t->nfields;
}

static void activate_field(int i)
{
	const Tool *t = g_tv.t;
	if (i >= t->nfields) {
		run_tool();
		return;
	}
	const ToolField *f = &t->fields[i];
	if (f->kind == TF_CHOICE) {
		cycle_choice(i, 1);
		return;
	}
	sheet_set_keyset(f->kind == TF_LIST ? KS_LIST : f->kind == TF_NUMBER ? KS_NUMBER : KS_ENTRY);
	app_open_sheet(f->label, f->hint, g_tv.vals[i], field_done, (void *)(intptr_t)i);
}

static void list_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_DPAD, "Choose" }, { GLYPH_CONFIRM, "Open" }, { GLYPH_TRIANGLE, "Search" }, { GLYPH_LR, "Mode" },
	};
	static const Hint shints[] = {
		{ GLYPH_DPAD, "Choose" }, { GLYPH_CONFIRM, "Open" }, { GLYPH_TRIANGLE, "Search again" },
		{ GLYPH_BACK, "Clear search" }, { GLYPH_LR, "Mode" },
	};
	if (g_tv.searching)
		app_hints(shints, 5);
	else
		app_hints(hints, 4);
	if (in->pressed & IN_TRIANGLE)
		tv_search_prompt();
	if (g_tv.searching && (in->pressed & (IN_BACK | IN_LEFT))) {
		tv_search_clear();
		g_tv.on_tools = false;
		return;
	}
	if (!g_tv.on_tools) {
		int prev = g_tv.cat;
		if (in->pressed & IN_UP) g_tv.cat = (g_tv.cat + g_tv.ncats - 1) % (g_tv.ncats ? g_tv.ncats : 1);
		if (in->pressed & IN_DOWN) g_tv.cat = (g_tv.cat + 1) % (g_tv.ncats ? g_tv.ncats : 1);
		if (g_tv.cat != prev) tv_load_tools();
		if ((in->pressed & (IN_RIGHT | IN_CONFIRM)) && g_tv.ntools > 0) g_tv.on_tools = true;
	} else {
		int n = g_tv.ntools;
		if (n > 0 && (in->pressed & IN_UP)) g_tv.tool_sel = (g_tv.tool_sel + n - 1) % n;
		if (n > 0 && (in->pressed & IN_DOWN)) g_tv.tool_sel = (g_tv.tool_sel + 1) % n;
		if (in->pressed & (IN_LEFT | IN_BACK)) g_tv.on_tools = false;
		else if (n > 0 && (in->pressed & IN_CONFIRM)) open_tool(g_tv.tools[g_tv.tool_sel], false);
	}
	if (in->tap) {
		if (in_rect(in->tap_x, in->tap_y, TV_CAT_X, TV_LIST_Y, TV_CAT_W, 40)) {
			tv_search_prompt();
		} else if (in_rect(in->tap_x, in->tap_y, TV_CAT_X, TV_CAT_ROW_Y, TV_CAT_W, g_tv.ncats * TV_CAT_ROW_H)) {
			int r = (in->tap_y - TV_CAT_ROW_Y) / TV_CAT_ROW_H;
			if (r != g_tv.cat || g_tv.searching) { g_tv.cat = r; tv_load_tools(); }
			g_tv.on_tools = false;
		} else if (in_rect(in->tap_x, in->tap_y, TV_TOOL_X, TV_TOOL_ROW_Y, TV_TOOL_W, TV_TOOL_ROWS * TV_TOOL_ROW_H)) {
			int r = g_tv.tool_scroll + (in->tap_y - TV_TOOL_ROW_Y) / TV_TOOL_ROW_H;
			if (r < g_tv.ntools) {
				g_tv.on_tools = true;
				if (r == g_tv.tool_sel) open_tool(g_tv.tools[r], false);
				g_tv.tool_sel = r;
			}
		}
	}
	if (in->dragging && in_rect(in->began_x, in->began_y, TV_TOOL_X, TV_LIST_Y, TV_TOOL_W, TV_LIST_H)) {
		static int acc;
		acc += in->drag_dy;
		while (acc <= -TV_TOOL_ROW_H) { g_tv.tool_scroll++; acc += TV_TOOL_ROW_H; }
		while (acc >= TV_TOOL_ROW_H) { g_tv.tool_scroll--; acc -= TV_TOOL_ROW_H; }
	} else if (g_tv.on_tools) {
		if (g_tv.tool_sel < g_tv.tool_scroll) g_tv.tool_scroll = g_tv.tool_sel;
		if (g_tv.tool_sel >= g_tv.tool_scroll + TV_TOOL_ROWS) g_tv.tool_scroll = g_tv.tool_sel - TV_TOOL_ROWS + 1;
	}
	int maxs = g_tv.ntools - TV_TOOL_ROWS;
	if (g_tv.tool_scroll > maxs) g_tv.tool_scroll = maxs;
	if (g_tv.tool_scroll < 0) g_tv.tool_scroll = 0;
}

static void form_update(const input_t *in)
{
	static const Hint hints[] = {
		{ GLYPH_DPAD, "Choose" }, { GLYPH_CONFIRM, "Edit" }, { GLYPH_START, "Work it out" },
		{ GLYPH_TRIANGLE, "Steps" }, { GLYPH_BACK, "Tools" },
	};
	app_hints(hints, 5);
	const Tool *t = g_tv.t;
	int n = t->nfields + 1;
	if (in->pressed & IN_BACK) { g_tv.page = TV_LIST; return; }
	if (in->pressed & IN_UP) g_tv.field = (g_tv.field + n - 1) % n;
	if (in->pressed & IN_DOWN) g_tv.field = (g_tv.field + 1) % n;
	if (g_tv.field < t->nfields && t->fields[g_tv.field].kind == TF_CHOICE) {
		if (in->pressed & IN_LEFT) cycle_choice(g_tv.field, -1);
		if (in->pressed & IN_RIGHT) cycle_choice(g_tv.field, 1);
	}
	if ((in->pressed & IN_SQUARE) && g_tv.field < t->nfields)
		g_tv.vals[g_tv.field][0] = 0;
	if (in->pressed & IN_CONFIRM) activate_field(g_tv.field);
	if (in->pressed & IN_START) run_tool();
	if ((in->pressed & IN_TRIANGLE) && g_tv.has_res)
		app_open_steps(&g_tv.res);
	if (in->tap) {
		if (in_rect(in->tap_x, in->tap_y, TF_X, TF_ROW_Y, TF_W, t->nfields * TF_ROW_H)) {
			int r = (in->tap_y - TF_ROW_Y) / TF_ROW_H;
			g_tv.field = r;
			activate_field(r);
		} else if (in_rect(in->tap_x, in->tap_y, TF_SIDE_X, BODY_Y, TF_SIDE_W, 150)) {
			run_tool();
		} else if (g_tv.has_res && in_rect(in->tap_x, in->tap_y, TF_SIDE_X, BODY_Y + 150, TF_SIDE_W, BODY_H - 150)) {
			app_open_steps(&g_tv.res);
		}
	}
}

void gcse_screen_update(const input_t *in)
{
	if (g_tv.page == TV_FORM && g_tv.t)
		form_update(in);
	else
		list_update(in);
}

void gcse_screen_draw(void)
{
	if (g_tv.page == TV_FORM && g_tv.t)
		tv_draw_form();
	else
		tv_draw_list();
}

void gcse_tour(int what)
{
	if (what == 0) {
		g_tv.page = TV_LIST;
		g_tv.on_tools = true;
		return;
	}
	if (g_tv.ntools == 0)
		return;
	if (!g_tv.t || what == 1)
		open_tool(g_tv.tools[g_tv.tool_sel], true);
	if (what == 2)
		run_tool();
}

bool gcse_open_tool_id(const char *id)
{
	const Tool *t = tools_find(id);
	if (!t)
		return false;
	for (int c = 0; c < g_tv.ncats; c++)
		if (!strcmp(g_tv.cats[c], t->category))
			gcse_show_cat(c);
	for (int i = 0; i < g_tv.ntools; i++)
		if (g_tv.tools[i] == t)
			g_tv.tool_sel = i;
	g_tv.on_tools = true;
	open_tool(t, false);
	return true;
}

void gcse_set_field(int i, const char *text)
{
	if (g_tv.t && i >= 0 && i < g_tv.t->nfields)
		snprintf(g_tv.vals[i], TV_VAL_MAX, "%s", text);
}

void gcse_run(void)
{
	if (g_tv.t)
		run_tool();
}

void gcse_show_cat(int cat)
{
	if (cat < 0 || cat >= g_tv.ncats)
		return;
	g_tv.page = TV_LIST;
	if (cat != g_tv.cat || g_tv.searching) {
		g_tv.cat = cat;
		tv_load_tools();
	}
	g_tv.on_tools = false;
}
