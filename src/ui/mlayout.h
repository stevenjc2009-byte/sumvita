/* mlayout.h - natural-display maths layout.
 *
 * A layout is a flat list of draw ops positioned relative to the layout's
 * origin: x = 0 at the left edge, y = 0 on the BASELINE (negative y is up).
 * Builders work post-order on one shared op buffer: every Frag owns the
 * contiguous op range [start, start + n), so combining two fragments built one
 * after the other is just shifting the second one's range.
 *
 * Used by: expression layout (mlayout_expr.c), the input editor
 * (editor_draw.c, which adds MO_CARET marks), the cache (mlayout_cache.c). */
#ifndef UI_MLAYOUT_H
#define UI_MLAYOUT_H

#include <stdint.h>

typedef enum {
	MO_TEXT,   /* text at (x, y = baseline), px, weight, role */
	MO_RECT,   /* filled rect x, y, w = x2, h = y2 */
	MO_LINE,   /* line (x, y) -> (x2, y2), thickness = px */
	MO_SLOT,   /* empty-slot placeholder box x, y, w = x2, h = y2 */
	MO_CARET,  /* editor caret mark at (x, baseline y); x2 = seq, y2 = pos, px = asc, weight = desc */
	MO_DOT,    /* filled dot centre (x, y), radius px/10 */
} MOpKind;

enum { MR_NORMAL = 0, MR_MUTED, MR_ACCENT, MR_ERROR, MR_STRONG };

typedef struct {
	uint8_t kind, px, weight, role;
	int16_t x, y, x2, y2;
	const char *text;
} MOp;

typedef struct {
	int w, asc, desc;
	int nops;
	MOp *ops;
} MLayout;

typedef struct {
	int w, asc, desc;
	int start, n;
} Frag;

/* ---- builder (mlayout.c) ---- */
void ml_reset(void);                 /* empty the shared op buffer */
int  ml_mark(void);                  /* current op count: start of the next frag */
Frag ml_empty(void);
/* text: s is copied into the builder's string pool */
Frag ml_text(const char *s, int px, int weight, int role);
Frag ml_space(int w, int px);
/* b must have been built right after a; result covers both */
Frag ml_cat(Frag a, Frag b);
void ml_shift(Frag f, int dx, int dy);
/* stacked fraction num over den, centred on the maths axis of size px */
Frag ml_frac(Frag num, Frag den, int px);
/* exponent: base already built, exp built after it at script size */
Frag ml_sup(Frag base, Frag exp, int px);
/* subscript */
Frag ml_sub(Frag base, Frag sub, int px);
/* radical over rad; index (may be an empty frag, n == 0 and w == 0) built before rad */
Frag ml_root(Frag index, Frag rad, int px);
/* brackets around inner: l/r are "(" ")", "[" "]", "|" "|", "⌊" "⌋", "⌈" "⌉" */
Frag ml_wrap(Frag inner, const char *l, const char *r, int px, int role);
/* adds an op to the current frag end; returns the updated frag */
Frag ml_add_op(Frag f, MOp op);
/* overbar above f (recurring bar) and dots */
Frag ml_overbar(Frag f, int px);
Frag ml_dot_above(Frag f, int x, int px);
/* slot placeholder box sized for px */
Frag ml_slot(int px);
/* caret mark (editor) */
Frag ml_caret(int seq, int pos, int px);

/* Copies the frag's ops (and strings) into a malloc'd MLayout. NULL on OOM. */
MLayout *ml_finish(Frag f);
void ml_free(MLayout *l);

/* Typographic helpers. */
int ml_axis(int px);              /* maths axis height above baseline */
int ml_script_px(int px);         /* exponent / index size */
int ml_quant(int px);             /* snap to the sizes the fonts are used at */
int ml_text_asc(int px);
int ml_text_desc(int px);

/* ---- drawing (mdraw.c) ---- */
/* Draws at (x, baseline). colour for MR_NORMAL; other roles map to the theme. */
void mdraw(const MLayout *l, int x, int baseline, unsigned int color);
/* Same, with a caret drawn at the caret mark (seq, pos) when visible. */
void mdraw_caret(const MLayout *l, int x, int baseline, unsigned int color, int seq, int pos, int show);

/* ---- expression layout (mlayout_expr.c) ---- */
struct Expr;
Frag ml_expr(const struct Expr *e, int px);

/* ---- cache (mlayout_cache.c) ----
 * Parses display syntax and lays it out once; later calls hit the cache. On a
 * parse error the text is laid out plainly (never NULL). Valid until the next
 * mlc_new_frame() that decides to flush. */
const MLayout *mlc_get(const char *src, int px);
/* Largest size from px down to min_px whose width fits max_w (or min_px). */
const MLayout *mlc_fit(const char *src, int px, int min_px, int max_w);
/* Plain text layout (no parsing), cached too. */
const MLayout *mlc_text(const char *s, int px, int weight);
void mlc_new_frame(void);
void mlc_flush(void);

#endif
