/* mlayout_expr.c - Expr tree -> natural display fragments.
 * Runs inside mlayout_cache.c's parse arena: temporary nodes made here
 * (absolute values of negative coefficients) are freed with it. */
#include "ui/mlayout.h"

#include "engine/arena.h"
#include "engine/expr.h"
#include "engine/num.h"
#include "ui/font.h"
#include "ui/mlayout_num.h"

#include <stdio.h>
#include <string.h>

#define MINUS "\xE2\x88\x92"   /* U+2212 */
#define TIMES "\xC3\x97"       /* U+00D7 */
#define PLUSMINUS "\xC2\xB1"

static int g_frac_depth;

static Frag lay(const Expr *e, int px);
static Frag lay_core(const Expr *e, int px);
/* Brackets the user typed are redundant where the structure already groups
 * (fraction parts, exponents, function arguments): drop them there. */
static Frag bare(const Expr *e, int px) { return e ? lay_core(e, px) : ml_slot(px); }

static int is_stacked_num(const Expr *e)
{
	return e->kind == E_NUM && !(e->flags & NF_DECIMAL) && e->r.d != 1;
}

static int is_negative(const Expr *e)
{
	if (e->flags & EF_PAREN)
		return 0;
	if (e->kind == E_NUM) return e->r.n < 0;
	if (e->kind == E_REAL) return e->v < 0;
	return 0;
}

static int prec(const Expr *e)
{
	if (e->flags & EF_PAREN)
		return 9;
	switch (e->kind) {
	case E_LIST: return 0;
	case E_REL:  return 1;
	case E_ADD:  return 2;
	case E_PM:   return e->nkids >= 2 ? 2 : 4;
	case E_MUL:  return 3;
	case E_NEG:  return 4;
	case E_POW:  return 5;
	case E_PCT: case E_FACT: case E_DEG: return 6;
	case E_DIV: case E_MIXED: return 8;
	case E_NUM:  return is_negative(e) ? 4 : is_stacked_num(e) ? 8 : 9;
	case E_REAL: return is_negative(e) ? 4 : 9;
	default:     return 9;
	}
}

static Frag paren(Frag f, int px) { return ml_wrap(f, "(", ")", px, MR_NORMAL); }

static Frag child(const Expr *e, int px, int min_prec)
{
	Frag f = lay(e, px);
	if (prec(e) < min_prec)
		f = paren(f, px);
	return f;
}

static Frag op_text(const char *op, int px)
{
	int sp = px / 5;
	Frag a = ml_space(sp, px);
	Frag t = ml_text(op, px, FW_REG, MR_NORMAL);
	Frag f = ml_cat(a, t);
	return ml_cat(f, ml_space(sp, px));
}

static Frag lay_num(const Expr *e, int px)
{
	char buf[64], fixed[96];
	Rat r = e->r;
	if (e->flags & NF_DECIMAL) {
		rat_to_decimal_str(r, 15, buf, sizeof buf);
		mlx_fix_minus(buf, fixed, sizeof fixed);
		return ml_text(fixed, px, FW_REG, MR_NORMAL);
	}
	if (r.d == 1) {
		rat_to_str(r, buf, sizeof buf);
		mlx_fix_minus(buf, fixed, sizeof fixed);
		return ml_text(fixed, px, FW_REG, MR_NORMAL);
	}
	Frag sign = ml_empty();
	if (r.n < 0)
		sign = ml_text(MINUS, px, FW_REG, MR_NORMAL);
	g_frac_depth++;
	int sp = g_frac_depth > 1 ? ml_quant(px * 85 / 100) : px;
	snprintf(buf, sizeof buf, "%lld", (long long)(r.n < 0 ? -r.n : r.n));
	Frag n = ml_text(buf, sp, FW_REG, MR_NORMAL);
	snprintf(buf, sizeof buf, "%lld", (long long)r.d);
	Frag d = ml_text(buf, sp, FW_REG, MR_NORMAL);
	g_frac_depth--;
	return ml_cat(sign, ml_frac(n, d, px));
}

static int starts_with_digit(const Expr *e)
{
	if (e->flags & EF_PAREN)
		return 0;
	switch (e->kind) {
	case E_NUM: return !is_stacked_num(e);
	case E_REAL: case E_MIXED: case E_RECUR: return 1;
	case E_POW: case E_MUL: case E_PCT: case E_FACT: case E_DEG:
		return e->nkids > 0 && starts_with_digit(e->kids[0]);
	default: return 0;
	}
}

static Frag lay_args(const Expr *e, int px)
{
	Frag f = ml_empty();
	for (int i = 0; i < e->nkids; i++) {
		if (i)
			f = ml_cat(f, ml_text(", ", px, FW_REG, MR_NORMAL));
		f = ml_cat(f, bare(e->kids[i], px));
	}
	return f;
}

static const char *func_name(int op)
{
	switch (op) {
	case F_SIN: return "sin"; case F_COS: return "cos"; case F_TAN: return "tan";
	case F_ASIN: return "sin"; case F_ACOS: return "cos"; case F_ATAN: return "tan";
	case F_SINH: return "sinh"; case F_COSH: return "cosh"; case F_TANH: return "tanh";
	case F_LN: return "ln"; case F_LOG: return "log"; case F_ROUND: return "round";
	default: return "f";
	}
}

static Frag lay_func(const Expr *e, int px)
{
	int sp = ml_script_px(px);
	switch (e->op) {
	case F_SQRT:
		if (e->nkids >= 1)
			return ml_root(ml_empty(), bare(e->kids[0], px), px);
		break;
	case F_ROOT:
		if (e->nkids >= 2) {
			Frag idx = bare(e->kids[0], sp);
			return ml_root(idx, bare(e->kids[1], px), px);
		}
		break;
	case F_ABS:
		if (e->nkids >= 1) return ml_wrap(bare(e->kids[0], px), "|", "|", px, MR_NORMAL);
		break;
	case F_FLOOR:
		if (e->nkids >= 1) return ml_wrap(lay(e->kids[0], px), "\xE2\x8C\x8A", "\xE2\x8C\x8B", px, MR_NORMAL);
		break;
	case F_CEIL:
		if (e->nkids >= 1) return ml_wrap(lay(e->kids[0], px), "\xE2\x8C\x88", "\xE2\x8C\x89", px, MR_NORMAL);
		break;
	case F_NCR:
	case F_NPR:
		if (e->nkids >= 2) {
			Frag n = lay(e->kids[0], sp);
			int rise = px * 40 / 100;
			ml_shift(n, 0, -rise);
			Frag c = ml_text(e->op == F_NCR ? "C" : "P", px, FW_REG, MR_NORMAL);
			ml_shift(c, n.w + 1, 0);
			Frag r = lay(e->kids[1], sp);
			int drop = px * 22 / 100;
			ml_shift(r, n.w + 1 + c.w + 1, drop);
			Frag f;
			f.start = n.n ? n.start : c.start;
			f.n = r.start + r.n - f.start;
			f.w = n.w + 1 + c.w + 1 + r.w;
			f.asc = c.asc > rise + n.asc ? c.asc : rise + n.asc;
			f.desc = c.desc > drop + r.desc ? c.desc : drop + r.desc;
			return f;
		}
		break;
	case F_EXP:
		if (e->nkids >= 1) {
			Frag b = ml_text("e", px, FW_REG, MR_NORMAL);
			return ml_sup(b, lay(e->kids[0], sp), px);
		}
		break;
	case F_LOG:
		if (e->nkids >= 2) {
			Frag b = ml_text("log", px, FW_REG, MR_NORMAL);
			b = ml_sub(b, lay(e->kids[0], sp), px);
			return ml_cat(b, paren(bare(e->kids[1], px), px));
		}
		break;
	default:
		break;
	}
	Frag name = ml_text(func_name(e->op), px, FW_REG, MR_NORMAL);
	if (e->op == F_ASIN || e->op == F_ACOS || e->op == F_ATAN)
		name = ml_sup(name, ml_text(MINUS "1", sp, FW_REG, MR_NORMAL), px);
	return ml_cat(name, paren(lay_args(e, px), px));
}

/* For a sum term: is it "minus something"? Returns the positive part or NULL. */
static const Expr *minus_part(const Expr *k)
{
	if (k->flags & EF_PAREN)
		return NULL;
	if (k->kind == E_NEG && k->nkids == 1)
		return k->kids[0];
	if (k->kind == E_NUM && k->r.n < 0) {
		Expr *a = ex_num(rat_neg(k->r));
		a->flags = k->flags;
		return a;
	}
	if (k->kind == E_REAL && k->v < 0)
		return ex_real(-k->v);
	if (k->kind == E_MUL && k->nkids >= 2 && k->kids[0]->kind == E_NUM && k->kids[0]->r.n < 0 &&
	    !(k->kids[0]->flags & EF_PAREN)) {
		Expr **kids = amalloc(sizeof(Expr *) * (size_t)k->nkids);
		memcpy(kids, k->kids, sizeof(Expr *) * (size_t)k->nkids);
		Rat pos = rat_neg(k->kids[0]->r);
		if (rat_is_one(pos))
			return k->nkids == 2 ? k->kids[1] : ex_node(E_MUL, k->nkids - 1, kids + 1);
		kids[0] = ex_num(pos);
		kids[0]->flags = k->kids[0]->flags;
		return ex_node(E_MUL, k->nkids, kids);
	}
	return NULL;
}

static Frag lay_mul(const Expr *e, int px)
{
	Frag f = ml_empty();
	int first = 0;
	if (e->nkids >= 2 && e->kids[0]->kind == E_NUM && e->kids[0]->r.n == -1 && e->kids[0]->r.d == 1 &&
	    !(e->kids[0]->flags & EF_PAREN)) {
		f = ml_text(MINUS, px, FW_REG, MR_NORMAL);
		first = 1;
	}
	for (int i = first; i < e->nkids; i++) {
		const Expr *k = e->kids[i];
		if (i > first) {
			const Expr *prev = e->kids[i - 1];
			if (starts_with_digit(k) || (prev->kind == E_NUM && k->kind == E_NUM))
				f = ml_cat(f, op_text(TIMES, px));
			else if (k->kind == E_FUNC || prev->kind == E_FUNC)
				f = ml_cat(f, ml_space(px / 6, px));
			else
				f = ml_cat(f, ml_space(1, px));
		}
		f = ml_cat(f, child(k, px, i == first ? 3 : 5));
	}
	return f;
}

static const char *rel_text(int op)
{
	switch (op) {
	case R_LT: return "<";
	case R_LE: return "\xE2\x89\xA4";
	case R_GT: return ">";
	case R_GE: return "\xE2\x89\xA5";
	case R_NE: return "\xE2\x89\xA0";
	default:   return "=";
	}
}

static Frag lay_core(const Expr *e, int px)
{
	switch (e->kind) {
	case E_NUM:   return lay_num(e, px);
	case E_REAL:  return mlx_real(e->v, px);
	case E_VAR:   return ml_text(e->name, px, FW_REG, MR_NORMAL);
	case E_CONST: return ml_text(e->op == C_PI ? "\xCF\x80" : "e", px, FW_REG, MR_NORMAL);
	case E_TEXT: {
		Frag f = ml_space(px / 3, px);
		f = ml_cat(f, ml_text(e->text ? e->text : "", ml_quant(px * 85 / 100), FW_REG, MR_MUTED));
		return ml_cat(f, ml_space(px / 3, px));
	}
	case E_RECUR: return mlx_recur(e, px);
	case E_ADD: {
		Frag f = ml_empty();
		for (int i = 0; i < e->nkids; i++) {
			const Expr *k = e->kids[i];
			const Expr *m = i > 0 ? minus_part(k) : NULL;
			if (i == 0) {
				f = child(k, px, 2);
			} else if (m) {
				f = ml_cat(f, op_text(MINUS, px));
				f = ml_cat(f, child(m, px, 3));
			} else {
				f = ml_cat(f, op_text("+", px));
				f = ml_cat(f, child(k, px, 2));
			}
		}
		return f;
	}
	case E_PM:
		if (e->nkids >= 2) {
			Frag f = child(e->kids[0], px, 2);
			f = ml_cat(f, op_text(PLUSMINUS, px));
			return ml_cat(f, child(e->kids[1], px, 3));
		}
		if (e->nkids == 1)
			return ml_cat(ml_text(PLUSMINUS, px, FW_REG, MR_NORMAL), child(e->kids[0], px, 3));
		break;
	case E_MUL: return lay_mul(e, px);
	case E_DIV:
		if (e->nkids >= 2) {
			g_frac_depth++;
			int sp = g_frac_depth > 1 ? ml_quant(px * 85 / 100) : px;
			Frag n = bare(e->kids[0], sp);
			Frag d = bare(e->kids[1], sp);
			g_frac_depth--;
			return ml_frac(n, d, px);
		}
		break;
	case E_NEG:
		if (e->nkids >= 1)
			return ml_cat(ml_text(MINUS, px, FW_REG, MR_NORMAL), child(e->kids[0], px, 3));
		break;
	case E_POW:
		if (e->nkids >= 2) {
			const Expr *b = e->kids[0];
			Frag base = lay(b, px);
			if (prec(b) < 6 || b->kind == E_DIV || b->kind == E_MIXED || is_stacked_num(b))
				if (!(b->flags & EF_PAREN))
					base = paren(base, px);
			return ml_sup(base, bare(e->kids[1], ml_script_px(px)), px);
		}
		break;
	case E_FUNC: return lay_func(e, px);
	case E_REL: {
		Frag f = ml_empty();
		for (int i = 0; i < e->nkids; i++) {
			if (i) {
				int op = (i - 1 < EXPR_MAX_REL) ? e->ops[i - 1] : e->op;
				f = ml_cat(f, op_text(rel_text(op ? op : e->op), px));
			}
			f = ml_cat(f, child(e->kids[i], px, 2));
		}
		return f;
	}
	case E_LIST: {
		Frag f = ml_empty();
		for (int i = 0; i < e->nkids; i++) {
			/* x=2 "or" x=-3: words join the items, no commas round them */
			if (i && ex_list_comma(e, i)) {
				f = ml_cat(f, ml_text(",", px, FW_REG, MR_NORMAL));
				f = ml_cat(f, ml_space(px * 7 / 10, px));
			}
			f = ml_cat(f, lay(e->kids[i], px));
		}
		return f;
	}
	case E_PCT: case E_FACT: case E_DEG:
		if (e->nkids >= 1) {
			Frag f = child(e->kids[0], px, 6);
			const char *s = e->kind == E_PCT ? "%" : e->kind == E_FACT ? "!" : "\xC2\xB0";
			return ml_cat(f, ml_text(s, px, FW_REG, MR_NORMAL));
		}
		break;
	case E_MIXED:
		if (e->nkids >= 3) {
			Frag w = lay(e->kids[0], px);
			int sp = ml_quant(px * 80 / 100);
			Frag n = lay(e->kids[1], sp);
			Frag d = lay(e->kids[2], sp);
			Frag fr = ml_frac(n, d, px);
			w = ml_cat(w, ml_space(2, px));
			return ml_cat(w, fr);
		}
		break;
	default:
		break;
	}
	return ml_text("?", px, FW_REG, MR_ERROR);
}

static Frag lay(const Expr *e, int px)
{
	if (!e)
		return ml_slot(px);
	Frag f = lay_core(e, px);
	if (e->flags & EF_PAREN)
		f = paren(f, px);
	return f;
}

Frag ml_expr(const struct Expr *e, int px)
{
	g_frac_depth = 0;
	return lay(e, px);
}
