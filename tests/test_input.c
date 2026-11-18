#include <assert.h>
#include <stdio.h>
#include <string.h>
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

static void test_clean_name(void) {
    char b[24];
    clean_name("Peem\n", b, sizeof b);            assert(strcmp(b, "Peem") == 0);
    clean_name("  Mew, TA  \n", b, sizeof b);     assert(strcmp(b, "Mew TA") == 0);
    clean_name("a,,b", b, sizeof b);              assert(strcmp(b, "a b") == 0);
    clean_name(" , , \n", b, sizeof b);           assert(strcmp(b, "Player") == 0);
    clean_name("", b, sizeof b);                  assert(strcmp(b, "Player") == 0);
    clean_name("ABCDEFGHIJKLMNOPQRSTUVWXYZ", b, sizeof b);
    assert(strcmp(b, "ABCDEFGHIJKLMNOPQRSTUVW") == 0);   /* 23 chars */
    clean_name("ABCDEFGHIJKLMNOPQRSTUV WXYZ", b, sizeof b);
    assert(strcmp(b, "ABCDEFGHIJKLMNOPQRSTUV") == 0);    /* cut never ends in a space */
}

int main(void) {
    test_parse_ok();
    test_parse_rejects_garbage();
    test_parse_rejects_out_of_range();
    test_clean_name();

    printf("test_input: ALL PASSED\n");
    return 0;
}
