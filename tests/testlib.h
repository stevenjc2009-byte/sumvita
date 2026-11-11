/* Tiny test helpers shared by every tests/test_*.c program. */
#ifndef TESTLIB_H
#define TESTLIB_H

#include <stdio.h>
#include <string.h>

static int t_total, t_fail;

#define CHECK(cond, ...) do { \
    t_total++; \
    if (!(cond)) { t_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

#define CHECK_STR(got, want) do { \
    const char *g_ = (got), *w_ = (want); \
    t_total++; \
    if (!g_ || strcmp(g_, w_) != 0) { t_fail++; \
        printf("FAIL %s:%d: got \"%s\" want \"%s\"\n", __FILE__, __LINE__, g_ ? g_ : "(null)", w_); } \
} while (0)

static inline int test_finish(void)
{
    printf("%s %d/%d\n", t_fail ? "FAILED" : "PASS", t_total - t_fail, t_total);
    return t_fail ? 1 : 0;
}

#endif
