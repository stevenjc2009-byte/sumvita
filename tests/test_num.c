#include "testlib.h"
#include "engine/num.h"
#include "engine/arena.h"
#include "engine/steps.h"

int main(void)
{
    char b[128];
    Rat r;

    CHECK(rat_add(RAT(1, 3), RAT(1, 6), &r) && rat_eq(r, RAT(1, 2)), "1/3+1/6");
    CHECK(rat_mul(RAT(-2, 3), RAT(9, 4), &r) && rat_eq(r, RAT(-3, 2)), "-2/3*9/4");
    CHECK(rat_div(RAT(1, 2), RAT(0, 1), &r) == false, "div by zero");
    CHECK(rat_pow_int(RAT(2, 3), -2, &r) && rat_eq(r, RAT(9, 4)), "(2/3)^-2");
    CHECK(!rat_mul(rat_int(INT64_MAX / 2), rat_int(3), &r), "overflow detected");
    CHECK(rat_root_exact(RAT(27, 8), 3, &r) && rat_eq(r, RAT(3, 2)), "cbrt 27/8");
    CHECK(!rat_root_exact(rat_int(12), 2, &r), "sqrt 12 not exact");

    CHECK(rat_from_decimal_str("3.25", &r) && rat_eq(r, RAT(13, 4)), "3.25");
    CHECK(rat_from_decimal_str("-.125", &r) && rat_eq(r, RAT(-1, 8)), "-.125");
    CHECK(rat_from_double(0.375, 1000, &r) && rat_eq(r, RAT(3, 8)), "0.375");
    CHECK(rat_from_double(1.0 / 3.0, 1000, &r) && rat_eq(r, RAT(1, 3)), "1/3 from double");

    rat_to_decimal_str(RAT(1, 8), 10, b, sizeof b); CHECK_STR(b, "0.125");
    rat_to_decimal_str(RAT(2, 3), 4, b, sizeof b); CHECK_STR(b, "0.6667");
    rat_to_decimal_str(RAT(-1, 3), 2, b, sizeof b); CHECK_STR(b, "-0.33");
    rat_to_decimal_str(RAT(-1, 1000), 2, b, sizeof b); CHECK_STR(b, "0");
    rat_to_decimal_str(RAT(199, 200), 2, b, sizeof b); CHECK_STR(b, "1");
    CHECK(rat_to_recurring_str(RAT(1, 7), b, sizeof b), "1/7 rec"); CHECK_STR(b, "0.(142857)");
    CHECK(rat_to_recurring_str(RAT(7, 55), b, sizeof b), "7/55 rec"); CHECK_STR(b, "0.1(27)");
    CHECK(rat_to_recurring_str(RAT(5, 4), b, sizeof b), "5/4 rec"); CHECK_STR(b, "1.25");
    CHECK(rat_terminates(RAT(3, 40)) && !rat_terminates(RAT(1, 6)), "terminates");

    int64_t p[8]; int e[8];
    int k = i64_factorize(360, p, e, 8);
    CHECK(k == 3 && p[0] == 2 && e[0] == 3 && p[1] == 3 && e[1] == 2 && p[2] == 5 && e[2] == 1, "360");
    int64_t l;
    CHECK(i64_lcm(84, 90, &l) && l == 1260 && i64_gcd(84, 90) == 6, "hcf/lcm 84 90");

    dbl_to_str(3.14159265358979, 10, b, sizeof b); CHECK_STR(b, "3.141592654");
    dbl_to_str(1e20, 10, b, sizeof b); CHECK_STR(b, "1E20");
    dbl_to_str(0.1 + 0.2, 10, b, sizeof b); CHECK_STR(b, "0.3");
    dbl_to_str(-2.5e-12, 10, b, sizeof b); CHECK_STR(b, "-2.5E-12");

    Steps s;
    steps_init(&s);
    for (int i = 0; i < 40; i++) steps_add(&s, "t", NULL, "x=%d", i);
    CHECK(s.n == 40 && strcmp(s.v[39].math, "x=39") == 0, "steps grow");
    return test_finish();
}
