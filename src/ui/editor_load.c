/* editor_load.c - display syntax -> editor templates (ed_load,
 * ed_insert_display): the text is parsed in a private arena and the Expr is
 * turned back into fraction / power / root / bracket templates so history
 * entries and answers can be edited structurally. Split from editor_ser.c. */
#include "ui/editor.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/num.h"

#include <stdio.h>
#include <string.h>

static void put_chars(Editor *ed, int seq, int *pos, const char *s)
{
	while (*s) {
		unsigned char c = (unsigned char)*s;
		int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
		char tok[ED_TEXT_MAX];
		int k = 0;
		for (; k < len && s[k] && k < ED_TEXT_MAX - 1; k++)
			tok[k] = s[k];
		tok[k] = 0;
		const char *t = !strcmp(tok, "-") ? "\xE2\x88\x92" : !strcmp(tok, "*") ? "\xC3\x97" : tok;
		if (ed_put_item(ed, seq, *pos, EK_TEXT, t))
			(*pos)++;
		s += k ? k : 1;
	}
}

static int put_tmpl(Editor *ed, int seq, int *pos, EdKind k, const char *name, int out_slots[3])
{
	EdItem *it = ed_put_item(ed, seq, *pos, k, name);
	if (!it)
		return 0;
	for (int i = 0; i < it->nslots; i++)
		out_slots[i] = it->slot[i];
	(*pos)++;
	return 1;
}

static void conv(Editor *ed, const Expr *e, int seq, int *pos, bool keep_paren);

static void conv_in(Editor *ed, const Expr *e, int seq)
{
	int p = ed->pool[seq].n;
	conv(ed, e, seq, &p, false);
}

static bool needs_br(const Expr *e, int min)
{
	if (e->flags & EF_PAREN)
		return false;
	int pr;
	switch (e->kind) {
	case E_LIST: pr = 0; break;
	case E_REL: pr = 1; break;
	case E_ADD: pr = 2; break;
	case E_PM: pr = e->nkids >= 2 ? 2 : 4; break;
	case E_MUL: pr = 3; break;
	case E_NEG: pr = 4; break;
	case E_POW: pr = 5; break;
	case E_NUM: pr = e->r.n < 0 ? 4 : 9; break;
	case E_REAL: pr = e->v < 0 ? 4 : 9; break;
	default: pr = 9; break;
	}
	return pr < min;
}

static void conv_child(Editor *ed, const Expr *e, int seq, int *pos, int min)
{
	if (needs_br(e, min)) {
		int sl[3];
		if (put_tmpl(ed, seq, pos, EK_PAREN, NULL, sl))
			conv_in(ed, e, sl[0]);
	} else {
		conv(ed, e, seq, pos, true);
	}
}

static const char *fname(int op)
{
	static const char *n[] = { [F_SIN] = "sin", [F_COS] = "cos", [F_TAN] = "tan", [F_ASIN] = "asin",
		[F_ACOS] = "acos", [F_ATAN] = "atan", [F_SINH] = "sinh", [F_COSH] = "cosh", [F_TANH] = "tanh",
		[F_LN] = "ln", [F_LOG] = "log", [F_FLOOR] = "floor", [F_CEIL] = "ceil", [F_ROUND] = "round",
		[F_NCR] = "nCr", [F_NPR] = "nPr", [F_EXP] = "exp" };
	return op > 0 && op < F_FUNC_COUNT && n[op] ? n[op] : "f";
}

static const char *rel_s(int op)
{
	switch (op) {
	case R_LT: return "<";
	case R_LE: return "\xE2\x89\xA4";
	case R_GT: return ">";
	case R_GE: return "\xE2\x89\xA5";
	case R_NE: return "\xE2\x89\xA0";
	default: return "=";
	}
}

static void conv(Editor *ed, const Expr *e, int seq, int *pos, bool keep_paren)
{
	char buf[64];
	int sl[3];
	if (!e)
		return;
	if (keep_paren && (e->flags & EF_PAREN)) {
		if (put_tmpl(ed, seq, pos, EK_PAREN, NULL, sl))
			conv_in(ed, e, sl[0]);
		return;
	}
	switch (e->kind) {
	case E_NUM:
		if ((e->flags & NF_DECIMAL) || e->r.d == 1) {
			rat_to_decimal_str(e->r, 12, buf, sizeof buf);
			put_chars(ed, seq, pos, buf);
		} else {
			if (e->r.n < 0)
				put_chars(ed, seq, pos, "-");
			if (put_tmpl(ed, seq, pos, EK_FRAC, NULL, sl)) {
				snprintf(buf, sizeof buf, "%lld", (long long)(e->r.n < 0 ? -e->r.n : e->r.n));
				int p = 0;
				put_chars(ed, sl[0], &p, buf);
				snprintf(buf, sizeof buf, "%lld", (long long)e->r.d);
				p = 0;
				put_chars(ed, sl[1], &p, buf);
			}
		}
		break;
	case E_REAL: {
		dbl_to_str(e->v, 10, buf, sizeof buf);
		char *E = strchr(buf, 'E');
		if (E) {
			*E = 0;
			put_chars(ed, seq, pos, buf);
			put_chars(ed, seq, pos, "\xC3\x97" "10");
			if (put_tmpl(ed, seq, pos, EK_POW, NULL, sl)) {
				int p = 0;
				put_chars(ed, sl[0], &p, E[1] == '+' ? E + 2 : E + 1);
			}
		} else {
			put_chars(ed, seq, pos, buf);
		}
		break;
	}
	case E_VAR: put_chars(ed, seq, pos, e->name); break;
	case E_CONST: put_chars(ed, seq, pos, e->op == C_PI ? "\xCF\x80" : "e"); break;
	case E_TEXT:
		put_chars(ed, seq, pos, "\"");
		put_chars(ed, seq, pos, e->text ? e->text : "");
		put_chars(ed, seq, pos, "\"");
		break;
	case E_RECUR:
		if (put_tmpl(ed, seq, pos, EK_FUNC, "recur", sl)) {
			int p = 0;
			put_chars(ed, sl[0], &p, e->text ? e->text : "");
		}
		break;
	case E_ADD:
		for (int i = 0; i < e->nkids; i++) {
			const Expr *k = e->kids[i];
			bool neg = (k->kind == E_NEG || (k->kind == E_NUM && k->r.n < 0) ||
				    (k->kind == E_REAL && k->v < 0)) && !(k->flags & EF_PAREN);
			if (i && !neg)
				put_chars(ed, seq, pos, "+");
			conv_child(ed, k, seq, pos, 2);
		}
		break;
	case E_MUL:
		for (int i = 0; i < e->nkids; i++) {
			const Expr *k = e->kids[i];
			if (i && (k->kind == E_NUM || k->kind == E_REAL || k->kind == E_MIXED ||
				  (k->kind == E_POW && k->kids[0]->kind == E_NUM)) && !(k->flags & EF_PAREN))
				put_chars(ed, seq, pos, "\xC3\x97");
			conv_child(ed, k, seq, pos, i ? 5 : 3);
		}
		break;
	case E_DIV:
		if (put_tmpl(ed, seq, pos, EK_FRAC, NULL, sl)) {
			conv_in(ed, e->kids[0], sl[0]);
			conv_in(ed, e->kids[1], sl[1]);
		}
		break;
	case E_NEG:
		put_chars(ed, seq, pos, "-");
		conv_child(ed, e->kids[0], seq, pos, 3);   /* -2x, not -(2x) */
		break;
	case E_POW:
		conv_child(ed, e->kids[0], seq, pos, 6);
		if (put_tmpl(ed, seq, pos, EK_POW, NULL, sl))
			conv_in(ed, e->kids[1], sl[0]);
		break;
	case E_FUNC: {
		EdKind k = EK_FUNC;
		const char *nm = fname(e->op);
		if (e->op == F_SQRT) k = EK_SQRT;
		else if (e->op == F_ROOT) k = EK_ROOT;
		else if (e->op == F_ABS) k = EK_ABS;
		else if (e->nkids >= 2) k = EK_FUNC2;
		if (put_tmpl(ed, seq, pos, k, nm, sl)) {
			int nk = k == EK_ROOT || k == EK_FUNC2 ? 2 : 1;
			for (int i = 0; i < nk && i < e->nkids; i++)
				conv_in(ed, e->kids[i], sl[i]);
		}
		break;
	}
	case E_REL:
		for (int i = 0; i < e->nkids; i++) {
			if (i)
				put_chars(ed, seq, pos, rel_s(i - 1 < EXPR_MAX_REL && e->ops[i - 1] ? e->ops[i - 1] : e->op));
			conv_child(ed, e->kids[i], seq, pos, 2);
		}
		break;
	case E_LIST:
		for (int i = 0; i < e->nkids; i++) {
			if (i)
				put_chars(ed, seq, pos, ",");
			conv(ed, e->kids[i], seq, pos, true);
		}
		break;
	case E_PM:
		if (e->nkids >= 2)
			conv_child(ed, e->kids[0], seq, pos, 2);
		put_chars(ed, seq, pos, "\xC2\xB1");
		conv_child(ed, e->kids[e->nkids - 1], seq, pos, 3);
		break;
	case E_PCT: case E_FACT: case E_DEG:
		conv_child(ed, e->kids[0], seq, pos, 6);
		put_chars(ed, seq, pos, e->kind == E_PCT ? "%" : e->kind == E_FACT ? "!" : "\xC2\xB0");
		break;
	case E_MIXED:
		if (put_tmpl(ed, seq, pos, EK_MIXED, NULL, sl))
			for (int i = 0; i < 3; i++)
				conv_in(ed, e->kids[i], sl[i]);
		break;
	default:
		break;
	}
}

static Arena *g_load_arena;

void ed_insert_display(Editor *ed, const char *display)
{
	if (!display || !*display)
		return;
	if (!g_load_arena)
		g_load_arena = arena_new(64 * 1024);
	Expr *e = NULL;
	Arena *prev = NULL;
	if (g_load_arena) {
		prev = arena_use(g_load_arena);
		arena_reset(g_load_arena);
		char err[64];
		int ep = -1;
		e = ex_parse(display, err, (int)sizeof err, &ep);
	}
	int seq = ed->cur_seq, pos = ed->cur_pos;
	if (e)
		conv(ed, e, seq, &pos, true);
	else
		put_chars(ed, seq, &pos, display);
	if (g_load_arena) {
		arena_reset(g_load_arena);
		arena_use(prev);
	}
	ed_set_cursor(ed, seq, pos);
}

void ed_load(Editor *ed, const char *display)
{
	ed_clear(ed);
	ed_insert_display(ed, display);
	ed_end(ed);
}
