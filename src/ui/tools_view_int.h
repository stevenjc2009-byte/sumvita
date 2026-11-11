/* tools_view_int.h - GCSE tools screen state, shared by tools_view.c (logic)
 * and tools_view_draw.c. Everything is generic over tools.h. */
#ifndef UI_TOOLS_VIEW_INT_H
#define UI_TOOLS_VIEW_INT_H

#include <stdbool.h>

#include "tools/tools.h"
#include "ui/app.h"
#include "ui/result.h"

#define TV_MAX_CATS 16
#define TV_MAX_TOOLS 64        /* tools in one category */
#define TV_VAL_MAX 256

/* category / tool lists */
#define TV_CAT_X 10
#define TV_CAT_W 250
#define TV_LIST_Y (BODY_Y + 8)
#define TV_LIST_H (BODY_H - 16)
#define TV_CAT_ROW_Y (TV_LIST_Y + 46)
#define TV_CAT_ROW_H 46
#define TV_TOOL_X (TV_CAT_X + TV_CAT_W + 10)
#define TV_TOOL_W (SCR_W - TV_TOOL_X - 10)
#define TV_TOOL_ROW_Y (TV_LIST_Y + 46)
#define TV_TOOL_ROW_H 58
#define TV_TOOL_ROWS ((TV_LIST_H - 52) / TV_TOOL_ROW_H)

/* form */
#define TF_X 10
#define TF_W 560
#define TF_ROW_Y (BODY_Y + 62)
#define TF_ROW_H 48
#define TF_LABEL_W 210
#define TF_SIDE_X (TF_X + TF_W + 10)
#define TF_SIDE_W (SCR_W - TF_SIDE_X - 10)

enum { TV_LIST = 0, TV_FORM };

typedef struct {
	int page;                 /* TV_LIST / TV_FORM */
	const char *cats[TV_MAX_CATS];
	int ncats;
	int cat;                  /* selected category */
	bool on_tools;            /* list focus: tool column */
	const Tool *tools[TV_MAX_TOOLS];
	int ntools;
	int tool_sel, tool_scroll;
	const Tool *t;            /* open tool */
	char vals[TOOL_MAX_FIELDS][TV_VAL_MAX];
	int field;                /* focused row; nfields = the run button */
	Res res;
	bool has_res;
	int pressed_run;
} ToolsState;

extern ToolsState g_tv;

/* number of choices in a TF_CHOICE string and the i-th one */
int tv_choice_count(const char *choices);
void tv_choice(const char *choices, int i, char *out, int cap);

void tv_draw_list(void);
/* scripted tour (tour_cmds.c) */
bool gcse_open_tool_id(const char *id);
void gcse_set_field(int i, const char *text);
void gcse_run(void);
void gcse_show_cat(int cat);
void tv_draw_form(void);

#endif
