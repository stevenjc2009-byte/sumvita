/* mdraw.c - draws an MLayout (see mlayout.h). */
#include "ui/mlayout.h"

#include "ui/draw.h"
#include "ui/font.h"
#include "ui/theme.h"

#include <vita2d.h>

static unsigned int role_colour(int role, unsigned int base)
{
	switch (role) {
	case MR_MUTED:  return col_mix(base, COL_BG, 110);
	case MR_ACCENT: return theme_accent();
	case MR_ERROR:  return COL_ERR;
	default:        return base;
	}
}

static void draw_op(const MOp *o, int x, int y, unsigned int color)
{
	unsigned int c = role_colour(o->role, color);
	switch (o->kind) {
	case MO_TEXT:
		font_draw_base((fweight_t)o->weight, x + o->x, y + o->y, c, o->px, o->text);
		break;
	case MO_RECT:
		draw_rect(x + o->x, y + o->y, o->x2, o->y2, c);
		break;
	case MO_LINE:
		draw_line_w((float)(x + o->x), (float)(y + o->y), (float)(x + o->x2), (float)(y + o->y2),
			    (float)(o->px ? o->px : 1), c);
		break;
	case MO_SLOT:
		draw_rect(x + o->x, y + o->y, o->x2, o->y2, col_alpha(theme_accent(), 28));
		draw_dashed_rect(x + o->x, y + o->y, o->x2, o->y2, col_alpha(theme_accent(), 170));
		break;
	case MO_DOT: {
		float r = (float)o->px / 11.0f;
		if (r < 1.6f) r = 1.6f;
		draw_disc((float)(x + o->x), (float)(y + o->y), r, c);
		break;
	}
	default:
		break;
	}
}

void mdraw(const MLayout *l, int x, int baseline, unsigned int color)
{
	if (!l)
		return;
	for (int i = 0; i < l->nops; i++)
		draw_op(&l->ops[i], x, baseline, color);
}

void mdraw_caret(const MLayout *l, int x, int baseline, unsigned int color, int seq, int pos, int show)
{
	if (!l)
		return;
	for (int i = 0; i < l->nops; i++) {
		const MOp *o = &l->ops[i];
		if (o->kind == MO_CARET) {
			if (show && o->x2 == seq && o->y2 == pos) {
				int cx = x + o->x, top = baseline + o->y - o->px, h = o->px + o->weight;
				draw_rect(cx - 1, top, 2, h, theme_accent());
				draw_rect(cx - 3, top, 6, 1, theme_accent());
				draw_rect(cx - 3, top + h - 1, 6, 1, theme_accent());
			}
			continue;
		}
		draw_op(o, x, baseline, color);
	}
}
