/* editor.c - structured editor model: sequences, insert, delete, cursor moves.
 * Serialisation and loading live in editor_ser.c, layout in editor_draw.c. */
#include "ui/editor.h"

#include <string.h>

static const uint8_t k_nslots[] = {
	[EK_TEXT] = 0, [EK_FRAC] = 2, [EK_POW] = 1, [EK_SQRT] = 1, [EK_ROOT] = 2,
	[EK_PAREN] = 1, [EK_ABS] = 1, [EK_MIXED] = 3, [EK_FUNC] = 1, [EK_FUNC2] = 2,
};

void ed_touch(Editor *ed) { ed->version++; }

int ed_new_seq(Editor *ed, int parent_seq, int parent_item, int parent_slot)
{
	for (int i = 0; i < ED_POOL; i++) {
		EdSeq *s = &ed->pool[i];
		if (!s->used) {
			s->used = 1;
			s->n = 0;
			s->parent_seq = (int16_t)parent_seq;
			s->parent_item = (int16_t)parent_item;
			s->parent_slot = (int8_t)parent_slot;
			return i;
		}
	}
	return -1;
}

void ed_free_seq(Editor *ed, int seq)
{
	if (seq < 0 || seq >= ED_POOL || !ed->pool[seq].used)
		return;
	EdSeq *s = &ed->pool[seq];
	for (int i = 0; i < s->n; i++)
		for (int k = 0; k < s->it[i].nslots; k++)
			ed_free_seq(ed, s->it[i].slot[k]);
	s->used = 0;
	s->n = 0;
}

/* Children store their item index in the parent: refresh after a shift. */
static void reindex(Editor *ed, int seq)
{
	EdSeq *s = &ed->pool[seq];
	for (int i = 0; i < s->n; i++)
		for (int k = 0; k < s->it[i].nslots; k++)
			ed->pool[s->it[i].slot[k]].parent_item = (int16_t)i;
}

EdItem *ed_put_item(Editor *ed, int seq, int pos, EdKind k, const char *text)
{
	EdSeq *s = &ed->pool[seq];
	if (s->n >= ED_SEQ_MAX || pos < 0 || pos > s->n)
		return NULL;
	EdItem item;
	memset(&item, 0, sizeof item);
	item.kind = (uint8_t)k;
	item.nslots = k_nslots[k];
	if (text) {
		strncpy(item.text, text, ED_TEXT_MAX - 1);
		item.text[ED_TEXT_MAX - 1] = 0;
	}
	for (int i = 0; i < item.nslots; i++) {
		int c = ed_new_seq(ed, seq, pos, i);
		if (c < 0) {
			for (int j = 0; j < i; j++)
				ed_free_seq(ed, item.slot[j]);
			return NULL;
		}
		item.slot[i] = (int16_t)c;
	}
	memmove(&s->it[pos + 1], &s->it[pos], (size_t)(s->n - pos) * sizeof(EdItem));
	s->it[pos] = item;
	s->n++;
	reindex(ed, seq);
	ed_touch(ed);
	return &s->it[pos];
}

static void remove_item(Editor *ed, int seq, int pos, bool free_slots)
{
	EdSeq *s = &ed->pool[seq];
	if (pos < 0 || pos >= s->n)
		return;
	if (free_slots)
		for (int k = 0; k < s->it[pos].nslots; k++)
			ed_free_seq(ed, s->it[pos].slot[k]);
	memmove(&s->it[pos], &s->it[pos + 1], (size_t)(s->n - pos - 1) * sizeof(EdItem));
	s->n--;
	reindex(ed, seq);
	ed_touch(ed);
}

void ed_init(Editor *ed)
{
	memset(ed, 0, sizeof *ed);
	ed->root = ed_new_seq(ed, -1, -1, -1);
	ed->cur_seq = ed->root;
	ed->cur_pos = 0;
}

void ed_clear(Editor *ed)
{
	unsigned v = ed->version;
	ed_init(ed);
	ed->version = v + 1;
}

bool ed_is_empty(const Editor *ed) { return ed->pool[ed->root].n == 0; }

void ed_set_cursor(Editor *ed, int seq, int pos)
{
	if (seq < 0 || seq >= ED_POOL || !ed->pool[seq].used)
		return;
	if (pos < 0)
		pos = 0;
	if (pos > ed->pool[seq].n)
		pos = ed->pool[seq].n;
	ed->cur_seq = seq;
	ed->cur_pos = pos;
	ed_touch(ed);
}

void ed_insert_text(Editor *ed, const char *tok)
{
	if (ed_put_item(ed, ed->cur_seq, ed->cur_pos, EK_TEXT, tok))
		ed->cur_pos++;
}

void ed_insert_template(Editor *ed, EdKind k, const char *name)
{
	EdItem *it = ed_put_item(ed, ed->cur_seq, ed->cur_pos, k, name);
	if (!it)
		return;
	ed_set_cursor(ed, it->slot[0], 0);
}

void ed_insert_pow(Editor *ed, const char *exp_text)
{
	int seq = ed->cur_seq, pos = ed->cur_pos;
	EdItem *it = ed_put_item(ed, seq, pos, EK_POW, NULL);
	if (!it)
		return;
	if (exp_text && *exp_text) {
		int es = it->slot[0];
		ed_put_item(ed, es, 0, EK_TEXT, exp_text);
		ed_set_cursor(ed, seq, pos + 1);
	} else {
		ed_set_cursor(ed, it->slot[0], 0);
	}
}

static EdItem *parent_item(Editor *ed, int seq)
{
	EdSeq *s = &ed->pool[seq];
	if (s->parent_seq < 0)
		return NULL;
	return &ed->pool[s->parent_seq].it[s->parent_item];
}

static void leave_after(Editor *ed, int seq)
{
	EdSeq *s = &ed->pool[seq];
	ed_set_cursor(ed, s->parent_seq, s->parent_item + 1);
}

void ed_close_paren(Editor *ed)
{
	for (int seq = ed->cur_seq; ed->pool[seq].parent_seq >= 0; seq = ed->pool[seq].parent_seq) {
		EdItem *p = parent_item(ed, seq);
		if (p->kind == EK_PAREN || p->kind == EK_FUNC || p->kind == EK_FUNC2 || p->kind == EK_ABS) {
			leave_after(ed, seq);
			return;
		}
	}
	ed_insert_text(ed, ")");
}

static bool slots_empty(const Editor *ed, const EdItem *it)
{
	for (int k = 0; k < it->nslots; k++)
		if (ed->pool[it->slot[k]].n)
			return false;
	return true;
}

/* Slot entered when the cursor moves right into a template / left into it:
 * the first slot from the left, the last (a fraction's denominator) from the right. */
static int entry_slot_right(const EdItem *it) { (void)it; return 0; }
static int entry_slot_left(const EdItem *it) { return it->nslots - 1; }

/* Neighbour slot inside the same template in reading order, -1 if none:
 * numerator -> denominator, whole -> numerator -> denominator, index -> radicand.
 * The keypad has only left/right, so right from a numerator must reach the
 * denominator (it used to leave the fraction, which left the denominator empty). */
static int slot_step(const EdItem *it, int slot, int dir)
{
	switch (it->kind) {
	case EK_FRAC:
	case EK_MIXED:
	case EK_ROOT:
	case EK_FUNC2: {
		int nb = slot + (dir > 0 ? 1 : -1);
		return nb >= 0 && nb < it->nslots ? nb : -1;
	}
	default:
		return -1;
	}
}

void ed_backspace(Editor *ed)
{
	int seq = ed->cur_seq, pos = ed->cur_pos;
	EdSeq *s = &ed->pool[seq];
	if (pos > 0) {
		EdItem *it = &s->it[pos - 1];
		if (it->kind == EK_TEXT || slots_empty(ed, it)) {
			remove_item(ed, seq, pos - 1, true);
			ed_set_cursor(ed, seq, pos - 1);
		} else {
			int sl = it->slot[entry_slot_left(it)];
			ed_set_cursor(ed, sl, ed->pool[sl].n);
		}
		return;
	}
	if (s->parent_seq < 0)
		return;
	int ps = s->parent_seq, pi = s->parent_item;
	EdItem *p = &ed->pool[ps].it[pi];
	int prev = slot_step(p, s->parent_slot, -1);
	if (prev >= 0 && !slots_empty(ed, p)) {
		int sl = p->slot[prev];
		ed_set_cursor(ed, sl, ed->pool[sl].n);
		return;
	}
	if (slots_empty(ed, p)) {
		remove_item(ed, ps, pi, true);
		ed_set_cursor(ed, ps, pi);
		return;
	}
	if (p->nslots == 1) {
		/* unwrap: splice the contents into the parent */
		EdSeq tmp = ed->pool[p->slot[0]];
		int inner = p->slot[0];
		remove_item(ed, ps, pi, false);
		EdSeq *dst = &ed->pool[ps];
		int room = ED_SEQ_MAX - dst->n;
		int cnt = tmp.n < room ? tmp.n : room;
		memmove(&dst->it[pi + cnt], &dst->it[pi], (size_t)(dst->n - pi) * sizeof(EdItem));
		memcpy(&dst->it[pi], tmp.it, (size_t)cnt * sizeof(EdItem));
		dst->n += cnt;
		for (int i = pi; i < pi + cnt; i++)
			for (int k = 0; k < dst->it[i].nslots; k++)
				ed->pool[dst->it[i].slot[k]].parent_seq = (int16_t)ps;
		for (int i = cnt; i < tmp.n; i++)   /* overflow (never in practice) */
			for (int k = 0; k < tmp.it[i].nslots; k++)
				ed_free_seq(ed, tmp.it[i].slot[k]);
		ed->pool[inner].used = 0;
		ed->pool[inner].n = 0;
		reindex(ed, ps);
		ed_set_cursor(ed, ps, pi);
		return;
	}
	ed_set_cursor(ed, ps, pi);
}

static bool move_lr(Editor *ed, int dir)
{
	int seq = ed->cur_seq, pos = ed->cur_pos;
	EdSeq *s = &ed->pool[seq];
	if (dir > 0 && pos < s->n) {
		EdItem *it = &s->it[pos];
		if (it->nslots)
			ed_set_cursor(ed, it->slot[entry_slot_right(it)], 0);
		else
			ed_set_cursor(ed, seq, pos + 1);
		return true;
	}
	if (dir < 0 && pos > 0) {
		EdItem *it = &s->it[pos - 1];
		if (it->nslots) {
			int sl = it->slot[entry_slot_left(it)];
			ed_set_cursor(ed, sl, ed->pool[sl].n);
		} else {
			ed_set_cursor(ed, seq, pos - 1);
		}
		return true;
	}
	if (s->parent_seq < 0)
		return false;
	EdItem *p = parent_item(ed, seq);
	int nb = slot_step(p, s->parent_slot, dir);
	if (nb >= 0) {
		int sl = p->slot[nb];
		ed_set_cursor(ed, sl, dir > 0 ? 0 : ed->pool[sl].n);
		return true;
	}
	if (dir > 0)
		ed_set_cursor(ed, s->parent_seq, s->parent_item + 1);
	else
		ed_set_cursor(ed, s->parent_seq, s->parent_item);
	return true;
}

/* Up/down: numerator <-> denominator of the innermost fraction / mixed number
 * around the cursor; down out of an exponent; else into an adjacent fraction. */
static bool move_ud(Editor *ed, int dir)
{
	for (int seq = ed->cur_seq; ed->pool[seq].parent_seq >= 0; seq = ed->pool[seq].parent_seq) {
		EdSeq *s = &ed->pool[seq];
		EdItem *p = parent_item(ed, seq);
		int num = p->kind == EK_FRAC ? 0 : (p->kind == EK_MIXED ? 1 : -1);
		if (num >= 0) {
			int want = dir < 0 ? num : num + 1;
			if (s->parent_slot == (dir < 0 ? num + 1 : num)) {
				int sl = p->slot[want];
				int pos = ed->cur_seq == seq ? ed->cur_pos : ed->pool[sl].n;
				ed_set_cursor(ed, sl, pos);
				return true;
			}
		}
		if (p->kind == EK_POW && dir > 0) {
			ed_set_cursor(ed, s->parent_seq, s->parent_item + 1);
			return true;
		}
	}
	/* a fraction right next to the cursor */
	EdSeq *s = &ed->pool[ed->cur_seq];
	for (int d = 0; d < 2; d++) {
		int idx = d == 0 ? ed->cur_pos - 1 : ed->cur_pos;
		if (idx < 0 || idx >= s->n)
			continue;
		EdItem *it = &s->it[idx];
		if (it->kind == EK_FRAC || it->kind == EK_MIXED) {
			int base = it->kind == EK_FRAC ? 0 : 1;
			int sl = it->slot[dir < 0 ? base : base + 1];
			ed_set_cursor(ed, sl, d == 0 ? ed->pool[sl].n : 0);
			return true;
		}
	}
	return false;
}

bool ed_move(Editor *ed, int dir)
{
	switch (dir) {
	case 0: return move_lr(ed, -1);
	case 1: return move_lr(ed, 1);
	case 2: return move_ud(ed, -1);
	default: return move_ud(ed, 1);
	}
}

void ed_home(Editor *ed) { ed_set_cursor(ed, ed->root, 0); }
void ed_end(Editor *ed) { ed_set_cursor(ed, ed->root, ed->pool[ed->root].n); }
