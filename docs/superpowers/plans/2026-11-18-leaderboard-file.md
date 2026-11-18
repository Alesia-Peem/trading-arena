# Leaderboard File Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ask the player's name, show an all-time top 10 read from `scores.txt`, and write the file back after each finished game.

**Architecture:** A new `scores.h/.c` module does all file reading and writing. A pure `clean_name` helper in `input.c` makes names safe for the file. `main.c` connects the two; `game.c`, `server.c` and the web files don't change.

**Tech Stack:** C99, gcc `-std=c99 -Wall -Wextra -Werror`, standard library only, `assert`-based tests run by `make test`.

**Spec:** `docs/superpowers/specs/2026-11-18-leaderboard-file-design.md`

## Global Constraints

- C99 and the standard library only; must build with `-Wall -Wextra -Werror` and no warnings.
- Money is a `long` in cents everywhere, including in the file.
- File format: one line per entry, `name,net_worth_cents,seed,YYYY-MM-DD`, best first.
- `MAX_SCORES` = 10, `SCORES_FILE` = `"scores.txt"`; names fit `char[24]` (23 characters max).
- Code is at intro-C level: arrays, structs, loops, no `malloc`. Every function gets a short comment, like the existing headers.
- The web version (`server.c`, `web/`) doesn't change.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. A name of only commas or spaces (`" , , "`) becomes `Player`, not an empty name that breaks the file → Task 1 test.
2. A line in `scores.txt` whose name is longer than 23 characters, or that has extra junk after the date, gets skipped, not cut short or misread → Task 2 test.
3. A file with more than 10 valid lines loads only 10, and a last line with no `\n` still loads → Task 2 test.
4. A new score lower than every score on a full board returns 0 and leaves the board unchanged → Task 2 test.
5. Saving to a path that can't be opened returns 0 and doesn't crash → Task 2 test.

---

### Task 1: Name cleaning and name prompt (`input` module)

**Files:**
- Modify: `src/input.h`, `src/input.c`
- Test: `tests/test_input.c`

**Interfaces:**
- Produces:
  - `void clean_name(const char *raw, char *out, size_t size);` makes a safe name: commas become spaces, runs of spaces/tabs collapse to one space, leading and trailing spaces and the newline are removed, the result is cut to `size - 1` characters, and an empty result becomes `"Player"`.
  - `int read_name(const char *prompt, char *out, size_t size);` shows the prompt, reads one line with `fgets` (skipping the rest of an overlong line with the existing `skip_rest_of_line`), and calls `clean_name`. Returns 1, or 0 if input ended.

- [ ] **Step 1: Write the failing tests** in `tests/test_input.c` and call them from `main`:

```c
static void test_clean_name(void) {
    char b[24];
    clean_name("Peem\n", b, sizeof b);            assert(strcmp(b, "Peem") == 0);
    clean_name("  Mew, TA  \n", b, sizeof b);     assert(strcmp(b, "Mew TA") == 0);
    clean_name("a,,b", b, sizeof b);              assert(strcmp(b, "a b") == 0);
    clean_name(" , , \n", b, sizeof b);           assert(strcmp(b, "Player") == 0);
    clean_name("", b, sizeof b);                  assert(strcmp(b, "Player") == 0);
    clean_name("ABCDEFGHIJKLMNOPQRSTUVWXYZ", b, sizeof b);
    assert(strcmp(b, "ABCDEFGHIJKLMNOPQRSTUVW") == 0);   /* 23 chars */
}
```
(Add `#include <string.h>` to the test file.)

- [ ] **Step 2: Check that it fails**
Run: `make tests/input_test`
Expected: compile error, `implicit declaration of function 'clean_name'`.

- [ ] **Step 3: Implement `clean_name` and `read_name`** in `src/input.c` and declare them, with comments, in `src/input.h` (add `#include <stddef.h>`). Trim any trailing space *after* cutting the name to length, so a cut never ends in a space.

- [ ] **Step 4: Check that it passes**
Run: `make test`
Expected: every suite prints `ALL PASSED`, including `test_input`.

- [ ] **Step 5: Commit**
```bash
git add src/input.h src/input.c tests/test_input.c
git commit -m "feat: safe player name input"
```

---

### Task 2: `scores` module (read, rank, write, print)

**Files:**
- Create: `src/scores.h`, `src/scores.c`, `tests/test_scores.c`

**Interfaces:**
- Consumes: `format_money(long cents, char *out, size_t n)` from `display.h`.
- Produces (in `src/scores.h`):
```c
#define MAX_SCORES  10
#define SCORES_FILE "scores.txt"

typedef struct {
    char name[24];          /* same size as Portfolio.name */
    long net_worth;         /* cents */
    unsigned long seed;
    char date[11];          /* "YYYY-MM-DD" */
} ScoreEntry;

int  scores_parse_line(const char *line, ScoreEntry *out);                 /* 1 ok, 0 bad line */
int  scores_load(const char *path, ScoreEntry list[], int max);            /* entries read; missing file = 0 */
int  scores_add(ScoreEntry list[], int *n, int max, const ScoreEntry *e);  /* place 1..max, or 0 if not in */
int  scores_save(const char *path, const ScoreEntry list[], int n);        /* 1 ok, 0 couldn't write */
void scores_print(const ScoreEntry list[], int n);
```

- [ ] **Step 1: Write the failing tests** in `tests/test_scores.c` (same style as `tests/test_input.c`, ending with `printf("test_scores: ALL PASSED\n")`). Use the temp path `"tests/tmp_scores.txt"` and `remove()` it at the end of each test that creates it. Make a helper `static ScoreEntry entry(const char *name, long worth)` (seed 1, date `"2026-11-18"`).

  - `test_parse_good_line`: `"Peem,1254300,42,2026-12-01\n"` → returns 1, name `"Peem"`, net_worth 1254300, seed 42, date `"2026-12-01"`. With no trailing `\n` → also 1.
  - `test_parse_bad_lines`: each returns 0: `""`, `"garbage line here\n"`, `"Kim,abc,1,2026-11-18\n"`, `"Kim,100,1\n"`, `"Kim,100,1,2026-11-18xyz\n"`, `"Kim,100,1,2026-1-8\n"`, and a 24-character name `"ABCDEFGHIJKLMNOPQRSTUVWX,100,1,2026-11-18\n"`.
  - `test_add_keeps_order`: add 500, 900, 700 to an empty list → places returned 1, 1, 2; final order 900, 700, 500; `n == 3`.
  - `test_add_tie_goes_below`: list `[A:500]`, add `B:500` → returns 2; `list[0]` is A.
  - `test_add_full_board`: fill 10 entries worth 1000..100 (step 100). Add 50 → returns 0, `n` stays 10, last entry still 100. Add 550 → returns 6, `n` stays 10, last entry is now 200.
  - `test_save_then_load_round_trip`: save 3 entries to the temp path → 1; load → 3, with the same fields in the same order.
  - `test_load_missing_file`: load `"tests/no_such_file.txt"` → 0.
  - `test_load_skips_bad_and_caps_at_max`: write the temp file by hand with `fopen`/`fputs`: one bad line, then 12 good lines, the last with no `\n`. Load with max 10 → 10. Load with max 20 → 12 (proves the last line without `\n` loads).
  - `test_save_bad_path`: `scores_save("tests/no_such_dir/x.txt", list, 1)` → 0.

- [ ] **Step 2: Check that it fails**
Run: `make tests/scores_test`
Expected: compile error, `scores.h: No such file or directory`.

- [ ] **Step 3: Implement `src/scores.h` and `src/scores.c`.**
  - Parse with `sscanf(line, "%23[^,],%ld,%lu,%10[0-9-]%n", ...)`: it must return 4, the date must be exactly 10 characters, and only whitespace may follow position `%n`.
  - `scores_load` opens with `"r"`, reads with `fgets` into a 128-byte buffer, keeps lines that parse, and stops at `max`.
  - `scores_add` finds the first position whose net worth is strictly lower, shifts the entries below it down by one (dropping the last if the list is full), and inserts there.
  - `scores_save` opens with `"w"`, writes with `fprintf(f, "%s,%ld,%lu,%s\n", ...)`, and returns `fclose(f) == 0`.
  - `scores_print` prints the heading `=== All-time top 10 ===`, then `No scores yet - be the first!` when empty, otherwise a `RANK NAME NET WORTH SEED DATE` table whose money column lines up under its heading (use the same widths in the heading `printf` and the row `printf`).

- [ ] **Step 4: Check that it passes**
Run: `make test`
Expected: every suite prints `ALL PASSED`, including `test_scores`. `tests/tmp_scores.txt` doesn't exist afterwards.

- [ ] **Step 5: Commit**
```bash
git add src/scores.h src/scores.c tests/test_scores.c
git commit -m "feat: leaderboard file module (scores.txt read/write)"
```

---

### Task 3: Connect it into the terminal game

**Files:**
- Modify: `src/main.c`, `.gitignore`

**Interfaces:**
- Consumes: `read_name` (Task 1); `ScoreEntry`, `scores_load`, `scores_add`, `scores_save`, `scores_print`, `MAX_SCORES`, `SCORES_FILE` (Task 2); existing `game_is_over`, `portfolio_value`.

- [ ] **Step 1: Change `main()` in `src/main.c`**
  - After the `Seed:` line: `read_name("Your name: ", ...)`. If it returns 0 (input ended), keep `"Player"`. Copy the name into `game.you.name` with `snprintf`.
  - Then load the board into a `static ScoreEntry board[MAX_SCORES]` and `scores_print` it.
  - After `show_final_standings`: if `game_is_over(&game)`, build the entry (name, `portfolio_value(&game.you, &game.market)`, `game.seed`, today's date from `time` + `localtime` + `strftime("%Y-%m-%d")`), call `scores_add` and then `scores_save`. Print `Warning: could not save scores.txt` if saving fails. Print `You placed #N on the all-time top 10!` or `Not in the top 10 this time.`, then `scores_print` the board.
  - Otherwise print `You quit early, so this game isn't saved to the leaderboard.`
  - Put the end-of-game part in its own `static void save_result(const Game *g, ScoreEntry board[], int *n)` so `main` stays short.

- [ ] **Step 2: Add `scores.txt` and `tests/tmp_scores.txt` to `.gitignore`.**

- [ ] **Step 3: Build and run the tests**
Run: `make clean && make all && make test`
Expected: both programs build with no warnings, and every suite prints `ALL PASSED`.

- [ ] **Step 4: Play-through check**
Run from the repo root, starting with no `scores.txt`:
```bash
rm -f scores.txt
{ printf 'Mew, TA\n'; for i in $(seq 30); do printf '6\n'; done; } | ./arena 7 | tail -8
cat scores.txt
printf 'Bob\n0\n' | ./arena 42 | tail -1
cat scores.txt
```
Expected: the first run prints `You placed #1 on the all-time top 10!` and a one-row table with `Mew TA` and `10,000.00`. `scores.txt` is `Mew TA,1000000,7,<today>`. The Bob run prints the quit-early message, and `scores.txt` is unchanged. Delete `scores.txt` afterwards.

- [ ] **Step 5: Commit**
```bash
git add src/main.c .gitignore
git commit -m "feat: name prompt and all-time top 10 in the terminal game"
```
