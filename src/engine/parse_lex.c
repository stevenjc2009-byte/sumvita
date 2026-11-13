/* Tokeniser for DISPLAY SYNTAX (docs/SYNTAX.md). Numbers, mixed numbers and
 * recur(...) become ready-made Expr atoms here; parse.c does the grammar. */
#include "parse_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

enum { KV_FUNC = 0, KV_RECUR, KV_PI, KV_ANS, KV_OF };

typedef struct {
    const char *s;
    int fid;
    int kind;       /* KV_* */
    int variant;    /* FV_* for functions */
} Kw;

static const Kw KW[] = {
    {"asin", F_ASIN, KV_FUNC, 0}, {"acos", F_ACOS, KV_FUNC, 0}, {"atan", F_ATAN, KV_FUNC, 0},
    {"sinh", F_SINH, KV_FUNC, 0}, {"cosh", F_COSH, KV_FUNC, 0}, {"tanh", F_TANH, KV_FUNC, 0},
    {"sin", F_SIN, KV_FUNC, 0}, {"cos", F_COS, KV_FUNC, 0}, {"tan", F_TAN, KV_FUNC, 0},
    {"ln", F_LN, KV_FUNC, 0}, {"log", F_LOG, KV_FUNC, 0}, {"sqrt", F_SQRT, KV_FUNC, 0},
    {"cbrt", F_ROOT, KV_FUNC, FV_CBRT}, {"root", F_ROOT, KV_FUNC, 0}, {"abs", F_ABS, KV_FUNC, 0},
    {"floor", F_FLOOR, KV_FUNC, 0}, {"ceil", F_CEIL, KV_FUNC, 0}, {"round", F_ROUND, KV_FUNC, 0},
    {"nCr", F_NCR, KV_FUNC, 0}, {"nPr", F_NPR, KV_FUNC, 0}, {"exp", F_EXP, KV_FUNC, 0},
    {"recur", 0, KV_RECUR, 0}, {"pi", 0, KV_PI, 0}, {"Ans", 0, KV_ANS, 0},
    {"of", 0, KV_OF, 0},    /* "20% of 300": of means times */
};
#define NKW ((int)(sizeof KW / sizeof KW[0]))

/* Longest keyword at s, or -1. */
static int kw_match(const char *s)
{
    int best = -1, bestlen = 0;
    for (int i = 0; i < NKW; i++) {
        int l = (int)strlen(KW[i].s);
        if (l > bestlen && strncmp(s, KW[i].s, (size_t)l) == 0) { best = i; bestlen = l; }
    }
    return best;
}

bool lex_keyword_spans(const char *s, int split)
{
    int n = (int)strlen(s);
    for (int st = 0; st < split; st++)
        for (int i = 0; i < NKW; i++) {
            int l = (int)strlen(KW[i].s);
            if (st + l > split && st + l <= n && strncmp(s + st, KW[i].s, (size_t)l) == 0) return true;
        }
    return false;
}

typedef struct {
    const char *src;
    Tok *t;
    int n, cap;
    char *err;
    int errcap;
    int *err_pos;
} Lx;

static Tok *push(Lx *L, TokKind k, int pos, int len)
{
    if (L->n == L->cap) {
        int nc = L->cap ? L->cap * 2 : 32;
        Tok *nt = amalloc(sizeof(Tok) * (size_t)nc);
        if (L->n) memcpy(nt, L->t, sizeof(Tok) * (size_t)L->n);
        L->t = nt;
        L->cap = nc;
    }
    Tok *t = &L->t[L->n++];
    memset(t, 0, sizeof *t);
    t->k = k;
    t->pos = pos;
    t->len = len;
    return t;
}

static bool lfail(Lx *L, int pos, const char *msg)
{
    if (L->err && L->errcap > 0) snprintf(L->err, (size_t)L->errcap, "%s", msg);
    if (L->err_pos) *L->err_pos = pos;
    return false;
}

static bool isdig(char c) { return c >= '0' && c <= '9'; }
static bool isalph(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

static Expr *num_atom(const char *s, int len, bool dec)
{
    char buf[80];
    if (len >= (int)sizeof buf) {
        char *big = astrndup(s, (size_t)len);
        return ex_real(strtod(big, NULL));
    }
    memcpy(buf, s, (size_t)len);
    buf[len] = 0;
    Rat r;
    if (rat_from_decimal_str(buf, &r)) {
        Expr *e = ex_num(r);
        if (dec) e->flags |= NF_DECIMAL;
        return e;
    }
    return ex_real(strtod(buf, NULL));
}

/* digits string -> int64 (false on overflow / empty allowed as 0) */
static bool digits_i64(const char *s, int len, int64_t *out)
{
    int64_t v = 0;
    for (int i = 0; i < len; i++)
        if (!i64_mul_ok(v, 10, &v) || !i64_add_ok(v, s[i] - '0', &v)) return false;
    *out = v;
    return true;
}

/* Recurring-decimal body I . A ( R ) at *pp; on success *out is the E_RECUR
 * atom and *pp points just past it. Used by recur(...) and the bare form. */
static bool recur_body(Lx *L, const char **pp, int kwpos, Expr **out)
{
    const char *p = *pp;
    const char *i0 = p;
    while (isdig(*p)) p++;
    const char *i1 = p, *a0 = p, *a1 = p, *r0 = p, *r1 = p;
    if (*p == '.') {
        p++;
        a0 = p;
        while (isdig(*p)) p++;
        a1 = p;
        if (*p == '(') {
            p++;
            r0 = p;
            while (isdig(*p)) p++;
            r1 = p;
            if (*p != ')' || r1 == r0) return lfail(L, (int)(p - L->src), "Type a recurring decimal like 0.1(27)");
            p++;
        }
    }
    if (i1 == i0 && a1 == a0 && r1 == r0) return lfail(L, (int)(p - L->src), "Type a recurring decimal like 0.1(27)");
    int il = (int)(i1 - i0), al = (int)(a1 - a0), rl = (int)(r1 - r0);
    if (il + al + rl > 18) return lfail(L, kwpos, "That recurring decimal is too long");
    int64_t I, A, AR;
    char ar[40];
    memcpy(ar, a0, (size_t)al);
    memcpy(ar + al, r0, (size_t)rl);
    if (!digits_i64(i0, il, &I) || !digits_i64(a0, al, &A) || !digits_i64(ar, al + rl, &AR))
        return lfail(L, kwpos, "That recurring decimal is too long");
    int64_t p10a = 1, p10r = 1;
    for (int i = 0; i < al; i++) p10a *= 10;
    for (int i = 0; i < rl; i++) p10r *= 10;
    Rat frac, val;
    if (rl > 0) {
        int64_t den;
        if (!i64_mul_ok(p10a, p10r - 1, &den)) return lfail(L, kwpos, "That recurring decimal is too long");
        frac = rat_make(AR - A, den);
    } else {
        frac = rat_make(A, p10a);
    }
    if (!rat_add(rat_int(I), frac, &val)) return lfail(L, kwpos, "That recurring decimal is too long");
    Expr *e = amalloc(sizeof(Expr));
    e->kind = E_RECUR;
    e->r = val;
    e->text = asprintf_a("%.*s.%.*s%s%.*s%s", il ? il : 1, il ? i0 : "0", al, a0,
                         rl ? "(" : "", rl, r0, rl ? ")" : "");
    *out = e;
    *pp = p;
    return true;
}

/* After "recur": ( I . A ( R ) ) */
static bool lex_recur(Lx *L, const char **pp, int kwpos)
{
    const char *p = *pp;
    while (*p == ' ') p++;
    if (*p != '(') return lfail(L, (int)(p - L->src), "recur needs brackets: recur(0.1(27))");
    p++;
    while (*p == ' ') p++;
    Expr *e;
    if (!recur_body(L, &p, kwpos, &e)) return false;
    while (*p == ' ') p++;
    if (*p != ')') return lfail(L, (int)(p - L->src), "Type a recurring decimal like 0.1(27)");
    p++;
    Tok *t = push(L, TK_ATOM, kwpos, (int)(p - L->src) - kwpos);
    t->atom = e;
    *pp = p;
    return true;
}

/* Bare recurring decimal "0.3(18)": a number with a '.' directly followed by
 * "(digits)". Anything else after the '(' leaves it a product. */
static bool bare_recur_at(const char *p)
{
    if (*p != '(' || !isdig(p[1])) return false;
    p++;
    while (isdig(*p)) p++;
    return *p == ')';
}

static bool lex_number(Lx *L, const char **pp)
{
    const char *st = *pp, *p = st;
    int pos = (int)(st - L->src);
    while (isdig(*p)) p++;
    bool dec = false;
    int numlen;
    if (*p == '.') {
        dec = true;
        p++;
        while (isdig(*p)) p++;
        numlen = (int)(p - st);
        if (p[-1] == '.') numlen--;            /* "3." means 3 */
    } else {
        numlen = (int)(p - st);
    }
    if (*p == '.') return lfail(L, (int)(p - L->src), "A number can only have one decimal point");
    if (!dec && *p == '_') {                    /* mixed number w_n/d */
        const char *a = p + 1, *q = a;
        while (isdig(*q)) q++;
        if (q == a || *q != '/' || !isdig(q[1])) return lfail(L, (int)(p - L->src), "Type a mixed number like 2_1/3");
        const char *b = q + 1, *r = b;
        while (isdig(*r)) r++;
        int64_t w, nn, dd;
        if (!digits_i64(st, numlen, &w) || !digits_i64(a, (int)(q - a), &nn) || !digits_i64(b, (int)(r - b), &dd))
            return lfail(L, pos, "That number is too big");
        if (dd == 0) return lfail(L, (int)(b - L->src), "Can't divide by 0");
        Expr *k[3] = {ex_int(w), ex_int(nn), ex_int(dd)};
        Tok *t = push(L, TK_ATOM, pos, (int)(r - st));
        t->atom = ex_node(E_MIXED, 3, k);
        *pp = r;
        return true;
    }
    if (dec && bare_recur_at(p)) {
        Expr *e;
        const char *q = st;
        if (!recur_body(L, &q, pos, &e)) return false;
        Tok *t = push(L, TK_ATOM, pos, (int)(q - st));
        t->atom = e;
        *pp = q;
        return true;
    }
    Tok *t = push(L, TK_ATOM, pos, (int)(p - st));
    t->atom = num_atom(st, numlen, dec);
    *pp = p;
    return true;
}

static bool lex_word(Lx *L, const char **pp)
{
    const char *p = *pp;
    int pos = (int)(p - L->src);
    int k = kw_match(p);
    if (k < 0) {
        Tok *t = push(L, TK_ATOM, pos, 1);
        if (*p == 'e') {
            t->atom = ex_const(C_E);
        } else {
            char nm[2] = {*p, 0};
            t->atom = ex_var(nm);
        }
        *pp = p + 1;
        return true;
    }
    const Kw *kw = &KW[k];
    p += strlen(kw->s);
    if (kw->kind == KV_RECUR) { *pp = p; return lex_recur(L, pp, pos); }
    if (kw->kind == KV_OF) { push(L, TK_TIMES, pos, 2); *pp = p; return true; }
    if (kw->kind == KV_PI || kw->kind == KV_ANS) {
        Tok *t = push(L, TK_ATOM, pos, (int)(p - *pp));
        t->atom = kw->kind == KV_PI ? ex_const(C_PI) : ex_var("Ans");
        *pp = p;
        return true;
    }
    int fid = kw->fid;
    if (fid == F_SIN || fid == F_COS || fid == F_TAN) {
        int inv = fid == F_SIN ? F_ASIN : (fid == F_COS ? F_ACOS : F_ATAN);
        if (strncmp(p, "\xE2\x81\xBB\xC2\xB9", 5) == 0) {          /* ⁻¹ */
            fid = inv;
            p += 5;
        } else if (strncmp(p, "^-1", 3) == 0) {
            const char *q = p + 3;
            while (*q == ' ') q++;
            if (*q == '(' || *q == '[') { fid = inv; p += 3; }
        }
    }
    Tok *t = push(L, TK_FUNC, pos, (int)(p - *pp));
    t->op = fid;
    t->variant = kw->variant;
    *pp = p;
    return true;
}

typedef struct { const char *s; TokKind k; int op; } Sym;
static const Sym SYMS[] = {
    {"\xC3\x97", TK_TIMES, 0}, {"\xC2\xB7", TK_TIMES, 0}, {"\xC3\xB7", TK_DIV, 0},
    {"\xE2\x88\x92", TK_MINUS, 0}, {"\xC2\xB1", TK_PM, 0}, {"\xC2\xB0", TK_DEG, 0},
    {"\xE2\x89\xA4", TK_REL, R_LE}, {"\xE2\x89\xA5", TK_REL, R_GE}, {"\xE2\x89\xA0", TK_REL, R_NE},
    {"<=", TK_REL, R_LE}, {">=", TK_REL, R_GE}, {"!=", TK_REL, R_NE},
    {"<", TK_REL, R_LT}, {">", TK_REL, R_GT}, {"=", TK_REL, R_EQ},
    {"+", TK_PLUS, 0}, {"-", TK_MINUS, 0}, {"*", TK_TIMES, 0}, {"/", TK_DIV, 0},
    {"^", TK_POW, 0}, {"!", TK_BANG, 0}, {"%", TK_PCT, 0}, {"|", TK_BAR, 0}, {",", TK_COMMA, 0}, {";", TK_COMMA, 0},
    {"(", TK_LP, '('}, {"[", TK_LP, '['}, {")", TK_RP, ')'}, {"]", TK_RP, ']'},
};
#define NSYM ((int)(sizeof SYMS / sizeof SYMS[0]))

/* Bare "and" / "or" as a list separator, "x=-2 and x=-3", "x=-2andx=-3" (docs/SYNTAX.md):
 * no letter just before it, and the text on BOTH sides, up to the next separator (',' or
 * such a word), has a relation sign. Else letters stay letters: "and", "x=and", "2a+n". */
static int sep_word_at(const char *s, int i)
{
    if (i > 0 && isalph(s[i - 1])) return 0;
    return strncmp(s + i, "and", 3) == 0 ? 3 : (strncmp(s + i, "or", 2) == 0 ? 2 : 0);
}

static bool rel_at(const unsigned char *s)
{
    return *s == '=' || *s == '<' || *s == '>' ||
           (s[0] == 0xE2 && s[1] == 0x89 && (s[2] == 0xA4 || s[2] == 0xA5 || s[2] == 0xA0));
}

static int lex_sep_word(const char *s, int i)
{
    int l = sep_word_at(s, i), before = 0, after = 0;
    for (int j = i - 1; l && j >= 0 && !before && s[j] != ',' && s[j] != ';' && !sep_word_at(s, j); j--)
        before = rel_at((const unsigned char *)s + j);
    for (int j = i + l; l && s[j] && !after && s[j] != ',' && s[j] != ';' && !sep_word_at(s, j); j++)
        after = rel_at((const unsigned char *)s + j);
    return before && after ? l : 0;
}

static int utf8_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

bool lex_all(const char *src, Tok **out, int *ntok, char *err, int errcap, int *err_pos)
{
    Lx L = {src, NULL, 0, 0, err, errcap, err_pos};
    const char *p = src;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        int pos = (int)(p - src);
        if (!*p) { push(&L, TK_END, pos, 0); break; }
        if (isdig(*p) || (*p == '.' && isdig(p[1]))) {
            if (!lex_number(&L, &p)) return false;
            continue;
        }
        if (isalph(*p)) {
            int sl = lex_sep_word(src, pos);           /* "x=1 and x=2": a comma */
            if (sl) { push(&L, TK_COMMA, pos, sl); p += sl; continue; }
            if (!lex_word(&L, &p)) return false;
            continue;
        }
        if (*p == '"') {
            const char *q = strchr(p + 1, '"');
            if (!q) return lfail(&L, pos, "A \" is missing to close the text");
            Tok *t = push(&L, TK_TEXT, pos, (int)(q + 1 - p));
            t->text = astrndup(p + 1, (size_t)(q - p - 1));
            p = q + 1;
            continue;
        }
        if (strncmp(p, "\xE2\x88\x9A", 3) == 0) {                   /* √ */
            Tok *t = push(&L, TK_FUNC, pos, 3);
            t->op = F_SQRT;
            t->variant = FV_ROOTSIGN;
            p += 3;
            continue;
        }
        if (strncmp(p, "\xCF\x80", 2) == 0 || strncmp(p, "\xCE\xB8", 2) == 0) {   /* π θ */
            Tok *t = push(&L, TK_ATOM, pos, 2);
            t->atom = p[0] == '\xCF' ? ex_const(C_PI) : ex_var("\xCE\xB8");
            p += 2;
            continue;
        }
        int i;
        for (i = 0; i < NSYM; i++) {
            int l = (int)strlen(SYMS[i].s);
            if (strncmp(p, SYMS[i].s, (size_t)l) == 0) {
                Tok *t = push(&L, SYMS[i].k, pos, l);
                t->op = SYMS[i].op;
                p += l;
                break;
            }
        }
        if (i < NSYM) continue;
        char msg[48];
        snprintf(msg, sizeof msg, "Unknown symbol %.*s", utf8_len((unsigned char)*p), p);
        return lfail(&L, pos, msg);
    }
    *out = L.t;
    *ntok = L.n;
    return true;
}
