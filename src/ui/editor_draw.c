/* editor_draw.c - lays the editor's item tree out with the mlayout builders,
 * with a caret mark at every cursor position (so the caret and touch hits
 * follow the natural-display layout exactly). Cached per editor version. */
#include "ui/editor_draw.h"

#include "ui/font.h"

#include <string.h>

#define MINUS "\xE2\x88\x92"

static Frag lay_seq(const Editor *ed, int seq, int px, int depth);

static bool is_binop(const char *t)
{
	static const char *ops[] = { "+", "-", MINUS, "\xC3\x97", "\xC3\xB7", "*", "/", "=", "<", ">",
		"\xE2\x89\xA4", "\xE2\x89\xA5", "\xE2\x89\xA0", "\xC2\xB1", NULL };
	for (int i = 0; ops[i]; i++)
		if (!strcmp(t, ops[i]))
			return true;
	return false;
}

static Frag lay_text(const char *t, int px)
{
	const char *s = !strcmp(t, "-") ? MINUS : !strcmp(t, "*") ? "\xC3\x97" : t;
	if (is_binop(t)) {
		int sp = px / 5;
		Frag f = ml_space(sp, px);
		f = ml_cat(f, ml_text(s, px, FW_REG, MR_NORMAL));
		return ml_cat(f, ml_space(sp, px));
	}
	if (!strcmp(t, ","))
		return ml_cat(ml_text(",", px, FW_REG, MR_NORMAL), ml_space(px / 4, px));
	return ml_text(s, px, FW_REG, MR_NORMAL);
}

static const char *disp_name(const char *n, bool *inverse)
{
	*inverse = false;
	if (n[0] == 'a' && (!strcmp(n, "asin") || !strcmp(n, "acos") || !strcmp(n, "atan"))) {
		*inverse = true;
		return n + 1;
	}
	return n;
}

static Frag lay_item(const Editor *ed, const EdItem *it, int px, int depth)
{
	int sp = ml_script_px(px);
	switch (it->kind) {
	case EK_TEXT:
		return lay_text(it->text, px);
	case EK_FRAC: {
		int fp = depth > 0 ? ml_quant(px * 85 / 100) : px;
		Frag n = lay_seq(ed, it->slot[0], fp, depth + 1);
		Frag d = lay_seq(ed, it->slot[1], fp, depth + 1);
		return ml_frac(n, d, px);
	}
	case EK_SQRT:
		return ml_root(ml_empty(), lay_seq(ed, it->slot[0], px, depth), px);
	case EK_ROOT: {
		Frag idx = lay_seq(ed, it->slot[0], sp, depth);
		return ml_root(idx, lay_seq(ed, it->slot[1], px, depth), px);
	}
	case EK_PAREN:
		return ml_wrap(lay_seq(ed, it->slot[0], px, depth), "(", ")", px, MR_NORMAL);
	case EK_ABS:
		return ml_wrap(lay_seq(ed, it->slot[0], px, depth), "|", "|", px, MR_NORMAL);
	case EK_MIXED: {
		Frag w = lay_seq(ed, it->slot[0], px, depth);
		int fp = ml_quant(px * 80 / 100);
		Frag n = lay_seq(ed, it->slot[1], fp, depth + 1);
		Frag d = lay_seq(ed, it->slot[2], fp, depth + 1);
		w = ml_cat(w, ml_space(2, px));
		return ml_cat(w, ml_frac(n, d, px));
	}
	case EK_FUNC: {
		bool inv;
		const char *nm = disp_name(it->text, &inv);
		Frag f = ml_text(nm, px, FW_REG, MR_NORMAL);
		if (inv)
			f = ml_sup(f, ml_text(MINUS "1", sp, FW_REG, MR_NORMAL), px);
		return ml_cat(f, ml_wrap(lay_seq(ed, it->slot[0], px, depth), "(", ")", px, MR_NORMAL));
	}
	case EK_FUNC2: {
		if (!strcmp(it->text, "log")) {
			Frag f = ml_text("log", px, FW_REG, MR_NORMAL);
			f = ml_sub(f, lay_seq(ed, it->slot[0], sp, depth), px);
			return ml_cat(f, ml_wrap(lay_seq(ed, it->slot[1], px, depth), "(", ")", px, MR_NORMAL));
		}
		Frag f = ml_text(it->text, px, FW_REG, MR_NORMAL);
		Frag in = lay_seq(ed, it->slot[0], px, depth);
		in = ml_cat(in, ml_text(",", px, FW_REG, MR_NORMAL));
		in = ml_cat(in, ml_space(px / 4, px));
		in = ml_cat(in, lay_seq(ed, it->slot[1], px, depth));
		return ml_cat(f, ml_wrap(in, "(", ")", px, MR_NORMAL));
	}
	default:
		return ml_empty();
	}
}

static Frag lay_seq(const Editor *ed, int seq, int px, int depth)
{
	const EdSeq *s = &ed->pool[seq];
	if (s->n == 0) {
		Frag c = ml_caret(seq, 0, px);
		if (seq == ed->root) {
			/* keep the line height of an empty input */
			Frag sp = ml_space(1, px);
			sp.asc = ml_text_asc(px);
			sp.desc = ml_text_desc(px);
			return ml_cat(c, sp);
		}
		return ml_cat(c, ml_slot(px));
	}
	Frag acc = ml_empty(), last = ml_empty();
	bool have_last = false;
	for (int i = 0; i < s->n; i++) {
		const EdItem *it = &s->it[i];
		if (it->kind == EK_POW) {
			Frag base = have_last ? last : ml_slot(px);
			base = ml_cat(base, ml_caret(seq, i, px));
			last = ml_sup(base, lay_seq(ed, it->slot[0], ml_script_px(px), depth), px);
			have_last = true;
			continue;
		}
		if (have_last)
			acc = ml_cat(acc, last);
		Frag c = ml_caret(seq, i, px);
		/* a sign at the start or after an operator is unary: no spaces round it */
		const EdItem *pv = i ? &s->it[i - 1] : NULL;
		bool sign = it->kind == EK_TEXT && (!strcmp(it->text, "-") || !strcmp(it->text, MINUS));
		if (sign && (!pv || (pv->kind == EK_TEXT && (is_binop(pv->text) || !strcmp(pv->text, ",")))))
			last = ml_cat(c, ml_text(MINUS, px, FW_REG, MR_NORMAL));
		else
			last = ml_cat(c, lay_item(ed, it, px, depth));
		have_last = true;
	}
	if (have_last)
		acc = ml_cat(acc, last);
	return ml_cat(acc, ml_caret(seq, s->n, px));
}

/* ---- cache: a few editors (main input, sheet input) ---- */
typedef struct {
	const Editor *ed;
	unsigned version;
	int px;
	MLayout *l;
} slot_t;

static slot_t g_c[4];
static int g_next;

const MLayout *ed_layout(const Editor *ed, int px)
{
	for (int i = 0; i < 4; i++)
		if (g_c[i].l && g_c[i].ed == ed && g_c[i].version == ed->version && g_c[i].px == px)
			return g_c[i].l;
	int v = -1;
	for (int i = 0; i < 4; i++)
		if (g_c[i].ed == ed && g_c[i].px == px)
			v = i;
	if (v < 0) {
		v = g_next;
		g_next = (g_next + 1) % 4;
	}
	if (g_c[v].l)
		ml_free(g_c[v].l);
	ml_reset();
	g_c[v].l = ml_finish(lay_seq(ed, ed->root, px, 0));
	g_c[v].ed = ed;
	g_c[v].version = ed->version;
	g_c[v].px = px;
	return g_c[v].l;
}

void ed_draw(const Editor *ed, const MLayout *l, int x, int baseline, unsigned int color, bool caret)
{
	mdraw_caret(l, x, baseline, color, ed->cur_seq, ed->cur_pos, caret ? 1 : 0);
}

int ed_caret_x(const Editor *ed, const MLayout *l)
{
	for (int i = 0; i < l->nops; i++) {
		const MOp *o = &l->ops[i];
		if (o->kind == MO_CARET && o->x2 == ed->cur_seq && o->y2 == ed->cur_pos)
			return o->x;
	}
	return l->w;
}

void ed_hit(Editor *ed, const MLayout *l, int rx, int ry)
{
	int best = -1, bestd = 1 << 30;
	for (int i = 0; i < l->nops; i++) {
		const MOp *o = &l->ops[i];
		if (o->kind != MO_CARET)
			continue;
		int top = o->y - o->px, bot = o->y + o->weight;
		int dy = ry < top ? top - ry : ry > bot ? ry - bot : 0;
		int dx = rx - o->x;
		if (dx < 0) dx = -dx;
		int d = dx + dy * 3;
		if (d < bestd) {
			bestd = d;
			best = i;
		}
	}
	if (best >= 0)
		ed_set_cursor(ed, l->ops[best].x2, l->ops[best].y2);
}
