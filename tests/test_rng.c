#include <assert.h>
#include <stdio.h>
#include "../src/rng.h"

static void test_same_seed_same_sequence(void) {
    rng_seed(42); unsigned long a1 = rng_next(), a2 = rng_next();
    rng_seed(42); assert(rng_next() == a1); assert(rng_next() == a2);
}
static void test_range_stays_inside(void) {
    rng_seed(7);
    for (int i = 0; i < 10000; i++) { int r = rng_range(-300, 300); assert(r >= -300 && r <= 300); }
}
static void test_range_hits_both_ends(void) {
    rng_seed(1); int lo = 0, hi = 0;
    for (int i = 0; i < 10000; i++) { int r = rng_range(1, 3); if (r == 1) lo = 1; if (r == 3) hi = 1; }
    assert(lo && hi);
}
static void test_range_single_value(void) { assert(rng_range(5, 5) == 5); }

int main(void) {
    test_same_seed_same_sequence(); test_range_stays_inside();
    test_range_hits_both_ends(); test_range_single_value();
    printf("test_rng: ALL PASSED\n"); return 0;
}
