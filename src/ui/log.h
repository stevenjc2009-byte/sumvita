/* log.h - append-only text log at ux0:data/Sumvita/log.txt.
 * Each line is opened, written and closed: Vita3K only flushes guest writes on
 * fclose. Cheap enough because the app logs rarely (boot, errors, tour). */
#ifndef UI_LOG_H
#define UI_LOG_H

#define SUMVITA_DATA_DIR "ux0:data/Sumvita"

void log_init(void);  /* creates the data dir, truncates the log */
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
