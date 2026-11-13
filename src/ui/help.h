/* help.h - the in-app Help: pages of plain text (help_text.c) drawn by help.c. */
#ifndef UI_HELP_H
#define UI_HELP_H

#include <stdbool.h>
#include <stddef.h>

/* One page. lines ends with NULL. A line starting "# " is a bold sub-heading,
 * an empty line is a small gap, anything else is body text. */
typedef struct {
	const char *title;
	const char *const *lines;
} HelpPage;

extern const HelpPage g_help_pages[];
extern const int g_help_npages;

#endif
