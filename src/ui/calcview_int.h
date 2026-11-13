/* calcview_int.h - state shared by calcview.c (logic) and calcview_draw.c. */
#ifndef UI_CALCVIEW_INT_H
#define UI_CALCVIEW_INT_H

#include "ui/app.h"
#include "ui/editor.h"
#include "ui/keypad.h"
#include "ui/result.h"

/* layout (left display pane, right keypad pane) */
#define CV_PANE_X 10
#define CV_PANE_W 530
#define CV_HIST_Y (BODY_Y + 8)
#define CV_HIST_H 158
#define CV_ED_Y (CV_HIST_Y + CV_HIST_H + 6)
#define CV_ED_H 92
#define CV_ANS_Y (CV_ED_Y + CV_ED_H + 6)
#define CV_ANS_H 100
#define CV_ACT_Y (CV_ANS_Y + CV_ANS_H + 8)
#define CV_ACT_H 34
#define CV_ACT_GAP 6
#define CV_ACT_COLS 3
/* the "Steps" pill in the answer area (drawn and hit-tested) */
#define CV_PILL_W 104
#define CV_PILL_H 26
#define CV_PILL_X (CV_PANE_X + CV_PANE_W - CV_PILL_W - 10)
#define CV_PILL_Y (CV_ANS_Y + 8)
#define CV_KP_X 554
#define CV_KP_Y (BODY_Y + 10)
#define CV_KP_W 396
#define CV_KP_H (BODY_H - 18)

enum { FOC_KEYPAD = 0, FOC_EDITOR, FOC_HISTORY, FOC_ACTIONS };

#define CV_MAX_SUBJ 8

typedef struct {
	Editor ed;
	Res cur;
	bool has_cur;
	bool show_alt;
	bool fresh;          /* just evaluated: typing starts a new sum */
	int focus;
	int key_sel;
	int hist_sel;        /* history index while FOC_HISTORY */
	int act_sel;
	int pressed_key;
	uint64_t pressed_until;
	int pressed_act;
	int ed_scroll;
	int hist_scroll;     /* px scrolled up from the newest entry */
	bool pick_subject;
	char subj[CV_MAX_SUBJ][12];
	int nsubj;
	bool err_edit;       /* the error on screen can be sent back to the editor */
	unsigned err_ver;    /* editor version when the error was made */
} CalcState;

extern CalcState g_cs[3];

static inline int cv_keyset(int mode)
{
	return mode == MODE_SCI ? KS_SCI : mode == MODE_ALG ? KS_ALG : KS_CALC;
}

/* action row */
int cv_action_count(int mode);
const char *cv_action_label(int mode, int i);
void cv_action_rect(int i, int *x, int *y, int *w, int *h);

/* drawing helpers shared with steps/history */
int cv_hist_entry_h(int i);
int cv_hist_hit(int mode, int ty);
void cv_run(int mode, CalcAction a, const char *subject);
void cv_do_action(int mode, int i);
void cv_empty_hint(int mode, CalcAction a);
void cv_same_hint(int mode, CalcAction a, const Res *r, const char *input);
void cv_insert_ans(int mode);
void cv_osk_accept(int mode, const char *text);

/* calcview_nav.c */
void cv_nav(int mode, const input_t *in);
void cv_hints(int mode);
void cv_hist_use(int mode, int i, bool answer);
bool cv_edit_error(int mode);       /* BACK on an error: back to the input, cursor at the fault */
bool cv_err_edit_ready(int mode);

#endif
