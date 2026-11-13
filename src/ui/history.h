/* history.h - the calculation history shared by the calculator modes. */
#ifndef UI_HISTORY_H
#define UI_HISTORY_H

#include "ui/result.h"

#define HISTORY_MAX 60

typedef struct {
	Res r;
	int mode;   /* the mode tab it was made in */
	int h;      /* cached row height in the history list, 0 = not measured yet */
} HEntry;

void history_add(Res *r, int mode);     /* takes ownership of *r */
int history_count(void);
const HEntry *history_get(int i);       /* 0 = oldest, count-1 = newest */
void history_clear(void);
void history_set_h(int i, int h);        /* cache a measured row height */
unsigned history_version(void);          /* changes whenever an entry is added or removed */

#endif
