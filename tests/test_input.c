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

/* Thai letters take 3 bytes each in UTF-8. A cut must never split one. */
static void test_clean_name_keeps_utf8_whole(void) {
    char b[24];
    clean_name("\xE0\xB8\xAA\xE0\xB8\xA1\xE0\xB8\x8A\xE0\xB8\xB2\xE0\xB8\xA2"   /* 5 Thai letters, 15 bytes */
               " "
               "\xE0\xB9\x83\xE0\xB8\x88\xE0\xB8\x94\xE0\xB8\xB5", b, sizeof b);   /* + 4 more */
    /* 15 + 1 + 6 = 22 bytes fit; the 3rd letter after the space (bytes 23-25) does not. */
    assert(strlen(b) == 22);
    assert(strcmp(b, "\xE0\xB8\xAA\xE0\xB8\xA1\xE0\xB8\x8A\xE0\xB8\xB2\xE0\xB8\xA2 \xE0\xB9\x83\xE0\xB8\x88") == 0);
}

/* Names arrive from the browser URL-encoded: spaces as + or %20, Thai as %E0%B8%AA ... */
static void test_url_decode(void) {
    char b[24];
    url_decode("Mew+TA", b, sizeof b);                 assert(strcmp(b, "Mew TA") == 0);
    url_decode("Mew%2C%20TA", b, sizeof b);            assert(strcmp(b, "Mew, TA") == 0);
    url_decode("%E0%B8%AA", b, sizeof b);              assert(strcmp(b, "\xE0\xB8\xAA") == 0);
    url_decode("100%", b, sizeof b);                   assert(strcmp(b, "100%") == 0);   /* lone % kept */
    url_decode("%zz", b, sizeof b);                    assert(strcmp(b, "%zz") == 0);    /* not hex: kept */
    url_decode("ABCDEFGHIJKLMNOPQRSTUVWXYZ", b, sizeof b);
    assert(strlen(b) == 23);                                                        /* cut to fit */
}

int main(void) {
    test_parse_ok();
    test_parse_rejects_garbage();
    test_parse_rejects_out_of_range();
    test_clean_name();
    test_clean_name_keeps_utf8_whole();
    test_url_decode();

    printf("test_input: ALL PASSED\n");
    return 0;
}
