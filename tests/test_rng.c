#include <assert.h>
#include <stdio.h>
#include "../src/rng.h"

static void test_known_values(void) {
    /* LCG: s = (s*1103515245 + 12345) & 0xFFFFFFFF; out = (s/65536) % 32768 */
    rng_seed(42);
    assert(rng_next() == 19081UL);
    assert(rng_next() == 17033UL);
    assert(rng_next() == 15269UL);
}

static void test_same_seed_same_sequence(void) {
    rng_seed(42);
    unsigned long first = rng_next();
    unsigned long second = rng_next();

    rng_seed(42);
    assert(rng_next() == first);
    assert(rng_next() == second);
}

static void test_range_stays_inside(void) {
    rng_seed(7);
    for (int i = 0; i < 10000; i++) {
        int r = rng_range(-300, 300);
        assert(r >= -300 && r <= 300);
    }
}

static void test_range_hits_both_ends(void) {
    rng_seed(1);
    int saw_low = 0;
    int saw_high = 0;
    for (int i = 0; i < 10000; i++) {
        int r = rng_range(1, 3);
        if (r == 1) saw_low = 1;
        if (r == 3) saw_high = 1;
    }
    assert(saw_low && saw_high);
}

static void test_range_single_value(void) {
    assert(rng_range(5, 5) == 5);
}

int main(void) {
    test_known_values();
    test_same_seed_same_sequence();
    test_range_stays_inside();
    test_range_hits_both_ends();
    test_range_single_value();

    printf("test_rng: ALL PASSED\n");
    return 0;
}
