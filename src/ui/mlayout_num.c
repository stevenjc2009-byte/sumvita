/* mlayout_num.c - number pieces of the expression layout: decimals with a
 * x10^n exponent, recurring decimals (dots over the repeating digits) and the
 * U+2212 minus sign. Split from mlayout_expr.c. */
#include "ui/mlayout_num.h"

#include "engine/expr.h"
#include "engine/num.h"
#include "ui/font.h"

#include <stdio.h>
#include <string.h>

#define MINUS "−"   /* U+2212 */
#define TIMES "×"       /* U+00D7 */

static Frag op_text(const char *op, int px)
{
	int sp = px / 5;
	Frag a = ml_space(sp, px);
	Frag t = ml_text(op, px, FW_REG, MR_NORMAL);
	Frag f = ml_cat(a, t);
	return ml_cat(f, ml_space(sp, px));
}

/* "-1.5" -> "−1.5" */
void mlx_fix_minus(const char *in, char *out, int cap)
{
	int o = 0;
	for (const char *p = in; *p && o < cap - 4; p++) {
		if (*p == '-') {
			memcpy(out + o, MINUS, 3);
			o += 3;
		} else {
			out[o++] = *p;
		}
	}
	out[o] = '\0';
}

Frag mlx_real(double v, int px)
{
	char buf[64], fixed[96];
	dbl_to_str(v, 10, buf, sizeof buf);
	char *E = strchr(buf, 'E');
	if (!E) {
		mlx_fix_minus(buf, fixed, sizeof fixed);
		return ml_text(fixed, px, FW_REG, MR_NORMAL);
	}
	*E = '\0';
	mlx_fix_minus(buf, fixed, sizeof fixed);
	Frag m = ml_text(fixed, px, FW_REG, MR_NORMAL);
	m = ml_cat(m, op_text(TIMES, px));
	Frag ten = ml_text("10", px, FW_REG, MR_NORMAL);
	mlx_fix_minus(E + 1, fixed, sizeof fixed);
	const char *ex = fixed[0] == '+' ? fixed + 1 : fixed;
	Frag x = ml_text(ex, ml_script_px(px), FW_REG, MR_NORMAL);
	return ml_cat(m, ml_sup(ten, x, px));
}

Frag mlx_recur(const Expr *e, int px)
{
	char pre[64] = "", rep[64] = "", fixed[96];
	const char *t = e->text ? e->text : "";
	const char *open = strchr(t, '(');
	if (!open) {
		mlx_fix_minus(t, fixed, sizeof fixed);
		return ml_text(fixed, px, FW_REG, MR_NORMAL);
	}
	snprintf(pre, sizeof pre, "%.*s", (int)(open - t), t);
	const char *close = strchr(open, ')');
	snprintf(rep, sizeof rep, "%.*s", (int)(close ? close - open - 1 : (long)strlen(open + 1)), open + 1);
	mlx_fix_minus(pre, fixed, sizeof fixed);
	Frag a = ml_text(fixed, px, FW_REG, MR_NORMAL);
	Frag b = ml_text(rep, px, FW_REG, MR_NORMAL);
	Frag f = ml_cat(a, b);
	int n = (int)strlen(rep);
	if (n > 0) {
		char one[2] = { rep[0], 0 };
		f = ml_dot_above(f, a.w + font_width(px, one) / 2, px);
		if (n > 1) {
			one[0] = rep[n - 1];
			char most[64];
			snprintf(most, sizeof most, "%.*s", n - 1, rep);
			f = ml_dot_above(f, a.w + font_width(px, most) + font_width(px, one) / 2, px);
		}
	}
	return f;
}
