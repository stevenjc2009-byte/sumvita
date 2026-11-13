/* settings.c - key=value settings file. */
#include "ui/settings.h"

#include "ui/log.h"
#include "ui/theme.h"

#include <psp2/io/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_PATH SUMVITA_DATA_DIR "/settings.txt"

UiSettings g_set;
static double g_ans;
static bool g_ans_set;

void settings_defaults(void)
{
	g_set.degrees = true;
	g_set.dp = -1;
	g_set.sf = 0;
	g_set.prefer_exact = true;
	g_set.accent = 0;
	g_set.seen_help = false;
	theme_set_accent(0);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void settings_load(void)
{
	settings_defaults();
	FILE *f = fopen(SETTINGS_PATH, "r");
	if (!f)
		return;
	char line[96];
	while (fgets(line, sizeof line, f)) {
		char *eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		int v = atoi(eq + 1);
		if (!strcmp(line, "degrees")) g_set.degrees = v != 0;
		else if (!strcmp(line, "dp")) g_set.dp = clampi(v, -1, 9);
		else if (!strcmp(line, "sf")) g_set.sf = clampi(v, 0, 10);
		else if (!strcmp(line, "exact")) g_set.prefer_exact = v != 0;
		else if (!strcmp(line, "seen_help")) g_set.seen_help = v != 0;
		else if (!strcmp(line, "accent")) g_set.accent = clampi(v, 0, theme_accent_count() - 1);
	}
	fclose(f);
	theme_set_accent(g_set.accent);
	log_printf("settings loaded: deg=%d dp=%d sf=%d exact=%d accent=%d", g_set.degrees, g_set.dp, g_set.sf,
		   g_set.prefer_exact, g_set.accent);
}

void settings_save(void)
{
	sceIoMkdir("ux0:data", 0777);
	sceIoMkdir(SUMVITA_DATA_DIR, 0777);
	FILE *f = fopen(SETTINGS_PATH, "w");
	if (!f) {
		log_printf("settings: cannot write %s", SETTINGS_PATH);
		return;
	}
	fprintf(f, "degrees=%d\ndp=%d\nsf=%d\nexact=%d\naccent=%d\nseen_help=%d\n", g_set.degrees ? 1 : 0, g_set.dp,
		g_set.sf, g_set.prefer_exact ? 1 : 0, g_set.accent, g_set.seen_help ? 1 : 0);
	fclose(f);
}

void settings_calc(CalcSettings *cs)
{
	calc_settings_default(cs);
	cs->degrees = g_set.degrees;
	cs->dp = g_set.dp;
	cs->sf = g_set.sf;
	cs->prefer_exact = g_set.prefer_exact;
	cs->ans = g_ans;
	cs->ans_set = g_ans_set;
}

/* "01.10" -> "1.1.0": major.minor from the two numbers, patch 0 */
const char *settings_version(void)
{
	static char buf[16];
	if (!buf[0]) {
		/* APP_VER "01.10" is v1.1.0: the two digits after the dot are minor and patch */
		int major = 0, minor = 0, patch = 0;
		if (sscanf(SUMVITA_VERSION, "%d.%1d%1d", &major, &minor, &patch) == 3)
			snprintf(buf, sizeof buf, "%d.%d.%d", major, minor, patch);
		else
			snprintf(buf, sizeof buf, "%s", SUMVITA_VERSION);
	}
	return buf;
}

void settings_set_ans(double v)
{
	g_ans = v;
	g_ans_set = true;
}
