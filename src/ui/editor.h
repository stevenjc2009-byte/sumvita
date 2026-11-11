/* editor.h - structured "natural display" input editor.
 *
 * The input is a tree of sequences. A sequence holds items; an item is either a
 * text token (a digit, an operator, a letter, "Ans", ...) or a template with
 * 1-3 slots, each slot being another sequence:
 *   EK_FRAC (num, den)  EK_POW (exponent; the base is whatever precedes it)
 *   EK_SQRT (radicand)  EK_ROOT (index, radicand)   EK_PAREN (inside)
 *   EK_ABS (inside)     EK_MIXED (whole, num, den)
 *   EK_FUNC (argument; name in text: "sin", "asin", "ln", ...)
 *   EK_FUNC2 (two arguments; "nCr", "nPr", "log" = log base)
 * The cursor is (sequence, position). ed_serialize() writes DISPLAY SYNTAX
 * (docs/SYNTAX.md), always √(...) with brackets. */
#ifndef UI_EDITOR_H
#define UI_EDITOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	EK_TEXT = 0, EK_FRAC, EK_POW, EK_SQRT, EK_ROOT, EK_PAREN, EK_ABS, EK_MIXED, EK_FUNC, EK_FUNC2
} EdKind;

#define ED_TEXT_MAX 8
#define ED_SEQ_MAX  80
#define ED_POOL     112

typedef struct {
	uint8_t kind;
	uint8_t nslots;
	int16_t slot[3];
	char text[ED_TEXT_MAX];
} EdItem;

typedef struct {
	EdItem it[ED_SEQ_MAX];
	int n;
	int16_t parent_seq;   /* -1 for the root */
	int16_t parent_item;
	int8_t parent_slot;
	uint8_t used;
} EdSeq;

typedef struct {
	EdSeq pool[ED_POOL];
	int root;
	int cur_seq, cur_pos;
	unsigned version;     /* bumped on every change (layout cache key) */
} Editor;

void ed_init(Editor *ed);
void ed_clear(Editor *ed);
bool ed_is_empty(const Editor *ed);

void ed_insert_text(Editor *ed, const char *tok);
/* Template with empty slots; cursor goes into the first slot. name for FUNC/FUNC2. */
void ed_insert_template(Editor *ed, EdKind k, const char *name);
/* Power: exp text pre-filled (cursor after the template) or NULL (cursor in it). */
void ed_insert_pow(Editor *ed, const char *exp_text);
/* ")" key: leave the innermost bracket template, else insert ")" as text. */
void ed_close_paren(Editor *ed);
void ed_backspace(Editor *ed);

/* dir: 0 left, 1 right, 2 up, 3 down. false when the move left the editor's
 * structure (up/down outside any vertical template): the screen may then move
 * focus elsewhere. */
bool ed_move(Editor *ed, int dir);
void ed_home(Editor *ed);
void ed_end(Editor *ed);
void ed_set_cursor(Editor *ed, int seq, int pos);

/* Writes display syntax; returns its length. */
int ed_serialize(const Editor *ed, char *out, int cap);
/* Replaces the contents by parsing display syntax (falls back to raw text
 * tokens if it does not parse). Cursor at the end. */
void ed_load(Editor *ed, const char *display);
/* Inserts parsed display syntax at the cursor. */
void ed_insert_display(Editor *ed, const char *display);

/* ---- internals shared by editor*.c ---- */
int  ed_new_seq(Editor *ed, int parent_seq, int parent_item, int parent_slot);
void ed_free_seq(Editor *ed, int seq);
EdItem *ed_put_item(Editor *ed, int seq, int pos, EdKind k, const char *text);
void ed_touch(Editor *ed);

#endif
