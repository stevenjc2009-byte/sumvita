/* calc_run must never crash: 5000 random strings from the display-syntax
 * alphabet through every action (ASan / UBSan build), arena reset between runs. */
#include <stdlib.h>

#include "testlib.h"
#include "engine/arena.h"
#include "engine/calc.h"

static const char *TOK[] = {
    "0", "1", "2", "3", "4", "5", "7", "9", "10", "12", "0.5", "0.375", ".", "+", "-", "\xC3\x97", "\xC3\xB7",
    "*", "/", "^", "(", ")", "[", "]", "|", "!", "%", "\xC2\xB0", ",", "=", "<", ">", "\xE2\x89\xA4", "\xE2\x89\xA5",
    "\xE2\x89\xA0", "\xC2\xB1", "\xE2\x88\x9A", "\xCF\x80", "x", "y", "a", "e", "sin", "cos", "tan",
    "\xE2\x81\xBB\xC2\xB9", "ln", "log", "sqrt", "root", "cbrt", "abs", "nCr", "nPr", "exp", "recur(", "Ans", "of",
    "_", " ", "1_3/4", "recur(0.3(18))", "10^", "\xC3\x97" "10^", "99999999999", "1e", "\"", "\"text\"", "2x",
    "(x+1)", "x^2", "^-", "--", "((", "))", "0^0", "/0", "\xE2\x88\x9A(-", "1/", "360", "90", "1000000",
};
#define NTOK ((int)(sizeof TOK / sizeof TOK[0]))

static const char *OPND[] = {
    "0", "1", "2", "3", "7", "12", "360", "0.375", "1/3", "3/4", "1_3/4", "recur(0.3(18))", "35%", "\xE2\x88\x9A" "8",
    "\xE2\x88\x9A(2+\xE2\x88\x9A" "3)", "\xCF\x80", "e", "x", "2x", "(x+1)", "y", "sin(30)", "cos(x)", "tan(90)",
    "sin\xE2\x81\xBB\xC2\xB9(1/2)", "ln(0)", "log(100)", "5!", "nCr(5,2)", "Ans", "|-4|", "(2+\xE2\x88\x9A" "3)",
    "10^-4", "4.2\xC3\x97" "10^5", "x^2", "99999999999", "0.0001", "-5", "(1/0)", "root(3,8)",
};
#define NOPND ((int)(sizeof OPND / sizeof OPND[0]))
static const char *OPR[] = {
    "+", "-", "\xC3\x97", "\xC3\xB7", "/", "^", "=", "<", "\xE2\x89\xA5", ",", " of ", "^-", "\xC2\xB1",
};
#define NOPR ((int)(sizeof OPR / sizeof OPR[0]))

static unsigned long long rng = 0x9E3779B97F4A7C15ull;
static unsigned rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (unsigned)(rng >> 16);
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 5000;
    CalcSettings s;
    CalcResult r;
    long runs = 0, ok = 0;
    char buf[512];
    for (int i = 0; i < n; i++) {
        int len = 1 + (int)(rnd() % 14);
        size_t o = 0;
        buf[0] = 0;
        bool shaped = i % 2 == 1;       /* odd inputs: operand, operator, operand ... so most parse */
        if (shaped && len % 2 == 0) len++;
        for (int k = 0; k < len; k++) {
            const char *t = !shaped ? TOK[rnd() % NTOK] : (k % 2 == 0 ? OPND[rnd() % NOPND] : OPR[rnd() % NOPR]);
            size_t l = strlen(t);
            if (o + l + 1 >= sizeof buf) break;
            memcpy(buf + o, t, l + 1);
            o += l;
        }
        if (rnd() % 50 == 0 && o > 2) buf[rnd() % o] = (char)(0x80 | (rnd() & 0x3F));   /* broken UTF-8 */
        calc_settings_default(&s);
        if (rnd() % 4 == 0) s.degrees = false;
        if (rnd() % 5 == 0) s.dp = (int)(rnd() % 12);
        if (rnd() % 5 == 0) s.sf = 1 + (int)(rnd() % 12);
        if (rnd() % 3 == 0) { s.ans = (double)(int)(rnd() % 2001 - 1000) / 8.0; s.ans_set = true; }
        if (rnd() % 3 == 0) snprintf(s.target_var, sizeof s.target_var, "x");
        s.prefer_exact = rnd() % 4 != 0;
        for (int a = 0; a < ACT_COUNT; a++) {
            calc_run((CalcAction)a, buf, &s, &r);
            runs++;
            ok += r.ok;
            if (r.ok && (!r.answer || !r.answer_alt || !r.input_echo)) {
                printf("FAIL NULL text: action %d input \"%s\"\n", a, buf);
                t_fail++;
            }
            if (!r.ok && !r.error[0]) {
                printf("FAIL empty error: action %d input \"%s\"\n", a, buf);
                t_fail++;
            }
            arena_reset(arena_current());
        }
        t_total++;
    }
    printf("fuzz: %d inputs, %ld calc_run calls, %ld ok, 0 crashes\n", n, runs, ok);
    return test_finish();
}
