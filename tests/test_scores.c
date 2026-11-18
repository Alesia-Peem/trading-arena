#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/scores.h"

#define TMP_FILE "tests/tmp_scores.txt"

/* A score entry for tests: seed 1, date 2026-11-18. */
static ScoreEntry entry(const char *name, long worth) {
    ScoreEntry e;
    snprintf(e.name, sizeof e.name, "%s", name);
    e.net_worth = worth;
    e.seed = 1;
    snprintf(e.date, sizeof e.date, "2026-11-18");
    return e;
}

/* ---------- Reading one line ---------- */

static void test_parse_good_line(void) {
    ScoreEntry e;
    assert(scores_parse_line("Peem,1254300,42,2026-12-01\n", &e) == 1);
    assert(strcmp(e.name, "Peem") == 0);
    assert(e.net_worth == 1254300);
    assert(e.seed == 42);
    assert(strcmp(e.date, "2026-12-01") == 0);

    /* The last line of a file may have no '\n'. */
    assert(scores_parse_line("Peem,1254300,42,2026-12-01", &e) == 1);
}

static void test_parse_bad_lines(void) {
    ScoreEntry e;
    assert(scores_parse_line("", &e) == 0);
    assert(scores_parse_line("garbage line here\n", &e) == 0);
    assert(scores_parse_line("Kim,abc,1,2026-11-18\n", &e) == 0);       /* letters in money */
    assert(scores_parse_line("Kim,100,1\n", &e) == 0);                  /* missing date */
    assert(scores_parse_line("Kim,100,1,2026-11-18xyz\n", &e) == 0);    /* junk after date */
    assert(scores_parse_line("Kim,100,1,2026-1-8\n", &e) == 0);         /* short date */
    assert(scores_parse_line("ABCDEFGHIJKLMNOPQRSTUVWX,100,1,2026-11-18\n", &e) == 0);  /* 24-letter name */
}

/* ---------- Ranking ---------- */

static void test_add_keeps_order(void) {
    ScoreEntry list[MAX_SCORES];
    int n = 0;
    ScoreEntry a = entry("A", 500), b = entry("B", 900), c = entry("C", 700);

    assert(scores_add(list, &n, MAX_SCORES, &a) == 1);
    assert(scores_add(list, &n, MAX_SCORES, &b) == 1);
    assert(scores_add(list, &n, MAX_SCORES, &c) == 2);
    assert(n == 3);
    assert(list[0].net_worth == 900);
    assert(list[1].net_worth == 700);
    assert(list[2].net_worth == 500);
}

static void test_add_tie_goes_below(void) {
    ScoreEntry list[MAX_SCORES];
    int n = 0;
    ScoreEntry a = entry("A", 500), b = entry("B", 500);

    scores_add(list, &n, MAX_SCORES, &a);
    assert(scores_add(list, &n, MAX_SCORES, &b) == 2);
    assert(strcmp(list[0].name, "A") == 0);
}

static void test_add_full_board(void) {
    ScoreEntry list[MAX_SCORES];
    int n = 0;
    for (long worth = 1000; worth >= 100; worth -= 100) {   /* 10 entries: 1000 ... 100 */
        ScoreEntry e = entry("X", worth);
        scores_add(list, &n, MAX_SCORES, &e);
    }
    assert(n == 10);

    ScoreEntry low = entry("Low", 50);
    assert(scores_add(list, &n, MAX_SCORES, &low) == 0);   /* not good enough */
    assert(n == 10);
    assert(list[9].net_worth == 100);

    ScoreEntry mid = entry("Mid", 550);
    assert(scores_add(list, &n, MAX_SCORES, &mid) == 6);
    assert(n == 10);
    assert(list[5].net_worth == 550);
    assert(list[9].net_worth == 200);                      /* 100 fell off the end */
}

/* ---------- The file ---------- */

static void test_save_then_load_round_trip(void) {
    ScoreEntry out[3] = { entry("Peem", 1254300), entry("Mew TA", 1000000), entry("Bob", 449200) };
    out[1].seed = 7;
    assert(scores_save(TMP_FILE, out, 3) == 1);

    ScoreEntry in[MAX_SCORES];
    assert(scores_load(TMP_FILE, in, MAX_SCORES) == 3);
    for (int i = 0; i < 3; i++) {
        assert(strcmp(in[i].name, out[i].name) == 0);
        assert(in[i].net_worth == out[i].net_worth);
        assert(in[i].seed == out[i].seed);
        assert(strcmp(in[i].date, out[i].date) == 0);
    }
    remove(TMP_FILE);
}

static void test_load_missing_file(void) {
    ScoreEntry in[MAX_SCORES];
    assert(scores_load("tests/no_such_file.txt", in, MAX_SCORES) == 0);
}

static void test_load_skips_bad_and_caps_at_max(void) {
    FILE *f = fopen(TMP_FILE, "w");
    assert(f != NULL);
    fputs("this line is broken\n", f);
    for (int i = 0; i < 12; i++) {
        fprintf(f, "P%d,%d,1,2026-11-18%s", i, 1000 - i, i < 11 ? "\n" : "");   /* last line: no '\n' */
    }
    fclose(f);

    ScoreEntry in[20];
    assert(scores_load(TMP_FILE, in, 10) == 10);
    assert(scores_load(TMP_FILE, in, 20) == 12);
    assert(strcmp(in[11].name, "P11") == 0);
    remove(TMP_FILE);
}

static void test_load_skips_overlong_line_tail(void) {
    FILE *f = fopen(TMP_FILE, "w");
    assert(f != NULL);
    for (int i = 0; i < 130; i++) fputc('Z', f);
    fputs("Evil,999999999,1,2026-11-18\n", f);       /* one overlong line */
    fputs("Peem,1254300,42,2026-12-01\n", f);
    fclose(f);

    ScoreEntry in[MAX_SCORES];
    assert(scores_load(TMP_FILE, in, MAX_SCORES) == 1);   /* only Peem */
    assert(strcmp(in[0].name, "Peem") == 0);
    remove(TMP_FILE);
}

static void test_load_sorts_hand_edited_file(void) {
    FILE *f = fopen(TMP_FILE, "w");
    assert(f != NULL);
    for (int i = 0; i < 11; i++) {
        fprintf(f, "Low%d,%d,1,2026-11-18\n", i, 100 + i);
    }
    fputs("Best,9999999,1,2026-11-18\n", f);         /* best score on the last line */
    fclose(f);

    ScoreEntry in[MAX_SCORES];
    assert(scores_load(TMP_FILE, in, MAX_SCORES) == 10);
    assert(strcmp(in[0].name, "Best") == 0);          /* sorted to the top */
    assert(in[9].net_worth == 102);                   /* the two lowest (100, 101) dropped */
    remove(TMP_FILE);
}

static void test_save_bad_path(void) {
    ScoreEntry list[1] = { entry("A", 1) };
    assert(scores_save("tests/no_such_dir/x.txt", list, 1) == 0);
}

static void test_make_entry(void) {
    ScoreEntry e;
    scores_make_entry(&e, "Peem", 1254300, 42);
    assert(strcmp(e.name, "Peem") == 0);
    assert(e.net_worth == 1254300);
    assert(e.seed == 42);
    assert(strlen(e.date) == 10 && e.date[4] == '-' && e.date[7] == '-');   /* YYYY-MM-DD */

    scores_make_entry(&e, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 1, 1);
    assert(strlen(e.name) == 23);
}

int main(void) {
    test_parse_good_line();
    test_parse_bad_lines();
    test_add_keeps_order();
    test_add_tie_goes_below();
    test_add_full_board();
    test_save_then_load_round_trip();
    test_load_missing_file();
    test_load_skips_bad_and_caps_at_max();
    test_save_bad_path();
    test_make_entry();
    test_load_skips_overlong_line_tail();
    test_load_sorts_hand_edited_file();

    printf("test_scores: ALL PASSED\n");
    return 0;
}
