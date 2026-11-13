/* tools_search.c - GCSE tool search: the system keyboard word is matched
 * (case-insensitively, every word) against each tool's name, description,
 * topic and id, across all topics. Name matches are listed first. */
#include "ui/tools_view_int.h"

#include "ui/osk.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SEARCH_WORDS 4

static void lower(char *d, size_t cap, const char *s, bool ids)
{
	size_t n = 0;
	for (; *s && n + 1 < cap; s++, n++)
		d[n] = ids && *s == '_' ? ' ' : (char)tolower((unsigned char)*s);
	d[n] = 0;
}

static bool has_all(const char *hay, char *const *w, int nw)
{
	for (int i = 0; i < nw; i++)
		if (!strstr(hay, w[i]))
			return false;
	return true;
}

void tv_search_clear(void)
{
	g_tv.searching = false;
	g_tv.query[0] = 0;
	tv_load_tools();
}

void gcse_search(const char *text)
{
	char buf[TV_QUERY_MAX], low[TV_QUERY_MAX];
	snprintf(buf, sizeof buf, "%s", text ? text : "");
	lower(low, sizeof low, buf, false);
	char *w[SEARCH_WORDS];
	int nw = 0;
	for (char *p = strtok(low, " \t,"); p && nw < SEARCH_WORDS; p = strtok(NULL, " \t,"))
		w[nw++] = p;
	if (nw == 0) {
		tv_search_clear();
		return;
	}
	/* keep what was typed (trimmed of spaces at the ends) for the header */
	char *a = buf;
	while (*a == ' ')
		a++;
	size_t l = strlen(a);
	while (l > 0 && a[l - 1] == ' ')
		a[--l] = 0;
	snprintf(g_tv.query, sizeof g_tv.query, "%s", a);

	int n = 0;
	const Tool *all = tools_all(&n);
	g_tv.ntools = 0;
	for (int pass = 0; pass < 2; pass++)
		for (int i = 0; i < n && g_tv.ntools < TV_MAX_TOOLS; i++) {
			char nm[160], hay[512];
			snprintf(hay, sizeof hay, "%s %s", all[i].name, all[i].id);
			lower(nm, sizeof nm, hay, true);
			bool name_hit = has_all(nm, w, nw);
			snprintf(hay, sizeof hay, "%s %s %s %s", all[i].name, all[i].desc ? all[i].desc : "",
				 all[i].category, all[i].id);
			char hl[512];
			lower(hl, sizeof hl, hay, true);
			if (pass == 0 ? name_hit : (!name_hit && has_all(hl, w, nw)))
				g_tv.tools[g_tv.ntools++] = &all[i];
		}
	g_tv.searching = true;
	g_tv.page = TV_LIST;
	g_tv.on_tools = true;
	g_tv.tool_sel = 0;
	g_tv.tool_scroll = 0;
}

void tv_search_prompt(void)
{
	char out[TV_QUERY_MAX];
	if (osk_prompt("Search the tools", g_tv.query, OSK_TEXT, out, sizeof out) == 1)
		gcse_search(out);
}
