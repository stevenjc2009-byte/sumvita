/* log.h - append-only text log at ux0:data/Sumvita/log.txt.
 * The file stays open while lines come in; each line is fflush()ed at once
 * (enough on a Vita). Vita3K holds guest writes in a host buffer until fclose,
 * so log_tick() (once a frame) closes the file 200 ms after the last line: the
 * emulator harness, which waits for lines like "TOUR shot NAME", sees every
 * line within a fraction of a second, and a burst of lines costs one open.
 * During a tour (log_set_sync(1)) every line is closed at once, as before,
 * because the tour logs "TOUR shot osk" from osk.c's own loop, where
 * log_tick() does not run. */
#ifndef UI_LOG_H
#define UI_LOG_H

#define SUMVITA_DATA_DIR "ux0:data/Sumvita"

void log_init(void);  /* creates the data dir, truncates the log */
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_tick(void);  /* main loop, every frame: closes the file once lines stop */
void log_close(void); /* at exit, and from log_tick */
void log_set_sync(int on); /* 1: close the file after every line */

#endif
