/* osk - the system on-screen keyboard (sceImeDialog), used by the editor's
 * full-keyboard key to type raw display syntax. Blocking: runs its own draw
 * loop (the dialog is composited by vita2d_common_dialog_update()).
 * Copied from Noctopage src/ui/osk.h (adapted from vita-radio). MAIN THREAD ONLY. */
#ifndef UI_OSK_H
#define UI_OSK_H

#include <stddef.h>

#define OSK_MAX_TEXT 255

typedef enum { OSK_TEXT, OSK_URL, OSK_PASSWORD } osk_kind_t;

int osk_init(void);
/* Optional: draws the app behind the dialog each frame (inside start/end drawing). */
void osk_set_backdrop(void (*fn)(void));
/* Returns 1 when confirmed, 0 when cancelled (out untouched), -1 on error. */
int osk_prompt(const char *title, const char *initial, osk_kind_t kind, char *out, size_t cap);
void osk_shutdown(void);

#endif
