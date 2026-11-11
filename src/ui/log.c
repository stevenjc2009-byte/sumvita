#include "ui/log.h"

#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <stdarg.h>
#include <stdio.h>

#define LOG_PATH SUMVITA_DATA_DIR "/log.txt"

static int g_ready;

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
	FILE *f = fopen(LOG_PATH, "a");
	if (!f)
		return;
	unsigned long long ms = sceKernelGetProcessTimeWide() / 1000;
	fprintf(f, "[%llu] ", ms);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}
