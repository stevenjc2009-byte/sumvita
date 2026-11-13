/* keypad.c - drawing, hit testing and key actions for the on-screen keypads. */
#include "ui/keypad.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/input.h"
#include "ui/mlayout.h"
#include "ui/theme.h"

#include <string.h>

#define GAP 6

void keypad_rect(const Keyset *ks, int x, int y, int w, int h, int idx, int *kx, int *ky, int *kw, int *kh)
{
	int c = idx % ks->cols, r = idx / ks->cols;
	int cw = (w - GAP * (ks->cols - 1)) / ks->cols;
	int rh = (h - GAP * (ks->rows - 1)) / ks->rows;
	*kx = x + c * (cw + GAP);
	*ky = y + r * (rh + GAP);
	*kw = cw;
	*kh = rh;
}

int keypad_hit(const Keyset *ks, int x, int y, int w, int h, int tx, int ty)
{
	int n = ks->cols * ks->rows;
	for (int i = 0; i < n; i++) {
		int kx, ky, kw, kh;
		keypad_rect(ks, x, y, w, h, i, &kx, &ky, &kw, &kh);
		/* half the gap belongs to each neighbour: no dead zones between keys */
		if (in_rect(tx, ty, kx - GAP / 2, ky - GAP / 2, kw + GAP, kh + GAP))
			return i;
	}
	return -1;
}

/* ---- icons: tiny natural-display templates with slot boxes ---- */
typedef struct {
	const Key *k;
	int px;
	MLayout *l;
} icon_t;
static icon_t g_ic[64];
static int g_ic_next;

static MLayout *icon_layout(const Key *k, int px)
{
	for (int i = 0; i < 64; i++)
		if (g_ic[i].l && g_ic[i].k == k && g_ic[i].px == px)
			return g_ic[i].l;
	icon_t *slot = &g_ic[g_ic_next];
	g_ic_next = (g_ic_next + 1) % 64;
	if (slot->l)
		ml_free(slot->l);
	int sp = ml_script_px(px);
	ml_reset();
	Frag f;
	switch (k->icon) {
	case ICON_FRAC:  f = ml_frac(ml_slot(sp), ml_slot(sp), px); break;
	case ICON_MIXED: f = ml_cat(ml_slot(px), ml_frac(ml_slot(sp), ml_slot(sp), px)); break;
	case ICON_SQ:    f = ml_sup(ml_text("x", px, FW_REG, MR_NORMAL), ml_text("2", sp, FW_REG, MR_NORMAL), px); break;
	case ICON_POW:   f = ml_sup(ml_text("x", px, FW_REG, MR_NORMAL), ml_slot(sp), px); break;
	case ICON_SQRT:  f = ml_root(ml_empty(), ml_slot(sp), px); break;
	case ICON_ROOT:  f = ml_root(ml_slot(ml_script_px(sp)), ml_slot(sp), px); break;
	case ICON_ABS:   f = ml_wrap(ml_slot(sp), "|", "|", px, MR_NORMAL); break;
	case ICON_TEN:   f = ml_sup(ml_text("\xC3\x97" "10", px, FW_REG, MR_NORMAL), ml_slot(sp), px); break;
	case ICON_INV:
		f = ml_sup(ml_text(k->label, px, FW_REG, MR_NORMAL), ml_text("\xE2\x88\x92" "1", sp, FW_REG, MR_NORMAL), px);
		break;
	default:         f = ml_text(k->label, px, FW_REG, MR_NORMAL); break;
	}
	slot->k = k;
	slot->px = px;
	slot->l = ml_finish(f);
	return slot->l;
}

static unsigned int key_fill(int style)
{
	switch (style) {
	case KST_NUM:    return COL_KEY_NUM;
	case KST_OP:     return COL_KEY_OP;
	case KST_DEL:    return COL_KEY_DEL;
	case KST_EQ:     return theme_accent();
	case KST_LETTER: return col_mix(COL_KEY_OP, theme_accent(), 40);
	default:         return COL_KEY_FN;
	}
}

/* Key body: edge, fill, highlight and focus ring. Shapes only, so a whole
 * keypad's bodies go to the GPU as one shape run. */
static void draw_key_body(const Key *k, int x, int y, int w, int h, bool sel, bool pressed)
{
	unsigned int fill = key_fill(k->style);
	if (pressed)
		fill = k->style == KST_EQ ? col_mix(fill, COL_TEXT, 90) : col_mix(fill, theme_accent(), 110);
	int r = h < 40 ? 7 : 9;
	int dy = pressed ? 1 : 0;
	draw_rrect(x, y + 2, w, h, r, COL_KEY_EDGE);
	draw_rrect(x, y + dy, w, h - dy, r, fill);
	/* soft top highlight */
	draw_rect(x + r, y + dy + 1, w - 2 * r, 1, col_alpha(COL_TEXT, pressed ? 10 : 22));
	if (sel)
		draw_rrect_outline(x - 2, y - 2 + dy, w + 4, h + 4 - dy, r + 2, 2, theme_accent());
}

static void draw_key_label(const Key *k, int x, int y, int w, int h, bool pressed, bool degrees)
{
	unsigned int tc = k->style == KST_EQ ? theme_on_accent() : k->style == KST_FN ? (unsigned int)COL_TEXT2 : (unsigned int)COL_TEXT;
	int dy = pressed ? 1 : 0;
	int px = h * 46 / 100;
	if (px > 26) px = 26;
	if (px < 14) px = 14;
	const char *label = k->label;
	if (k->act == KA_CMD && k->arg == KC_DEGRAD)
		label = degrees ? "DEG" : "RAD";
	int cx = x + w / 2, cy = y + dy + h / 2;
	if (k->icon != ICON_NONE) {
		MLayout *l = icon_layout(k, ml_quant(px - 2));
		int base = cy + (l->asc - l->desc) / 2;
		mdraw(l, cx - l->w / 2, base, tc);
		return;
	}
	int lp = px;
	bool word = strlen(label) > 2 && (unsigned char)label[0] < 0x80;
	if (word)
		lp = px * 78 / 100;
	lp = ml_quant(lp);
	int wm = font_width_w(FW_MED, lp, label);
	while (wm > w - 8 && lp > 12) {
		lp = ml_quant(lp - 2);
		wm = font_width_w(FW_MED, lp, label);
	}
	fweight_t fw = k->style == KST_NUM ? FW_MED : FW_REG;
	int base = cy + ml_text_asc(lp) / 2 - 1;
	int tw = fw == FW_MED ? wm : font_width_w(fw, lp, label);
	font_draw_base(fw, cx - tw / 2, base, tc, lp, label);
}

void keypad_draw(const Keyset *ks, int x, int y, int w, int h, int sel, int pressed, bool degrees)
{
	int n = ks->cols * ks->rows;
	/* bodies first, then labels: keys never overlap, so the picture is the same */
	draw_shapes_begin();
	for (int i = 0; i < n; i++) {
		int kx, ky, kw, kh;
		keypad_rect(ks, x, y, w, h, i, &kx, &ky, &kw, &kh);
		draw_key_body(&ks->k[i], kx, ky, kw, kh, i == sel, i == pressed);
	}
	draw_shapes_end();
	for (int i = 0; i < n; i++) {
		int kx, ky, kw, kh;
		keypad_rect(ks, x, y, w, h, i, &kx, &ky, &kw, &kh);
		draw_key_label(&ks->k[i], kx, ky, kw, kh, i == pressed, degrees);
	}
}

int key_apply(const Key *k, Editor *ed)
{
	switch (k->act) {
	case KA_TEXT:
		ed_insert_text(ed, k->ins);
		return KC_NONE;
	case KA_TMPL:
		ed_insert_template(ed, (EdKind)k->arg, NULL);
		return KC_NONE;
	case KA_FUNC:
		ed_insert_template(ed, EK_FUNC, k->ins);
		return KC_NONE;
	case KA_FUNC2:
		ed_insert_template(ed, EK_FUNC2, k->ins);
		return KC_NONE;
	case KA_POW:
		ed_insert_pow(ed, k->ins && *k->ins ? k->ins : NULL);
		return KC_NONE;
	case KA_TEN:
		ed_insert_text(ed, "\xC3\x97");
		ed_insert_text(ed, "1");
		ed_insert_text(ed, "0");
		ed_insert_pow(ed, NULL);
		return KC_NONE;
	case KA_CMD:
		switch (k->arg) {
		case KC_DEL:   ed_backspace(ed); return KC_NONE;
		case KC_LEFT:  ed_move(ed, 0); return KC_NONE;
		case KC_RIGHT: ed_move(ed, 1); return KC_NONE;
		case KC_CLOSE: ed_close_paren(ed); return KC_NONE;
		default:       return k->arg;
		}
	default:
		return KC_NONE;
	}
}
