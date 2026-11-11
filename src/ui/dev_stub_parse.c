/* dev_stub_parse.c - UI_ONLY stand-ins for ex_parse / ex_print / ex_eval, used
 * only until the engine's parse.c / print.c / eval.c join the UI build
 * (.markers/front-ready -> SUMVITA_HAVE_FRONT). Small recursive descent over
 * the common parts of docs/SYNTAX.md so the renderer can be exercised. */
#if defined(SUMVITA_UI_ONLY) && !defined(SUMVITA_HAVE_FRONT)

#include "engine/arena.h"
#include "engine/expr.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *P;
static const char *P0;
static char *g_err;
static int g_errcap, g_bad;

static void fail(const char *m)
{
	if (!g_bad && g_err && g_errcap > 0)
		snprintf(g_err, (size_t)g_errcap, "%s", m);
	g_bad = 1;
}

static void ws(void) { while (*P == ' ') P++; }
static int eat(const char *s)
{
	ws();
	size_t n = strlen(s);
	if (!strncmp(P, s, n)) { P += n; return 1; }
	return 0;
}

static Expr *flat2(ExprKind k, Expr *a, Expr *b)
{
	if (a->kind == k && !(a->flags & EF_PAREN)) {
		Expr **kk = amalloc(sizeof(Expr *) * (size_t)(a->nkids + 1));
		memcpy(kk, a->kids, sizeof(Expr *) * (size_t)a->nkids);
		kk[a->nkids] = b;
		Expr *e = ex_node(k, a->nkids + 1, kk);
		return e;
	}
	Expr *kk[2] = { a, b };
	return ex_node(k, 2, kk);
}

static const struct { const char *n; int f; } k_fn[] = {
	{ "asin", F_ASIN }, { "acos", F_ACOS }, { "atan", F_ATAN }, { "sinh", F_SINH }, { "cosh", F_COSH },
	{ "tanh", F_TANH }, { "sin", F_SIN }, { "cos", F_COS }, { "tan", F_TAN }, { "ln", F_LN },
	{ "log", F_LOG }, { "sqrt", F_SQRT }, { "\xE2\x88\x9A", F_SQRT }, { "root", F_ROOT }, { "abs", F_ABS },
	{ "floor", F_FLOOR }, { "ceil", F_CEIL }, { "round", F_ROUND }, { "nCr", F_NCR }, { "nPr", F_NPR },
	{ "exp", F_EXP }, { NULL, 0 } };

static Expr *p_list(void);
static Expr *p_add(void);
static Expr *p_unary(void);

static Expr *p_number(void)
{
	long long n = 0, d = 1;
	int dec = 0;
	while ((*P >= '0' && *P <= '9') || *P == '.') {
		if (*P == '.') dec = 1;
		else { n = n * 10 + (*P - '0'); if (dec) d *= 10; }
		P++;
	}
	Expr *e = ex_num(rat_make(n, d));
	if (dec) e->flags |= NF_DECIMAL;
	if (*P == '_' && !dec) {
		P++;
		Expr *k[3] = { e, p_number(), NULL };
		if (*P == '/') { P++; k[2] = p_number(); } else k[2] = ex_int(1);
		return ex_node(E_MIXED, 3, k);
	}
	return e;
}

static Expr *p_atom(void)
{
	ws();
	unsigned char c = (unsigned char)*P;
	if ((c >= '0' && c <= '9') || c == '.')
		return p_number();
	if (eat("(") || eat("[")) {
		Expr *e = p_list();
		if (!eat(")") && !eat("]")) fail("Missing )");
		if (e) e->flags |= EF_PAREN;
		return e;
	}
	if (eat("|")) {
		Expr *e = p_add();
		if (!eat("|")) fail("Missing |");
		return ex_func1(F_ABS, e);
	}
	if (eat("\"")) {
		const char *s = P;
		while (*P && *P != '"') P++;
		Expr *e = ex_text(astrndup(s, (size_t)(P - s)));
		if (*P) P++;
		return e;
	}
	if (eat("recur(")) {
		const char *s = P;
		int depth = 1;
		while (*P && depth) { if (*P == '(') depth++; if (*P == ')') depth--; if (depth) P++; }
		Expr *e = ex_node(E_RECUR, 0, NULL);
		e->text = astrndup(s, (size_t)(P - s));
		if (*P) P++;
		return e;
	}
	for (int i = 0; k_fn[i].n; i++) {
		if (eat(k_fn[i].n)) {
			int f = k_fn[i].f;
			if (eat("(")) {
				Expr *a = p_list();
				if (!eat(")")) fail("Missing )");
				if (a && a->kind == E_LIST && !(a->flags & EF_PAREN))
					{ Expr *e = ex_node(E_FUNC, a->nkids, a->kids); e->op = f; return e; }
				return ex_func1(f, a);
			}
			return ex_func1(f, p_unary());
		}
	}
	if (eat("Ans")) return ex_var("Ans");
	if (eat("\xCF\x80") || eat("pi")) return ex_const(C_PI);
	if (eat("\xCE\xB8")) return ex_var("\xCE\xB8");
	if ((c | 32) >= 'a' && (c | 32) <= 'z') {
		P++;
		if (c == 'e') return ex_const(C_E);
		char nm[2] = { (char)c, 0 };
		return ex_var(nm);
	}
	fail(*P ? "Unexpected symbol" : "Incomplete");
	if (*P) P++;
	return ex_int(0);
}

static Expr *p_postfix(void)
{
	Expr *e = p_atom();
	for (;;) {
		if (eat("!")) e = ex_node(E_FACT, 1, &e);
		else if (eat("%")) e = ex_node(E_PCT, 1, &e);
		else if (eat("\xC2\xB0")) e = ex_node(E_DEG, 1, &e);
		else return e;
	}
}

static Expr *p_pow(void)
{
	Expr *b = p_postfix();
	if (eat("^")) return ex_pow2(b, p_unary());
	return b;
}

static Expr *p_unary(void)
{
	if (eat("-") || eat("\xE2\x88\x92")) return ex_neg(p_unary());
	if (eat("\xC2\xB1")) { Expr *k = p_unary(); return ex_node(E_PM, 1, &k); }
	if (eat("+")) return p_unary();
	return p_pow();
}

static int starts_atom(void)
{
	ws();
	unsigned char c = (unsigned char)*P;
	return (c >= '0' && c <= '9') || c == '.' || c == '(' || c == '[' || ((c | 32) >= 'a' && (c | 32) <= 'z') ||
	       !strncmp(P, "\xCF\x80", 2) || !strncmp(P, "\xE2\x88\x9A", 3) || !strncmp(P, "\xCE\xB8", 2);
}

static Expr *p_mul(void)
{
	Expr *e = p_unary();
	for (;;) {
		if (eat("\xC3\x97") || eat("*")) e = flat2(E_MUL, e, p_unary());
		else if (eat("\xC3\xB7") || eat("/")) e = ex_div2(e, p_unary());
		else if (starts_atom()) e = flat2(E_MUL, e, p_pow());
		else return e;
	}
}

static Expr *p_add(void)
{
	Expr *e = p_mul();
	for (;;) {
		if (eat("+")) e = flat2(E_ADD, e, p_mul());
		else if (eat("-") || eat("\xE2\x88\x92")) e = flat2(E_ADD, e, ex_neg(p_mul()));
		else if (eat("\xC2\xB1")) { Expr *k[2] = { e, p_mul() }; e = ex_node(E_PM, 2, k); }
		else return e;
	}
}

static Expr *p_rel(void)
{
	static const struct { const char *s; int op; } rel[] = {
		{ "<=", R_LE }, { ">=", R_GE }, { "!=", R_NE }, { "\xE2\x89\xA4", R_LE }, { "\xE2\x89\xA5", R_GE },
		{ "\xE2\x89\xA0", R_NE }, { "=", R_EQ }, { "<", R_LT }, { ">", R_GT }, { NULL, 0 } };
	Expr *e = p_add();
	for (;;) {
		int op = 0;
		for (int i = 0; rel[i].s && !op; i++)
			if (eat(rel[i].s)) op = rel[i].op;
		if (!op) return e;
		Expr *r = p_add();
		if (e->kind == E_REL && !(e->flags & EF_PAREN) && e->nkids < EXPR_MAX_REL) {
			Expr *n = flat2(E_REL, e, r);
			n->op = e->op;
			memcpy(n->ops, e->ops, sizeof n->ops);
			n->ops[e->nkids - 1] = op;
			e = n;
		} else {
			e = ex_rel(op, e, r);
			e->ops[0] = op;
		}
	}
}

static Expr *p_list(void)
{
	Expr *e = p_rel();
	while (eat(",")) e = flat2(E_LIST, e, p_rel());
	return e;
}

Expr *ex_parse(const char *src, char *err, int errcap, int *err_pos)
{
	P = P0 = src ? src : "";
	g_err = err;
	g_errcap = errcap;
	g_bad = 0;
	Expr *e = p_list();
	ws();
	if (*P) fail("Unexpected symbol");
	if (g_bad) {
		if (err_pos) *err_pos = (int)(P - P0);
		return NULL;
	}
	return e;
}

const char *ex_print(const Expr *e)
{
	(void)e;
	return astrdup("?");
}

const char *ex_print_plain(const Expr *e) { return ex_print(e); }

bool ex_eval(const Expr *e, const EvalEnv *env, double *out)
{
	double a = 0, b = 0, r = NAN;
	if (!e) { *out = NAN; return false; }
	if (e->nkids >= 1 && e->kind != E_ADD && e->kind != E_MUL && e->kind != E_LIST && !ex_eval(e->kids[0], env, &a))
		return false;
	if (e->nkids >= 2 && e->kind != E_ADD && e->kind != E_MUL && e->kind != E_LIST && !ex_eval(e->kids[1], env, &b))
		return false;
	double tr = env && env->degrees ? M_PI / 180.0 : 1.0;
	switch (e->kind) {
	case E_NUM: case E_RECUR: r = (double)e->r.n / (double)e->r.d; break;
	case E_REAL: r = e->v; break;
	case E_CONST: r = e->op == C_PI ? M_PI : M_E; break;
	case E_VAR:
		for (int i = 0; env && i < env->nvars; i++)
			if (!strcmp(env->var_names[i], e->name)) r = env->var_values[i];
		break;
	case E_ADD: case E_MUL:
		r = e->kind == E_ADD ? 0 : 1;
		for (int i = 0; i < e->nkids; i++) {
			if (!ex_eval(e->kids[i], env, &a)) return false;
			r = e->kind == E_ADD ? r + a : r * a;
		}
		break;
	case E_DIV: r = a / b; break;
	case E_NEG: r = -a; break;
	case E_POW: r = pow(a, b); break;
	case E_PCT: r = a / 100; break;
	case E_DEG: r = env && env->degrees ? a : a * M_PI / 180; break;
	case E_FACT: r = tgamma(a + 1); break;
	case E_MIXED: { double c = 0; ex_eval(e->kids[2], env, &c); r = a + b / c; break; }
	case E_FUNC:
		switch (e->op) {
		case F_SIN: r = sin(a * tr); break;
		case F_COS: r = cos(a * tr); break;
		case F_TAN: r = tan(a * tr); break;
		case F_ASIN: r = asin(a) / tr; break;
		case F_ACOS: r = acos(a) / tr; break;
		case F_ATAN: r = atan(a) / tr; break;
		case F_LN: r = log(a); break;
		case F_LOG: r = e->nkids >= 2 ? log(b) / log(a) : log10(a); break;
		case F_SQRT: r = sqrt(a); break;
		case F_ROOT: r = pow(b, 1.0 / a); break;
		case F_ABS: r = fabs(a); break;
		case F_EXP: r = exp(a); break;
		default: r = NAN; break;
		}
		break;
	default: break;
	}
	*out = r;
	return isfinite(r);
}

#else
typedef int dev_stub_parse_unused_t;
#endif
