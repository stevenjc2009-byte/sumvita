/* Expression construction and inspection. Parse/print/simplify/eval live in
 * their own files (see expr.h). */
#include "expr.h"

#include <string.h>

#include "arena.h"

static Expr *ex_alloc(ExprKind k)
{
    Expr *e = amalloc(sizeof(Expr));
    e->kind = k;
    return e;
}

Expr *ex_num(Rat r) { Expr *e = ex_alloc(E_NUM); e->r = r; return e; }
Expr *ex_int(long long n) { return ex_num(rat_int(n)); }
Expr *ex_real(double v) { Expr *e = ex_alloc(E_REAL); e->v = v; return e; }

Expr *ex_var(const char *name)
{
    Expr *e = ex_alloc(E_VAR);
    memcpy(e->name, name, strnlen(name, sizeof e->name - 1));
    return e;
}

Expr *ex_const(int c) { Expr *e = ex_alloc(E_CONST); e->op = c; return e; }
Expr *ex_text(const char *s) { Expr *e = ex_alloc(E_TEXT); e->text = astrdup(s ? s : ""); return e; }

Expr *ex_node(ExprKind k, int nkids, Expr **kids)
{
    Expr *e = ex_alloc(k);
    e->nkids = nkids;
    if (nkids > 0) {
        e->kids = amalloc(sizeof(Expr *) * (size_t)nkids);
        if (kids) memcpy(e->kids, kids, sizeof(Expr *) * (size_t)nkids);
    }
    return e;
}

Expr *ex_add2(Expr *a, Expr *b) { Expr *k[2] = {a, b}; return ex_node(E_ADD, 2, k); }
Expr *ex_sub2(Expr *a, Expr *b) { return ex_add2(a, ex_neg(b)); }
Expr *ex_mul2(Expr *a, Expr *b) { Expr *k[2] = {a, b}; return ex_node(E_MUL, 2, k); }
Expr *ex_div2(Expr *a, Expr *b) { Expr *k[2] = {a, b}; return ex_node(E_DIV, 2, k); }
Expr *ex_pow2(Expr *a, Expr *b) { Expr *k[2] = {a, b}; return ex_node(E_POW, 2, k); }
Expr *ex_neg(Expr *a) { return ex_node(E_NEG, 1, &a); }

Expr *ex_func1(int f, Expr *a)
{
    Expr *e = ex_node(E_FUNC, 1, &a);
    e->op = f;
    return e;
}

Expr *ex_func2(int f, Expr *a, Expr *b)
{
    Expr *k[2] = {a, b};
    Expr *e = ex_node(E_FUNC, 2, k);
    e->op = f;
    return e;
}

Expr *ex_rel(int op, Expr *l, Expr *r)
{
    Expr *k[2] = {l, r};
    Expr *e = ex_node(E_REL, 2, k);
    e->op = op;
    e->ops[0] = op;
    return e;
}

Expr *ex_clone(const Expr *e)
{
    if (!e) return NULL;
    Expr *c = ex_alloc(e->kind);
    *c = *e;
    if (e->text) c->text = astrdup(e->text);
    if (e->nkids > 0) {
        c->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
        for (int i = 0; i < e->nkids; i++) c->kids[i] = ex_clone(e->kids[i]);
    }
    return c;
}

bool ex_is_num(const Expr *e) { return e && e->kind == E_NUM; }

bool ex_is_int(const Expr *e, long long *v)
{
    if (!e || e->kind != E_NUM || e->r.d != 1) return false;
    if (v) *v = (long long)e->r.n;
    return true;
}

bool ex_is_zero(const Expr *e) { return e && e->kind == E_NUM && e->r.n == 0; }
bool ex_is_one(const Expr *e) { return e && e->kind == E_NUM && e->r.n == 1 && e->r.d == 1; }

static int cmp_int(long long a, long long b) { return a < b ? -1 : (a > b ? 1 : 0); }

int ex_compare(const Expr *a, const Expr *b)
{
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    if (a->kind != b->kind) return cmp_int(a->kind, b->kind);
    int c;
    switch (a->kind) {
    case E_NUM:
    case E_RECUR:
        c = rat_cmp(a->r, b->r);
        if (c) return c;
        break;
    case E_REAL:
        if (a->v != b->v) return a->v < b->v ? -1 : 1;
        break;
    case E_VAR:
        c = strcmp(a->name, b->name);
        if (c) return c < 0 ? -1 : 1;
        break;
    case E_TEXT:
        c = strcmp(a->text ? a->text : "", b->text ? b->text : "");
        if (c) return c < 0 ? -1 : 1;
        break;
    default:
        break;
    }
    if (a->op != b->op) return cmp_int(a->op, b->op);
    if (a->kind == E_REL) {
        for (int i = 0; i + 1 < a->nkids && i < EXPR_MAX_REL; i++)
            if (a->ops[i] != b->ops[i]) return cmp_int(a->ops[i], b->ops[i]);
    }
    if (a->nkids != b->nkids) return cmp_int(a->nkids, b->nkids);
    for (int i = 0; i < a->nkids; i++) {
        c = ex_compare(a->kids[i], b->kids[i]);
        if (c) return c;
    }
    return 0;
}

bool ex_equal(const Expr *a, const Expr *b) { return ex_compare(a, b) == 0; }

bool ex_list_comma(const Expr *l, int i)
{
    const Expr *a = l->kids[i - 1], *b = l->kids[i];
    if (l->flags & EF_COMMA) return (a->flags & EF_COMMA) != 0;
    return a->kind != E_TEXT && b->kind != E_TEXT;
}

bool ex_has_var(const Expr *e, const char *name)
{
    if (!e) return false;
    if (e->kind == E_VAR) return name == NULL || strcmp(e->name, name) == 0;
    for (int i = 0; i < e->nkids; i++)
        if (ex_has_var(e->kids[i], name)) return true;
    return false;
}

static void collect_vars(const Expr *e, char names[][12], int max, int *n)
{
    if (!e) return;
    if (e->kind == E_VAR) {
        for (int i = 0; i < *n; i++)
            if (strcmp(names[i], e->name) == 0) return;
        if (*n < max) {
            /* insertion sort keeps the list alphabetical */
            int j = *n;
            while (j > 0 && strcmp(names[j - 1], e->name) > 0) {
                memcpy(names[j], names[j - 1], 12);
                j--;
            }
            memset(names[j], 0, 12);
            memcpy(names[j], e->name, strnlen(e->name, 11));
            (*n)++;
        }
        return;
    }
    for (int i = 0; i < e->nkids; i++) collect_vars(e->kids[i], names, max, n);
}

int ex_vars(const Expr *e, char names[][12], int max)
{
    int n = 0;
    collect_vars(e, names, max, &n);
    return n;
}

bool ex_is_numeric(const Expr *e) { return !ex_has_var(e, NULL); }
