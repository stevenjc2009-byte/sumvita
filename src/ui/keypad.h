/* keypad.h - on-screen keypads (one keyset per mode), drawing, hit testing and
 * applying a key to an Editor. */
#ifndef UI_KEYPAD_H
#define UI_KEYPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "ui/editor.h"

typedef enum { KA_TEXT = 0, KA_TMPL, KA_FUNC, KA_FUNC2, KA_POW, KA_TEN, KA_CMD } KeyAct;
typedef enum { KST_NUM = 0, KST_OP, KST_FN, KST_DEL, KST_EQ, KST_LETTER } KeyStyle;
typedef enum {
	ICON_NONE = 0, ICON_FRAC, ICON_MIXED, ICON_SQ, ICON_POW, ICON_SQRT, ICON_ROOT, ICON_ABS, ICON_TEN,
	ICON_INV, ICON_COUNT
} KeyIcon;
typedef enum {
	KC_NONE = 0, KC_AC, KC_DEL, KC_LEFT, KC_RIGHT, KC_EQ, KC_SD, KC_OSK, KC_DEGRAD, KC_CLOSE, KC_OK, KC_MORE
} KeyCmd;

typedef struct {
	const char *label;  /* what the key shows (icon keys: base text, e.g. "sin") */
	uint8_t act;        /* KeyAct */
	uint8_t style;      /* KeyStyle */
	uint8_t icon;       /* KeyIcon */
	uint8_t arg;        /* KA_TMPL: EdKind; KA_CMD: KeyCmd */
	const char *ins;    /* KA_TEXT: token; KA_FUNC*: name; KA_POW: exponent ("" = empty) */
} Key;

typedef struct {
	const Key *k;
	int cols, rows;
} Keyset;

/* KS_NUMBER / KS_LIST: the big-key pads of the field sheet (plain number, comma list) */
enum { KS_CALC = 0, KS_SCI, KS_ALG, KS_ENTRY, KS_NUMBER, KS_LIST, KS_COUNT };

const Keyset *keyset_get(int id);
/* sheet.c: the keypad the next sheet_open uses (KS_ENTRY unless set; one use) */
void sheet_set_keyset(int id);

void keypad_rect(const Keyset *ks, int x, int y, int w, int h, int idx, int *kx, int *ky, int *kw, int *kh);
int  keypad_hit(const Keyset *ks, int x, int y, int w, int h, int tx, int ty);
/* sel: focused key (-1 none); pressed: key flashing as pressed (-1 none). */
void keypad_draw(const Keyset *ks, int x, int y, int w, int h, int sel, int pressed, bool degrees);

/* Edits the editor for text/template keys and returns KC_NONE; for command
 * keys returns the KeyCmd for the screen to handle. */
int key_apply(const Key *k, Editor *ed);

#endif
