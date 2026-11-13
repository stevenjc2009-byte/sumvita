/* settings.h - user settings, persisted to ux0:data/Sumvita/settings.txt. */
#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <stdbool.h>

#include "engine/calc.h"

typedef struct {
	bool degrees;
	int dp;            /* -1 auto, 0..9 */
	int sf;            /* 0 off, 1..10 */
	bool prefer_exact;
	int accent;
	bool seen_help;    /* the Help has been shown once (first launch) */
} UiSettings;

extern UiSettings g_set;

void settings_defaults(void);
void settings_load(void);
void settings_save(void);
/* SUMVITA_VERSION ("01.10") shown as "1.1.0"; a future "01.20" gives "1.2.0". */
const char *settings_version(void);
/* Engine settings from the UI settings plus the current Ans. */
void settings_calc(CalcSettings *cs);
void settings_set_ans(double v);

#endif
