/* tour_script.c - the line-based tour script read from tour.txt. One command
 * per line, "#" starts a comment; the commands are in tour_cmds.c. Lines run
 * one after another in the same frame until one of them yields (a shot,
 * virtual input, a wait). */
#include "ui/log.h"
#include "ui/tour_int.h"

#include <stdlib.h>
#include <string.h>

#define TS_MAX_LINES 2048
#define TS_PER_FRAME 64

static char *g_text;
static char *g_line[TS_MAX_LINES];
static int g_n, g_at;

bool ts_load(char *text)
{
	free(g_text);
	g_text = text;
	g_n = 0;
	g_at = 0;
	char *p = text;
	while (p && *p && g_n < TS_MAX_LINES) {
		char *e = strchr(p, '\n');
		if (e)
			*e = 0;
		size_t len = strlen(p);
		while (len && (p[len - 1] == '\r' || p[len - 1] == ' ' || p[len - 1] == '\t'))
			p[--len] = 0;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p && *p != '#')
			g_line[g_n++] = p;
		p = e ? e + 1 : NULL;
	}
	return g_n > 0;
}

bool ts_step(void)
{
	for (int k = 0; k < TS_PER_FRAME && g_at < g_n; k++) {
		char cmd[24];
		const char *l = g_line[g_at];
		size_t n = strcspn(l, " \t");
		if (n >= sizeof cmd)
			n = sizeof cmd - 1;
		memcpy(cmd, l, n);
		cmd[n] = 0;
		const char *arg = l + strcspn(l, " \t");
		while (*arg == ' ' || *arg == '\t')
			arg++;
		int r = tc_exec(cmd, arg);
		if (r == TC_AGAIN)
			return true;
		if (r == TC_END)
			return false;
		g_at++;
		if (r == TC_YIELD)
			return true;
	}
	return g_at < g_n;
}
