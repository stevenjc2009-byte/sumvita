/* editor_draw.h - natural-display layout + drawing of an Editor. */
#ifndef UI_EDITOR_DRAW_H
#define UI_EDITOR_DRAW_H

#include "ui/editor.h"
#include "ui/mlayout.h"

/* Cached layout (rebuilt only when the editor's version or px changes). */
const MLayout *ed_layout(const Editor *ed, int px);
void ed_draw(const Editor *ed, const MLayout *l, int x, int baseline, unsigned int color, bool caret);
/* x offset of the caret inside the layout (for horizontal scrolling). */
int ed_caret_x(const Editor *ed, const MLayout *l);
/* Touch: put the cursor at the caret mark nearest (rx, ry), relative to the
 * layout's origin (x of its left edge, y of its baseline). */
void ed_hit(Editor *ed, const MLayout *l, int rx, int ry);

#endif
