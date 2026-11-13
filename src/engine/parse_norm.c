/* Input clean-up in front of the parser: what students type that is not quite
 * display syntax (docs/SYNTAX.md, "Typed forms that are accepted too"). */
#include "parse_norm.h"

#include <ctype.h>
#include <string.h>

#include "arena.h"

typedef struct { const char *from; const char *to; } Rep;
static const Rep REPS[] = {
    {"\xC2\xB2", "^2"}, {"\xC2\xB3", "^3"},                         /* ² ³ */
    {"\xE2\x80\x93", "-"}, {"\xE2\x80\x94", "-"},                   /* en dash, em dash */
    {"**", "^"},
};
static const Rep FRACS[] = {
    {"\xC2\xBD", "1/2"}, {"\xC2\xBC", "1/4"}, {"\xC2\xBE", "3/4"},  /* ½ ¼ ¾ */
};

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

/* a digit run ending just before out[n], with no '.' or '_' in front of it: a whole number */
static bool whole_number_before(const char *out, int n)
{
    if (n == 0 || !is_digit(out[n - 1])) return false;
    int i = n;
    while (i > 0 && is_digit(out[i - 1])) i--;
    return i == 0 || (out[i - 1] != '.' && out[i - 1] != '_');
}

const char *pn_normalise(const char *src, int **map)
{
    *map = NULL;
    bool any = false;
    for (const char *p = src; *p && !any; p++)
        any = (unsigned char)*p >= 0x80 || (p[0] == '*' && p[1] == '*');
    if (!any) return src;
    size_t cap = strlen(src) * 5 + 8;
    char *out = amalloc(cap);
    int *m = amalloc(sizeof(int) * cap);
    int n = 0;
    bool changed = false;
    for (const char *p = src; *p;) {
        const char *to = NULL;
        int l = 0, k;
        for (k = 0; k < (int)(sizeof REPS / sizeof REPS[0]) && !to; k++)
            if (strncmp(p, REPS[k].from, strlen(REPS[k].from)) == 0) { to = REPS[k].to; l = (int)strlen(REPS[k].from); }
        bool frac = false;
        for (k = 0; k < (int)(sizeof FRACS / sizeof FRACS[0]) && !to; k++)
            if (strncmp(p, FRACS[k].from, strlen(FRACS[k].from)) == 0) { to = FRACS[k].to; l = (int)strlen(FRACS[k].from); frac = true; }
        int at = (int)(p - src);
        if (!to) { m[n] = at; out[n++] = *p++; continue; }
        changed = true;
        const char *pre = "(", *post = ")";
        if (frac && whole_number_before(out, n)) { pre = "_"; post = ""; }   /* 2½ is 2_1/2 */
        if (!frac) pre = post = "";
        for (const char *q = pre; *q; q++) { m[n] = at; out[n++] = *q; }
        for (const char *q = to; *q; q++) { m[n] = at; out[n++] = *q; }
        for (const char *q = post; *q; q++) { m[n] = at; out[n++] = *q; }
        p += l;
    }
    if (!changed) return src;
    m[n] = (int)strlen(src);
    out[n] = 0;
    *map = m;
    return out;
}

static const struct { const char *w; CalcAction a; } CMDS[] = {
    {"factorise", ACT_FACTOR}, {"factorize", ACT_FACTOR}, {"rearrange", ACT_REARRANGE},
    {"simplify", ACT_SIMPLIFY}, {"expand", ACT_EXPAND}, {"factor", ACT_FACTOR}, {"solve", ACT_SOLVE},
};

int pn_command(const char *s, CalcAction *act)
{
    const char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    for (int i = 0; i < (int)(sizeof CMDS / sizeof CMDS[0]); i++) {
        size_t l = strlen(CMDS[i].w);
        size_t j = 0;
        while (j < l && tolower((unsigned char)p[j]) == CMDS[i].w[j]) j++;
        if (j < l || isalpha((unsigned char)p[l])) continue;
        p += l;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ':') p++;
        while (*p == ' ' || *p == '\t') p++;
        *act = CMDS[i].a;
        return (int)(p - s);
    }
    return 0;
}

void pn_unify_x(Tok *toks, int n)
{
    bool lower = false, upper = false;
    for (int i = 0; i < n; i++) {
        const Expr *a = toks[i].k == TK_ATOM ? toks[i].atom : NULL;
        if (!a || a->kind != E_VAR) continue;
        lower |= strcmp(a->name, "x") == 0;
        upper |= strcmp(a->name, "X") == 0;
    }
    if (!lower || !upper) return;
    for (int i = 0; i < n; i++)
        if (toks[i].k == TK_ATOM && toks[i].atom->kind == E_VAR && strcmp(toks[i].atom->name, "X") == 0)
            toks[i].atom->name[0] = 'x';
}
