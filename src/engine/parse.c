/* ex_parse: DISPLAY SYNTAX (docs/SYNTAX.md) -> display-faithful Expr tree.
 * Recursive descent over the tokens from parse_lex.c, lowest precedence first:
 * list , text  ->  relation chain  ->  + - ±  ->  × ÷ implicit  ->  unary - ±
 *   ->  ^ (right assoc)  ->  postfix ! % °  ->  atoms, ( ), | |, functions. */
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "expr.h"
#include "parse_int.h"

#define MAX_DEPTH 200

typedef struct {
    const char *src;
    Tok *t;
    int i;
    char *err;
    int errcap;
    int *err_pos;
    bool failed;
    int absdepth;   /* open |...| at this bracket level: a '|' then closes */
    int depth;
} P;

typedef struct { Expr **v; int n, cap; } Vec;

static void vpush(Vec *v, Expr *e)
{
    if (v->n == v->cap) {
        int nc = v->cap ? v->cap * 2 : 8;
        Expr **nv = amalloc(sizeof(Expr *) * (size_t)nc);
        if (v->n) memcpy(nv, v->v, sizeof(Expr *) * (size_t)v->n);
        v->v = nv;
        v->cap = nc;
    }
    v->v[v->n++] = e;
}

static Expr *collapse(Vec *v, ExprKind k) { return v->n == 1 ? v->v[0] : ex_node(k, v->n, v->v); }

static Tok *cur(P *p) { return &p->t[p->i]; }
static void advance(P *p) { if (p->t[p->i].k != TK_END) p->i++; }

static Expr *fail_msg(P *p, const Tok *t, const char *msg)
{
    if (!p->failed) {
        p->failed = true;
        if (p->err && p->errcap > 0) snprintf(p->err, (size_t)p->errcap, "%s", msg);
        if (p->err_pos) *p->err_pos = t->pos;
    }
    return NULL;
}

static Expr *fail_unexpected(P *p, const Tok *t)
{
    char msg[64];
    if (t->k == TK_END) return fail_msg(p, t, "Unexpected end");
    snprintf(msg, sizeof msg, "Unexpected %.*s", t->len > 20 ? 20 : t->len, p->src + t->pos);
    return fail_msg(p, t, msg);
}

static Expr *parse_list(P *p);
static Expr *parse_rel(P *p);
static Expr *parse_add(P *p);
static Expr *parse_unary(P *p);
static Expr *parse_power(P *p);
static Expr *parse_primary(P *p);

/* Can this token start a factor of an implicit product? */
static bool starts_atom(P *p, const Tok *t)
{
    switch (t->k) {
    case TK_ATOM: case TK_FUNC: case TK_LP: return true;
    case TK_BAR: return p->absdepth == 0;
    default: return false;
    }
}

static Expr *parse_list(P *p)
{
    Vec v = {0};
    for (;;) {
        Tok *t = cur(p);
        if (t->k == TK_TEXT && p->t[p->i + 1].k != TK_REL) {
            Expr *tx = ex_text(t->text);
            vpush(&v, tx);
            advance(p);
            Tok *n = cur(p);
            if (n->k == TK_COMMA) { tx->flags |= EF_COMMA; advance(p); continue; }
            if (n->k == TK_END || n->k == TK_RP || n->k == TK_BAR) break;
            continue;
        }
        Expr *e = parse_rel(p);
        if (!e) return NULL;
        vpush(&v, e);
        t = cur(p);
        if (t->k == TK_COMMA) { e->flags |= EF_COMMA; advance(p); continue; }
        if (t->k == TK_TEXT) continue;
        break;
    }
    Expr *l = collapse(&v, E_LIST);
    if (v.n > 1) l->flags |= EF_COMMA;      /* separators as typed */
    return l;
}

/* One side of a relation: maths, or a quoted text ("HCF"=6, x="none"). A text
 * opens a relation only when a relation sign follows it. */
static Expr *rel_side(P *p, bool first)
{
    Tok *t = cur(p);
    if (t->k == TK_TEXT && (!first || p->t[p->i + 1].k == TK_REL)) {
        advance(p);
        return ex_text(t->text);
    }
    return parse_add(p);
}

static Expr *parse_rel(P *p)
{
    Expr *first = rel_side(p, true);
    if (!first) return NULL;
    if (cur(p)->k != TK_REL) return first;
    Expr *kids[EXPR_MAX_REL + 1];
    int ops[EXPR_MAX_REL] = {0};
    int nk = 1;
    kids[0] = first;
    while (cur(p)->k == TK_REL) {
        if (nk - 1 >= EXPR_MAX_REL) return fail_msg(p, cur(p), "Too many relations");
        ops[nk - 1] = cur(p)->op;
        advance(p);
        Expr *r = rel_side(p, false);
        if (!r) return NULL;
        kids[nk++] = r;
    }
    Expr *e = ex_node(E_REL, nk, kids);
    e->op = ops[0];
    memcpy(e->ops, ops, sizeof ops);
    return e;
}

static Expr *parse_term(P *p);

static Expr *parse_add(P *p)
{
    Vec v = {0};
    Expr *t = parse_term(p);
    if (!t) return NULL;
    vpush(&v, t);
    for (;;) {
        TokKind k = cur(p)->k;
        if (k != TK_PLUS && k != TK_MINUS && k != TK_PM) break;
        advance(p);
        Expr *r = parse_term(p);
        if (!r) return NULL;
        if (k == TK_PLUS) {
            vpush(&v, r);
        } else if (k == TK_MINUS) {
            vpush(&v, ex_neg(r));
        } else {
            Expr *kk[2] = {collapse(&v, E_ADD), r};
            v.n = 0;
            vpush(&v, ex_node(E_PM, 2, kk));
        }
    }
    return collapse(&v, E_ADD);
}

static Expr *parse_term(P *p)
{
    Vec v = {0};
    Expr *f = parse_unary(p);
    if (!f) return NULL;
    vpush(&v, f);
    for (;;) {
        Tok *o = cur(p);
        if (o->k == TK_TIMES) {
            advance(p);
            if (!(f = parse_unary(p))) return NULL;
            vpush(&v, f);
        } else if (o->k == TK_DIV) {
            advance(p);
            if (!(f = parse_unary(p))) return NULL;
            Expr *num = collapse(&v, E_MUL);
            v.n = 0;
            vpush(&v, ex_div2(num, f));
        } else if (starts_atom(p, o)) {
            if (!(f = parse_power(p))) return NULL;
            vpush(&v, f);
        } else {
            break;
        }
    }
    return collapse(&v, E_MUL);
}

static Expr *parse_unary(P *p)
{
    if (++p->depth > MAX_DEPTH) return fail_msg(p, cur(p), "Too complicated");
    Expr *r;
    TokKind k = cur(p)->k;
    if (k == TK_MINUS || k == TK_PLUS || k == TK_PM) {
        advance(p);
        Expr *a = parse_unary(p);
        if (!a) return NULL;
        if (k == TK_PLUS) {
            r = a;
        } else if (k == TK_PM) {
            r = ex_node(E_PM, 1, &a);
        } else if (a->kind == E_NUM && !(a->flags & EF_PAREN) && !rat_is_neg(a->r)) {
            a->r = rat_neg(a->r);       /* "-3" is the number -3 */
            r = a;
        } else if (a->kind == E_REAL && !(a->flags & EF_PAREN) && a->v >= 0) {
            a->v = -a->v;
            r = a;
        } else {
            r = ex_neg(a);
        }
    } else {
        r = parse_power(p);
    }
    p->depth--;
    return r;
}

static Expr *parse_power(P *p)
{
    Expr *b = parse_primary(p);
    if (!b) return NULL;
    for (;;) {
        TokKind k = cur(p)->k;
        if (k != TK_BANG && k != TK_PCT && k != TK_DEG) break;
        advance(p);
        b = ex_node(k == TK_BANG ? E_FACT : (k == TK_PCT ? E_PCT : E_DEG), 1, &b);
    }
    if (cur(p)->k != TK_POW) return b;
    advance(p);
    Expr *x = parse_unary(p);
    if (!x) return NULL;
    return ex_pow2(b, x);
}

/* Bracketed contents after an opening bracket token (already current).
 * args != NULL: comma-separated function arguments; else one list/expression. */
static bool parse_bracketed(P *p, Vec *args, Expr **out)
{
    bool as_args = args != NULL;
    Tok *open = cur(p);
    advance(p);
    if (cur(p)->k == TK_RP) { fail_msg(p, cur(p), "Empty brackets"); return false; }
    int saved = p->absdepth;
    p->absdepth = 0;
    Expr *in = NULL;
    if (as_args) {
        for (;;) {
            Expr *a = parse_rel(p);
            if (!a) return false;
            vpush(args, a);
            if (cur(p)->k != TK_COMMA) break;
            advance(p);
        }
    } else {
        in = parse_list(p);
        if (!in) return false;
    }
    p->absdepth = saved;
    Tok *c = cur(p);
    if (c->k != TK_RP) {
        if (c->k == TK_END) fail_msg(p, c, "Missing )"); else fail_unexpected(p, c);
        return false;
    }
    if ((open->op == '(') != (c->op == ')')) { fail_msg(p, c, "Brackets don't match"); return false; }
    advance(p);
    if (out) *out = in;
    return true;
}

static Expr *parse_func(P *p)
{
    Tok *ft = cur(p);
    int fid = ft->op, var = ft->variant;
    advance(p);
    Vec args = {0};
    if (cur(p)->k == TK_LP) {
        if (!parse_bracketed(p, &args, NULL)) return NULL;
    } else if (var == FV_ROOTSIGN) {
        Expr *a = parse_primary(p);                 /* √ takes the next atom only */
        if (!a) return NULL;
        vpush(&args, a);
    } else {
        if ((fid == F_ROOT && var != FV_CBRT) || fid == F_NCR || fid == F_NPR) {
            char msg[48];
            snprintf(msg, sizeof msg, "%.*s needs brackets", ft->len, p->src + ft->pos);
            return fail_msg(p, cur(p), msg);
        }
        Vec v = {0};
        Expr *a = parse_unary(p);
        if (!a) return NULL;
        vpush(&v, a);
        while (starts_atom(p, cur(p)) && cur(p)->k != TK_FUNC) {
            if (!(a = parse_power(p))) return NULL;
            vpush(&v, a);
        }
        vpush(&args, collapse(&v, E_MUL));
    }
    int lo = 1, hi = 1;
    if (fid == F_LOG) hi = 2;
    if ((fid == F_ROOT && var != FV_CBRT) || fid == F_NCR || fid == F_NPR) lo = hi = 2;
    if (args.n < lo || args.n > hi) {
        char msg[64];
        snprintf(msg, sizeof msg, "Wrong number of values for %.*s", ft->len, p->src + ft->pos);
        return fail_msg(p, ft, msg);
    }
    if (var == FV_CBRT) return ex_func2(F_ROOT, ex_int(3), args.v[0]);
    Expr *e = ex_node(E_FUNC, args.n, args.v);
    e->op = fid;
    return e;
}

static Expr *parse_primary(P *p)
{
    if (++p->depth > MAX_DEPTH) return fail_msg(p, cur(p), "Too complicated");
    Tok *t = cur(p);
    Expr *r = NULL;
    switch (t->k) {
    case TK_ATOM:
        advance(p);
        r = t->atom;
        break;
    case TK_LP:
        if (!parse_bracketed(p, NULL, &r)) return NULL;
        r->flags |= EF_PAREN;
        break;
    case TK_BAR: {
        advance(p);
        p->absdepth++;
        Expr *in = parse_add(p);
        p->absdepth--;
        if (!in) return NULL;
        if (cur(p)->k != TK_BAR) return fail_msg(p, cur(p), "Missing |");
        advance(p);
        r = ex_func1(F_ABS, in);
        break;
    }
    case TK_FUNC:
        r = parse_func(p);
        break;
    default:
        return fail_unexpected(p, t);
    }
    p->depth--;
    return r;
}

Expr *ex_parse(const char *src, char *err, int errcap, int *err_pos)
{
    if (err && errcap > 0) err[0] = 0;
    if (err_pos) *err_pos = -1;
    if (!src) src = "";
    Tok *toks;
    int n;
    if (!lex_all(src, &toks, &n, err, errcap, err_pos)) return NULL;
    P p = {src, toks, 0, err, errcap, err_pos, false, 0, 0};
    if (n == 1) return fail_msg(&p, &toks[0], "Empty input");
    Expr *e = parse_list(&p);
    if (e && cur(&p)->k != TK_END) fail_unexpected(&p, cur(&p));
    return p.failed ? NULL : e;
}
