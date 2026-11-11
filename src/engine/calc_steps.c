/* BIDMAS working for Evaluate. The parse tree already encodes the order of
 * operations, so a post-order walk visits brackets first, then indices, then
 * × ÷, then + −. Every operation whose result looks different from what was
 * written becomes one step ("Multiply first: 4×2 = 8", "Add: 3 + 8 = 11");
 * fractions, indices, surds, % of and nCr get fuller working from calc_work.c.
 * A single plain step (3+4 = 7) is not worth showing and is dropped. */
#include <string.h>

#include "arena.h"
#include "calc_util.h"

#define MAX_WALK_STEPS 60

typedef struct {
    bool degrees;
    Steps *st;
    int start;          /* st->n before the walk */
    int plain, special;
    bool failed;
    int depth;
} W;

static const char *plain_title(const Expr *e)
{
    switch (e->kind) {
    case E_ADD: {
        int neg = 0;
        for (int i = 1; i < e->nkids; i++) neg += e->kids[i]->kind == E_NEG;
        if (neg == 0) return "Add";
        return neg == e->nkids - 1 ? "Subtract" : "Add and subtract";
    }
    case E_MUL: return "Multiply";
    case E_DIV: return "Divide";
    case E_POW: return "Indices";
    case E_NEG: return "Negative";
    case E_PCT: return "Percentage";
    case E_FACT: return "Factorial";
    case E_DEG: return "Degrees";
    case E_MIXED: return "Mixed number";
    case E_FUNC:
        switch (e->op) {
        case F_SQRT: return "Square root";
        case F_ROOT: return "Root";
        case F_SIN: return "sin";
        case F_COS: return "cos";
        case F_TAN: return "tan";
        case F_ASIN: return "sin\xE2\x81\xBB\xC2\xB9";
        case F_ACOS: return "cos\xE2\x81\xBB\xC2\xB9";
        case F_ATAN: return "tan\xE2\x81\xBB\xC2\xB9";
        case F_LN: case F_LOG: return "Logarithm";
        case F_NCR: return "Combinations";
        case F_NPR: return "Permutations";
        default: return "Work it out";
        }
    default: return "Work it out";
    }
}

static bool atom(const Expr *e)
{
    switch (e->kind) {
    case E_NUM: case E_REAL: case E_CONST: case E_RECUR: case E_VAR: case E_TEXT: case E_MIXED: return true;
    default: return false;
    }
}

static CVal walk(const Expr *e, W *w, bool top, bool under_mul)
{
    CVal r = {0};
    if (w->failed) return r;
    if (++w->depth > 200) { w->failed = true; return r; }
    if (atom(e) || e->nkids == 0) {
        if (!cu_value(e, w->degrees, &r)) w->failed = true;
        w->depth--;
        return r;
    }
    CVal *kv = amalloc(sizeof(CVal) * (size_t)e->nkids);
    Expr *calc = amalloc(sizeof(Expr)), *shown = amalloc(sizeof(Expr));
    *calc = *e;
    *shown = *e;
    calc->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
    shown->kids = amalloc(sizeof(Expr *) * (size_t)e->nkids);
    calc->flags &= ~(unsigned)EF_PAREN;
    shown->flags &= ~(unsigned)EF_PAREN;
    bool pct_of = e->kind == E_MUL && e->nkids == 2;
    for (int i = 0; i < e->nkids; i++) {
        kv[i] = walk(e->kids[i], w, false, pct_of && i == 0);
        if (w->failed) return r;
        calc->kids[i] = kv[i].e;
        shown->kids[i] = kv[i].e;
    }
    w->depth--;
    if (!cu_value(calc, w->degrees, &r)) { w->failed = true; return r; }
    const char *lhs = ex_print(shown), *rhs = ex_print(r.e);
    if (strcmp(lhs, rhs) == 0 || w->st->n - w->start >= MAX_WALK_STEPS) return r;
    int before = w->st->n;
    if (cw_node(e, kv, e->nkids, &r, under_mul, w->st)) {
        w->special += w->st->n > before;
        return r;
    }
    if (e->kind == E_PCT && under_mul) return r;     /* shown by the "% of" working */
    const char *title = plain_title(e);
    if ((e->flags & EF_PAREN) && !top) title = asprintf_a("Brackets first: %s", title);
    steps_add(w->st, title, "", "%s=%s", lhs, rhs);
    w->plain++;
    return r;
}

void calc_eval_steps(const Expr *t, bool degrees, Steps *st)
{
    if (!st || atom(t)) return;
    W w = {degrees, st, st->n, 0, 0, false, 0};
    walk(t, &w, true, false);
    if (w.failed || (w.special == 0 && w.plain <= 1)) {
        st->n = w.start;
        return;
    }
    Step *first = &st->v[w.start];
    if (w.plain + w.special >= 2 && (strcmp(first->title, "Multiply") == 0 || strcmp(first->title, "Divide") == 0 ||
                                     strcmp(first->title, "Indices") == 0))
        first->title = asprintf_a("%s first", first->title);
}
