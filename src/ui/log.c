#include "ui/log.h"

#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <stdarg.h>
#include <stdio.h>

#define LOG_PATH SUMVITA_DATA_DIR "/log.txt"
#define LOG_IDLE_CLOSE_MS 200 /* log_tick closes the file this long after the last line */

static int g_ready;
static FILE *g_f; /* open while lines are coming in */
static int g_sync; /* close after every line */
static unsigned long long g_last_ms;

void log_init(void)
{
	sceIoMkdir("ux0:data", 0777);
	sceIoMkdir(SUMVITA_DATA_DIR, 0777); /* fails harmlessly when it exists */
	FILE *f = fopen(LOG_PATH, "w");
	if (f) {
		fputs("Sumvita log\n", f);
		fclose(f);
		g_ready = 1;
	}
}

void log_printf(const char *fmt, ...)
{
	if (!g_ready)
		return;
	if (!g_f)
		g_f = fopen(LOG_PATH, "a");
	if (!g_f)
		return;
	unsigned long long ms = sceKernelGetProcessTimeWide() / 1000;
	fprintf(g_f, "[%llu] ", ms);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(g_f, fmt, ap);
	va_end(ap);
	fputc('\n', g_f);
	fflush(g_f);
	g_last_ms = ms;
	if (g_sync)
		log_close();
}

void log_set_sync(int on)
{
	g_sync = on;
	if (on)
		log_close();
}

void log_tick(void)
{
	if (g_f && sceKernelGetProcessTimeWide() / 1000 - g_last_ms >= LOG_IDLE_CLOSE_MS)
		log_close();
}

void log_close(void)
{
	if (g_f) {
		fclose(g_f);
		g_f = NULL;
	}
}
