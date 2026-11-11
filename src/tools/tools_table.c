/* The tool table: every category's tools in menu order, plus lookup. */
#include "tools_util.h"

#define TOOLS_CAP 160

static Tool g_tools[TOOLS_CAP];
static int g_count;

typedef const Tool *(*ListFn)(int *);

static void build(void)
{
    static const ListFn lists[] = {
        tools_number, tools_algebra, tools_coord, tools_ratio, tools_geometry, tools_solid,
        tools_trig, tools_prob, tools_stats, tools_convert,
    };
    if (g_count) return;
    for (size_t i = 0; i < sizeof lists / sizeof lists[0]; i++) {
        int n;
        const Tool *t = lists[i](&n);
        for (int k = 0; k < n && g_count < TOOLS_CAP; k++) g_tools[g_count++] = t[k];
    }
}

const Tool *tools_all(int *count)
{
    build();
    if (count) *count = g_count;
    return g_tools;
}

int tools_categories(const char **out, int max)
{
    build();
    int n = 0;
    for (int i = 0; i < g_count; i++) {
        bool seen = false;
        for (int j = 0; j < n; j++) if (strcmp(out[j], g_tools[i].category) == 0) { seen = true; break; }
        if (!seen && n < max) out[n++] = g_tools[i].category;
    }
    return n;
}

const Tool *tools_find(const char *id)
{
    build();
    if (!id) return NULL;
    for (int i = 0; i < g_count; i++) if (strcmp(g_tools[i].id, id) == 0) return &g_tools[i];
    return NULL;
}
