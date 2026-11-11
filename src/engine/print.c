/* ex_print / ex_print_plain: Expr -> DISPLAY SYNTAX (docs/SYNTAX.md).
 * Every node is printed to an arena string together with the precedence of
 * the text produced; a parent brackets a child whose text binds too loosely.
 * The output re-parses with ex_parse to the same tree (up to EF_PAREN). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "expr.h"
#include "parse_int.h"

enum { PL_LIST, PL_REL, PL_ADD, PL_MUL, PL_UNARY, PL_POW, PL_POST, PL_ATOM };

typedef struct { char *s; int n, cap; } SB;

static void sb_put(SB *b, const char *s, int len)
{
    if (b->n + len + 1 > b->cap) {
        int nc = b->cap ? b->cap : 64;
        while (b->n + len + 1 > nc) nc *= 2;
        char *ns = amalloc((size_t)nc);
        if (b->n) memcpy(ns, b->s, (size_t)b->n);
        b->s = ns;
        b->cap = nc;
    }
    memcpy(b->s + b->n, s, (size_t)len);
    b->n += len;
    b->s[b->n] = 0;
}

static void sb_puts(SB *b, const char *s) { sb_put(b, s, (int)strlen(s)); }
static const char *sb_str(SB *b) { return b->s ? b->s : ""; }

typedef struct { const char *s; int prec; } Out;

static Out pr(const Expr *e, int minprec, bool honor_paren, bool plain);

static Out mk(const char *s, int prec) { Out o = {s, prec}; return o; }
static bool isdig(char c) { return c >= '0' && c <= '9'; }
static bool isalph(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

static const char *FNAMES[F_FUNC_COUNT] = {
    [F_SIN] = "sin", [F_COS] = "cos", [F_TAN] = "tan", [F_ASIN] = "asin", [F_ACOS] = "acos",
    [F_ATAN] = "atan", [F_SINH] = "sinh", [F_COSH] = "cosh", [F_TANH] = "tanh", [F_LN] = "ln",
    [F_LOG] = "log", [F_SQRT] = "sqrt", [F_ROOT] = "root", [F_ABS] = "abs", [F_FLOOR] = "floor",
    [F_CEIL] = "ceil", [F_ROUND] = "round", [F_NCR] = "nCr", [F_NPR] = "nPr", [F_EXP] = "exp",
};

/* decimal places a terminating denominator needs */
static int dec_places(int64_t d)
{
    int a = 0, b = 0;
    while (d % 2 == 0) { d /= 2; a++; }
    while (d % 5 == 0) { d /= 5; b++; }
    return d == 1 ? (a > b ? a : b) : -1;
}

static Out num_out(Rat r, unsigned flags)
{
    char buf[96];
    int dp = dec_places(r.d);
    if (r.d == 1) {
        snprintf(buf, sizeof buf, "%lld", (long long)r.n);
    } else if ((flags & NF_DECIMAL) && dp >= 0 && dp <= 30) {
        rat_to_decimal_str(r, 30, buf, sizeof buf);
    } else {
        snprintf(buf, sizeof buf, "%lld/%lld", (long long)r.n, (long long)r.d);
        return mk(astrdup(buf), PL_MUL);
    }
    return mk(astrdup(buf), r.n < 0 ? PL_UNARY : PL_ATOM);
}

static Out real_out(double v, bool plain)
{
    if (isnan(v) || isinf(v)) return mk("\"undefined\"", PL_ATOM);
    char buf[64];
    dbl_to_str(v, 10, buf, sizeof buf);
    char *E = strchr(buf, 'E');
    if (E) {
        *E = 0;
        return mk(asprintf_a("%s%s10^%s", buf, plain ? "*" : "\xC3\x97", E + 1), PL_MUL);
    }
    return mk(astrdup(buf), v < 0 ? PL_UNARY : PL_ATOM);
}

/* Append factor text f to a product being built in b. */
static void join_factor(SB *b, const char *f, bool plain)
{
    if (b->n > 0) {
        char last = b->s[b->n - 1];
        if (isdig(f[0]) || f[0] == '.') {
            sb_puts(b, plain ? "*" : "\xC3\x97");
        } else if (isalph(last) && isalph(f[0])) {
            char tmp[16];
            int a = 0, k = b->n;
            while (k > 0 && a < 6 && isalph(b->s[k - 1])) { k--; a++; }
            memcpy(tmp, b->s + k, (size_t)a);
            int c = 0;
            while (c < 6 && isalph(f[c])) { tmp[a + c] = f[c]; c++; }
            tmp[a + c] = 0;
            if (lex_keyword_spans(tmp, a)) sb_puts(b, " ");
        }
    }
    sb_puts(b, f);
}

static int count_mul(const Expr *e)
{
    int n = 0;
    for (int i = 0; i < e->nkids; i++) {
        const Expr *k = e->kids[i];
        n += (k->kind == E_MUL && !(k->flags & EF_PAREN)) ? count_mul(k) : 1;
    }
    return n;
}

static void flatten_mul(const Expr *e, const Expr **out, int *n)
{
    for (int i = 0; i < e->nkids; i++) {
        const Expr *k = e->kids[i];
        if (k->kind == E_MUL && !(k->flags & EF_PAREN)) flatten_mul(k, out, n);
        else out[(*n)++] = k;
    }
}

static Out mul_out(const Expr *e, bool plain)
{
    const Expr **f = amalloc(sizeof(Expr *) * (size_t)(count_mul(e) + 1));
    int n = 0;
    flatten_mul(e, f, &n);
    if (n == 0) return mk("1", PL_ATOM);
    SB b = {0};
    int start = 1;
    const Expr *c = f[0];
    if (n >= 2 && c->kind == E_NUM && c->r.d != 1 && !(c->flags & (EF_PAREN | NF_DECIMAL))) {
        /* rational coefficient: (1/2)x -> x/2, (2/3)x -> 2/3x */
        if (c->r.n == 1 || c->r.n == -1) {
            SB num = {0};
            for (int i = 1; i < n; i++) join_factor(&num, pr(f[i], i == 1 ? PL_MUL : PL_POW, true, plain).s, plain);
            return mk(asprintf_a("%s%s/%lld", c->r.n < 0 ? "-" : "", sb_str(&num), (long long)c->r.d), PL_MUL);
        }
        sb_puts(&b, asprintf_a("%lld/%lld", (long long)c->r.n, (long long)c->r.d));
    } else {
        sb_puts(&b, pr(c, PL_MUL, true, plain).s);
    }
    for (int i = start; i < n; i++) join_factor(&b, pr(f[i], PL_POW, true, plain).s, plain);
    return mk(sb_str(&b), PL_MUL);
}

static Out add_out(const Expr *e, SB *b, bool first_done, bool plain)
{
    for (int i = 0; i < e->nkids; i++) {
        const Expr *k = e->kids[i];
        bool first = !first_done && i == 0;
        if (k->kind == E_ADD && !(k->flags & EF_PAREN)) {
            add_out(k, b, !first, plain);
            continue;
        }
        const char *s = pr(k, first ? PL_ADD : PL_MUL, true, plain).s;
        if (!first && s[0] != '-') sb_puts(b, "+");
        sb_puts(b, s);
    }
    return mk(sb_str(b), PL_ADD);
}

/* "-x", "±x": operand printed at product level; a bare product keeps PL_MUL. */
static Out sign_out(const char *sign, const Expr *a, bool plain)
{
    Out o = pr(a, PL_MUL, true, plain);
    return mk(asprintf_a("%s%s", sign, o.s), o.prec >= PL_POW ? PL_UNARY : PL_MUL);
}

static Out func_out(const Expr *e, bool plain)
{
    if (e->op == F_SQRT && e->nkids == 1 && !plain) {
        Out o = pr(e->kids[0], PL_REL, false, plain);
        if (o.prec >= PL_ATOM && o.s[0] != '-') return mk(asprintf_a("\xE2\x88\x9A%s", o.s), PL_ATOM);
        return mk(asprintf_a("\xE2\x88\x9A(%s)", o.s), PL_ATOM);
    }
    if (e->op == F_ABS && e->nkids == 1 && !plain) {
        Out o = pr(e->kids[0], PL_REL, false, plain);
        if (!strchr(o.s, '|')) return mk(asprintf_a("|%s|", o.s), PL_ATOM);
    }
    SB b = {0};
    sb_puts(&b, e->op > 0 && e->op < F_FUNC_COUNT && FNAMES[e->op] ? FNAMES[e->op] : "f");
    sb_puts(&b, "(");
    for (int i = 0; i < e->nkids; i++) {
        if (i) sb_puts(&b, ",");
        sb_puts(&b, pr(e->kids[i], PL_REL, false, plain).s);
    }
    sb_puts(&b, ")");
    return mk(sb_str(&b), PL_ATOM);
}

static const char *rel_sym(int op, bool plain)
{
    switch (op) {
    case R_LT: return "<";
    case R_GT: return ">";
    case R_LE: return plain ? "<=" : "\xE2\x89\xA4";
    case R_GE: return plain ? ">=" : "\xE2\x89\xA5";
    case R_NE: return plain ? "!=" : "\xE2\x89\xA0";
    default: return "=";
    }
}

static Out body(const Expr *e, bool plain)
{
    SB b = {0};
    switch (e->kind) {
    case E_NUM: return num_out(e->r, e->flags);
    case E_REAL: return real_out(e->v, plain);
    case E_VAR:
        if (plain && strcmp(e->name, "\xCE\xB8") == 0) return mk("theta", PL_ATOM);
        return mk(astrdup(e->name), PL_ATOM);
    case E_CONST:
        if (e->op == C_PI) return mk(plain ? "pi" : "\xCF\x80", PL_ATOM);
        return mk("e", PL_ATOM);
    case E_TEXT: return mk(asprintf_a("\"%s\"", e->text ? e->text : ""), PL_ATOM);
    case E_ADD: return add_out(e, &b, false, plain);
    case E_MUL: return mul_out(e, plain);
    case E_DIV:
        return mk(asprintf_a("%s/%s", pr(e->kids[0], PL_MUL, true, plain).s,
                             pr(e->kids[1], PL_POW, true, plain).s), PL_MUL);
    case E_NEG: return sign_out("-", e->kids[0], plain);
    case E_PM:
        if (e->nkids == 1) return sign_out(plain ? "+/-" : "\xC2\xB1", e->kids[0], plain);
        return mk(asprintf_a("%s%s%s", pr(e->kids[0], PL_ADD, true, plain).s, plain ? "+/-" : "\xC2\xB1",
                             pr(e->kids[1], PL_MUL, true, plain).s), PL_ADD);
    case E_POW:
        return mk(asprintf_a("%s^%s", pr(e->kids[0], PL_POST, true, plain).s,
                             pr(e->kids[1], PL_UNARY, true, plain).s), PL_POW);
    case E_FUNC: return func_out(e, plain);
    case E_REL:
        for (int i = 0; i < e->nkids; i++) {
            if (i) {
                int op = (i - 1 < EXPR_MAX_REL && e->ops[i - 1]) ? e->ops[i - 1] : e->op;
                const char *sym = rel_sym(op, plain);
                if (sym[0] == '=' && b.n && b.s[b.n - 1] == '!') sb_puts(&b, " ");
                sb_puts(&b, sym);
            }
            sb_puts(&b, pr(e->kids[i], PL_ADD, true, plain).s);
        }
        return mk(sb_str(&b), PL_REL);
    case E_LIST:
        for (int i = 0; i < e->nkids; i++) {
            if (i) sb_puts(&b, ex_list_comma(e, i) ? ", " : " ");
            sb_puts(&b, pr(e->kids[i], PL_REL, true, plain).s);
        }
        return mk(sb_str(&b), PL_LIST);
    case E_PCT: case E_FACT: case E_DEG: {
        const char *sym = e->kind == E_PCT ? "%" : (e->kind == E_FACT ? "!" : (plain ? "deg" : "\xC2\xB0"));
        return mk(asprintf_a("%s%s", pr(e->kids[0], PL_POST, true, plain).s, sym), PL_POST);
    }
    case E_MIXED:
        if (e->nkids == 3)
            return mk(asprintf_a("%s_%s/%s", pr(e->kids[0], PL_ATOM, true, plain).s,
                                 pr(e->kids[1], PL_ATOM, true, plain).s, pr(e->kids[2], PL_ATOM, true, plain).s),
                      PL_ATOM);
        return mk("\"?\"", PL_ATOM);
    case E_RECUR:
        if (e->text) return mk(asprintf_a("recur(%s)", e->text), PL_ATOM);
        {
            char buf[96];
            if (rat_to_recurring_str(e->r, buf, sizeof buf)) return mk(asprintf_a("recur(%s)", buf), PL_ATOM);
            return num_out(e->r, 0);
        }
    }
    return mk("\"?\"", PL_ATOM);
}

static Out pr(const Expr *e, int minprec, bool honor_paren, bool plain)
{
    if (!e) return mk("", PL_ATOM);
    Out o = body(e, plain);
    if ((honor_paren && (e->flags & EF_PAREN)) || o.prec < minprec)
        return mk(asprintf_a("(%s)", o.s), PL_ATOM);
    return o;
}

const char *ex_print(const Expr *e) { return pr(e, 0, true, false).s; }
const char *ex_print_plain(const Expr *e) { return pr(e, 0, true, true).s; }
