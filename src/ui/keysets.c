/* keysets.c - the key tables for each mode. */
#include "ui/keypad.h"

#include <stddef.h>

#define T(l, s)        { l, KA_TEXT, s, ICON_NONE, 0, l }
#define TI(l, s, i)    { l, KA_TEXT, s, ICON_NONE, 0, i }
#define TM(l, k, ic)   { l, KA_TMPL, KST_FN, ic, k, NULL }
#define FN(l, n)       { l, KA_FUNC, KST_FN, ICON_NONE, 0, n }
#define FNI(l, n)      { l, KA_FUNC, KST_FN, ICON_INV, 0, n }
#define F2(l, n)       { l, KA_FUNC2, KST_FN, ICON_NONE, 0, n }
#define CMD(l, c, s)   { l, KA_CMD, s, ICON_NONE, c, NULL }
#define D(l)           T(l, KST_NUM)
#define OP(l)          T(l, KST_OP)

#define MINUS "\xE2\x88\x92"
#define TIMES "\xC3\x97"
#define DIVIDE "\xC3\xB7"
#define BKSP "\xE2\x8C\xAB"
#define KBD "\xE2\x8C\xA8"
#define SD "S\xE2\x87\x94" "D"
#define LEFT "\xE2\x97\x80"
#define RIGHT "\xE2\x96\xB6"
#define PI "\xCF\x80"

#define K_FRAC   TM("a/b", EK_FRAC, ICON_FRAC)
#define K_MIXED  TM("mixed", EK_MIXED, ICON_MIXED)
#define K_SQ     { "x2", KA_POW, KST_FN, ICON_SQ, 0, "2" }
#define K_POW    { "xn", KA_POW, KST_FN, ICON_POW, 0, "" }
#define K_SQRT   TM("sqrt", EK_SQRT, ICON_SQRT)
#define K_ROOT   TM("root", EK_ROOT, ICON_ROOT)
#define K_ABS    TM("abs", EK_ABS, ICON_ABS)
#define K_TEN    { "x10", KA_TEN, KST_FN, ICON_TEN, 0, NULL }
#define K_OPEN   TM("(", EK_PAREN, ICON_NONE)
#define K_CLOSE  CMD(")", KC_CLOSE, KST_OP)
#define K_AC     CMD("AC", KC_AC, KST_DEL)
#define K_DEL    CMD(BKSP, KC_DEL, KST_DEL)
#define K_LEFT   CMD(LEFT, KC_LEFT, KST_FN)
#define K_RIGHT  CMD(RIGHT, KC_RIGHT, KST_FN)
#define K_EQ     CMD("=", KC_EQ, KST_EQ)
#define K_SD     CMD(SD, KC_SD, KST_FN)
#define K_OSK    CMD(KBD, KC_OSK, KST_FN)
#define K_MORE   CMD("more", KC_MORE, KST_FN)
#define K_OK     CMD("OK", KC_OK, KST_EQ)
#define REL(l)   T(l, KST_LETTER)   /* = < > <= >= : a sign in the maths, not "work it out" */
#define K_DEG    CMD("DEG", KC_DEGRAD, KST_FN)
#define K_ANS    TI("Ans", KST_FN, "Ans")
#define K_MINUS  TI(MINUS, KST_OP, MINUS)
#define K_TIMES  TI(TIMES, KST_OP, TIMES)
#define K_DIVIDE TI(DIVIDE, KST_OP, DIVIDE)

static const Key k_calc[] = {
	K_AC, K_DEL, K_OPEN, K_CLOSE, K_DIVIDE,
	D("7"), D("8"), D("9"), K_FRAC, K_TIMES,
	D("4"), D("5"), D("6"), K_MIXED, K_MINUS,
	D("1"), D("2"), D("3"), OP("%"), OP("+"),
	D("0"), D("."), K_ANS, K_SD, K_EQ,
	K_LEFT, K_RIGHT, K_SQ, K_SQRT, K_OSK,
};

static const Key k_sci[] = {
	K_DEG, K_SD, K_ANS, K_LEFT, K_RIGHT, K_OSK,
	FN("sin", "sin"), FN("cos", "cos"), FN("tan", "tan"), FN("ln", "ln"), FN("log", "log"), TI(PI, KST_FN, PI),
	FNI("sin", "asin"), FNI("cos", "acos"), FNI("tan", "atan"), TI("e", KST_FN, "e"), F2("nCr", "nCr"), F2("nPr", "nPr"),
	K_SQ, K_POW, K_SQRT, K_ROOT, K_FRAC, K_ABS,
	D("7"), D("8"), D("9"), K_OPEN, K_CLOSE, K_AC,
	D("4"), D("5"), D("6"), K_TIMES, K_DIVIDE, K_DEL,
	D("1"), D("2"), D("3"), OP("+"), K_MINUS, OP("!"),
	D("0"), D("."), K_TEN, OP("%"), OP("\xC2\xB0"), K_EQ,
};

static const Key k_alg[] = {
	T("x", KST_LETTER), T("y", KST_LETTER), T("a", KST_LETTER), T("b", KST_LETTER), T("c", KST_LETTER), K_OSK,
	REL("="), REL("<"), REL(">"), REL("\xE2\x89\xA4"), REL("\xE2\x89\xA5"), OP(","),
	K_SQ, K_POW, K_SQRT, K_FRAC, K_OPEN, K_CLOSE,
	D("7"), D("8"), D("9"), K_TIMES, K_DIVIDE, K_AC,
	D("4"), D("5"), D("6"), OP("+"), K_MINUS, K_DEL,
	D("1"), D("2"), D("3"), K_LEFT, K_RIGHT, T("n", KST_LETTER),
	D("0"), D("."), TI(PI, KST_FN, PI), K_ANS, T("k", KST_LETTER), CMD("Go", KC_EQ, KST_EQ),
};

static const Key k_entry[] = {
	T("x", KST_LETTER), OP(","), K_LEFT, K_RIGHT, K_OSK, K_DEG,
	FN("sin", "sin"), FN("cos", "cos"), FN("tan", "tan"), FN("ln", "ln"), FN("log", "log"), TI(PI, KST_FN, PI),
	FNI("sin", "asin"), FNI("cos", "acos"), FNI("tan", "atan"), TI("e", KST_FN, "e"), F2("nCr", "nCr"), F2("nPr", "nPr"),
	K_SQ, K_POW, K_SQRT, K_ROOT, K_FRAC, K_ABS,
	D("7"), D("8"), D("9"), K_OPEN, K_CLOSE, K_AC,
	D("4"), D("5"), D("6"), K_TIMES, K_DIVIDE, K_DEL,
	D("1"), D("2"), D("3"), OP("+"), K_MINUS, OP("!"),
	D("0"), D("."), K_TEN, OP("%"), OP("\xC2\xB0"), CMD("OK", KC_OK, KST_EQ),
};

/* the field sheet's big-key pads: a plain number, and a comma separated list.
 * "more" swaps to the full entry keypad (sheet.c). */
static const Key k_number[] = {
	D("7"), D("8"), D("9"), K_DEL, K_AC,
	D("4"), D("5"), D("6"), K_MINUS, K_FRAC,
	D("1"), D("2"), D("3"), D("."), K_DIVIDE,
	D("0"), K_LEFT, K_RIGHT, K_MORE, K_OK,
};

static const Key k_list[] = {
	D("7"), D("8"), D("9"), K_DEL, K_AC,
	D("4"), D("5"), D("6"), K_MINUS, OP(","),
	D("1"), D("2"), D("3"), D("."), K_FRAC,
	D("0"), K_LEFT, K_RIGHT, K_MORE, K_OK,
};

static const Keyset k_sets[KS_COUNT] = {
	{ k_calc, 5, 6 },
	{ k_sci, 6, 8 },
	{ k_alg, 6, 7 },
	{ k_entry, 6, 8 },
	{ k_number, 5, 4 },
	{ k_list, 5, 4 },
};

const Keyset *keyset_get(int id)
{
	if (id < 0 || id >= KS_COUNT)
		id = 0;
	return &k_sets[id];
}
