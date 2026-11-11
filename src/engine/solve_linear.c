/* Linear equations and inequalities a1·x + b1  op  a2·x + b2, GCSE-style:
 * unknowns to one side, numbers to the other, divide (flip the sign for a negative). */
#include "solve_int.h"
#include <stddef.h>

static const char *mono(Rat a, const char *var)
{
    return ex_print(sv_lin_expr(a, rat_int(0), var));
}

static bool holds(int op, Rat a, Rat b)
{
    int k = rat_cmp(a, b);
    switch (op) {
    case R_EQ: return k == 0;
    case R_NE: return k != 0;
    case R_LT: return k < 0;
    case R_LE: return k <= 0;
    case R_GT: return k > 0;
    case R_GE: return k >= 0;
    }
    return false;
}

static const char *move_title(Rat a, const char *var)
{
    if (rat_is_neg(a)) return sv_fmt("Add %s to both sides", mono(rat_neg(a), var));
    return sv_fmt("Subtract %s from both sides", mono(a, var));
}

#define OK(x) do { if (!(x)) return sv_fail(c, "The numbers are too large to solve exactly"); } while (0)

bool sv_linear(SvCtx *c, int op, const Poly *PL, const Poly *PR, SvLin *res)
{
    Rat l[SV_MAXDEG + 1], r[SV_MAXDEG + 1];
    int dl, dr;
    const char *v = c->var;
    if (!sv_upoly(PL, v, l, &dl) || !sv_upoly(PR, v, r, &dr) || dl > 1 || dr > 1)
        return sv_fail(c, "This is not a linear equation");
    Rat a1 = dl >= 1 ? l[1] : rat_int(0), b1 = l[0];
    Rat a2 = dr >= 1 ? r[1] : rat_int(0), b2 = r[0];
    bool ineq = op != R_EQ && op != R_NE;
#define SHOW(t, n) sv_step_rel(c, (t), (n), op, sv_lin_expr(a1, b1, v), sv_lin_expr(a2, b2, v))

    if (rat_eq(a1, a2)) {
        if (!rat_is_zero(a2)) {
            const char *t = move_title(a2, v);
            a1 = a2 = rat_int(0);
            SHOW(t, NULL);
        }
        bool ok = holds(op, b1, b2);
        res->kind = ok ? SV_ALL : SV_NONE;
        res->op = op;
        res->val = rat_int(0);
        sv_step(c, ok ? "This is always true" : "This is never true",
                ok ? sv_fmt("Every value of %s works.", v) : sv_fmt("No value of %s works, so there is no solution.", v),
                NULL);
        return true;
    }
    if (!rat_is_zero(a1) && !rat_is_zero(a2)) {
        if (rat_cmp(a1, a2) >= 0) {
            const char *t = move_title(a2, v);
            OK(rat_sub(a1, a2, &a1));
            a2 = rat_int(0);
            SHOW(t, sv_fmt("Keep the %s terms on the side with more %s.", v, v));
        } else {
            const char *t = move_title(a1, v);
            OK(rat_sub(a2, a1, &a2));
            a1 = rat_int(0);
            SHOW(t, sv_fmt("Keep the %s terms on the side with more %s.", v, v));
        }
    }
    if (rat_is_zero(a1)) {
        Rat t = a1; a1 = a2; a2 = t;
        t = b1; b1 = b2; b2 = t;
        op = sv_flip(op);
        SHOW("Swap the sides", ineq ? "Writing it the other way round turns the inequality sign round too." : NULL);
    }
    if (!rat_is_zero(b1)) {
        const char *t = rat_is_neg(b1) ? sv_fmt("Add %s to both sides", sv_rat_str(rat_neg(b1)))
                                       : sv_fmt("Subtract %s from both sides", sv_rat_str(b1));
        OK(rat_sub(b2, b1, &b2));
        b1 = rat_int(0);
        SHOW(t, NULL);
    }
    if (!rat_is_one(a1)) {
        const char *t = sv_fmt("Divide both sides by %s", sv_rat_str(a1));
        const char *note = NULL;
        if (ineq && rat_is_neg(a1)) {
            op = sv_flip(op);
            note = "Dividing by a negative number reverses the inequality sign.";
        }
        OK(rat_div(b2, a1, &b2));
        a1 = rat_int(1);
        SHOW(t, note);
    }
#undef SHOW
    res->kind = SV_ONE;
    res->op = op;
    res->val = b2;
    return true;
}
