/* editor_ser.c - editor <-> display syntax.
 * ed_serialize writes what the engine parses (docs/SYNTAX.md); ed_load turns a
 * parsed Expr back into editor templates so history entries and answers can be
 * edited structurally. */
#include "ui/editor.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/num.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------ serialise */

typedef struct {
	char *p;
	int n, cap;
} sb_t;

static void sb_put(sb_t *b, const char *s)
{
	while (*s) {
		if (b->n + 1 < b->cap)
			b->p[b->n] = *s;
		b->n++;
		s++;
	}
	if (b->cap > 0)
		b->p[b->n < b->cap ? b->n : b->cap - 1] = 0;
}

static bool is_digitish(const EdItem *it)
{
	return it->kind == EK_TEXT && ((it->text[0] >= '0' && it->text[0] <= '9') || it->text[0] == '.') &&
	       !it->text[1];
}

static bool is_letterish(const EdItem *it)
{
	if (it->kind != EK_TEXT)
		return false;
	unsigned char c = (unsigned char)it->text[0];
	if (((c | 32) >= 'a' && (c | 32) <= 'z') && !it->text[1])
		return true;
	return !strcmp(it->text, "\xCF\x80") || !strcmp(it->text, "\xCE\xB8"); /* π θ */
}

/* operator-like text: a fraction next to it needs no protective brackets */
static bool is_operator(const EdItem *it)
{
	static const char *ops[] = { "+", "-", "\xE2\x88\x92", "\xC3\x97", "\xC3\xB7", "*", "/", "=", "<", ">",
		"\xE2\x89\xA4", "\xE2\x89\xA5", "\xE2\x89\xA0", ",", "\xC2\xB1", "(", NULL };
	if (it->kind != EK_TEXT)
		return false;
	for (int i = 0; ops[i]; i++)
		if (!strcmp(it->text, ops[i]))
			return true;
	return false;
}

/* A slot that is a plain number or one letter needs no brackets as a part. */
static bool seq_atomic(const Editor *ed, int seq)
{
	const EdSeq *s = &ed->pool[seq];
	if (s->n == 1 && (is_letterish(&s->it[0]) || s->it[0].kind == EK_PAREN || s->it[0].kind == EK_FUNC ||
			  s->it[0].kind == EK_SQRT || s->it[0].kind == EK_ABS))
		return true;
	if (!s->n)
		return false;
	for (int i = 0; i < s->n; i++)
		if (!is_digitish(&s->it[i]))
			return false;
	return true;
}

static bool seq_integer(const Editor *ed, int seq)
{
	const EdSeq *s = &ed->pool[seq];
	if (!s->n)
		return false;
	for (int i = 0; i < s->n; i++)
		if (!is_digitish(&s->it[i]) || s->it[i].text[0] == '.')
			return false;
	return true;
}

static void ser_seq(const Editor *ed, int seq, sb_t *b);

static void ser_part(const Editor *ed, int seq, sb_t *b)
{
	bool br = !seq_atomic(ed, seq);
	if (br) sb_put(b, "(");
	ser_seq(ed, seq, b);
	if (br) sb_put(b, ")");
}

static void ser_inner(const Editor *ed, int seq, sb_t *b)
{
	sb_put(b, "(");
	ser_seq(ed, seq, b);
	sb_put(b, ")");
}

static void ser_item(const Editor *ed, const EdSeq *s, int i, sb_t *b)
{
	const EdItem *it = &s->it[i];
	const EdItem *prev = i > 0 ? &s->it[i - 1] : NULL;
	const EdItem *next = i + 1 < s->n ? &s->it[i + 1] : NULL;
	switch (it->kind) {
	case EK_TEXT:
		sb_put(b, !strcmp(it->text, "\xE2\x88\x92") ? "-" : it->text);
		break;
	case EK_FRAC: {
		bool wrap = (prev && !is_operator(prev)) ||
			    (next && !is_operator(next) && !is_letterish(next) && strcmp(next->text, ")"));
		if (wrap) sb_put(b, "(");
		ser_part(ed, it->slot[0], b);
		sb_put(b, "/");
		ser_part(ed, it->slot[1], b);
		if (wrap) sb_put(b, ")");
		break;
	}
	case EK_POW: {
		const EdSeq *e = &ed->pool[it->slot[0]];
		bool simple = seq_integer(ed, it->slot[0]) || (e->n == 1 && is_letterish(&e->it[0]));
		sb_put(b, "^");
		if (simple) ser_seq(ed, it->slot[0], b);
		else ser_inner(ed, it->slot[0], b);
		break;
	}
	case EK_SQRT:
		sb_put(b, "\xE2\x88\x9A");
		ser_inner(ed, it->slot[0], b);
		break;
	case EK_ROOT:
		sb_put(b, "root(");
		ser_seq(ed, it->slot[0], b);
		sb_put(b, ",");
		ser_seq(ed, it->slot[1], b);
		sb_put(b, ")");
		break;
	case EK_PAREN:
		ser_inner(ed, it->slot[0], b);
		break;
	case EK_ABS:
		sb_put(b, "abs");
		ser_inner(ed, it->slot[0], b);
		break;
	case EK_MIXED:
		if (seq_integer(ed, it->slot[0]) && seq_integer(ed, it->slot[1]) && seq_integer(ed, it->slot[2])) {
			bool wrap = prev && !is_operator(prev);
			if (wrap) sb_put(b, "(");
			ser_seq(ed, it->slot[0], b);
			sb_put(b, "_");
			ser_seq(ed, it->slot[1], b);
			sb_put(b, "/");
			ser_seq(ed, it->slot[2], b);
			if (wrap) sb_put(b, ")");
		} else {
			sb_put(b, "(");
			ser_part(ed, it->slot[0], b);
			sb_put(b, "+");
			ser_part(ed, it->slot[1], b);
			sb_put(b, "/");
			ser_part(ed, it->slot[2], b);
			sb_put(b, ")");
		}
		break;
	case EK_FUNC:
		sb_put(b, it->text);
		ser_inner(ed, it->slot[0], b);
		break;
	case EK_FUNC2:
		sb_put(b, it->text);
		sb_put(b, "(");
		ser_seq(ed, it->slot[0], b);
		sb_put(b, ",");
		ser_seq(ed, it->slot[1], b);
		sb_put(b, ")");
		break;
	default:
		break;
	}
}

static void ser_seq(const Editor *ed, int seq, sb_t *b)
{
	const EdSeq *s = &ed->pool[seq];
	for (int i = 0; i < s->n; i++) {
		/* "2" then a pre-filled text like "Ans" or a letter is implicit
		 * multiplication; the parser handles that. Two numbers in a row
		 * only happen via templates, which bracket themselves. */
		ser_item(ed, s, i, b);
	}
}

int ed_serialize(const Editor *ed, char *out, int cap)
{
	sb_t b = { out, 0, cap };
	if (cap > 0)
		out[0] = 0;
	ser_seq(ed, ed->root, &b);
	return b.n < cap ? b.n : cap - 1;
}
