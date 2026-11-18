#include <assert.h>
#include <stdio.h>
#include "../src/input.h"

static void test_parse_ok(void) {
    long v;
    assert(parse_long_in_range("42\n", 1, 100, &v) && v == 42);
    assert(parse_long_in_range("  7  ", 1, 100, &v) && v == 7);
}

static void test_parse_rejects_garbage(void) {
    long v;
    assert(!parse_long_in_range("abc", 1, 100, &v));
    assert(!parse_long_in_range("", 1, 100, &v));
    assert(!parse_long_in_range("12abc", 1, 100, &v));
    assert(!parse_long_in_range("\n", 1, 100, &v));
}

static void test_parse_rejects_out_of_range(void) {
    long v;
    assert(!parse_long_in_range("0", 1, 100, &v));
    assert(!parse_long_in_range("101", 1, 100, &v));
    assert(!parse_long_in_range("99999999999999999999", 1, 100, &v));
}

int main(void) {
    test_parse_ok();
    test_parse_rejects_garbage();
    test_parse_rejects_out_of_range();

    printf("test_input: ALL PASSED\n");
    return 0;
}
