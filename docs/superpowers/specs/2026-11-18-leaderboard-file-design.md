# Leaderboard File — Design

Short design for a small change to Stock Trading Arena (CPE101).

## Why

The course now requires a C function that reads and writes a file (TA announcement, 14/11/2569 BE). The proposal promises a `scores.h` / `scores.c` module that keeps an all-time top 10 in `scores.txt`. Claude writes the code; the team owner explains it to the team, so the code must stay at intro-C level and read clearly line by line.

## Goals

- At start: ask the player's name, then show the all-time top 10 read from `scores.txt`.
- At the end of a finished game: add the player's result, keep the best 10, and write the whole file back.
- Tell the player their place ("You placed #3 on the all-time top 10!") or that they missed it.
- Unit tests for the new module, in the same style as the existing `tests/test_*.c`.

## Non-goals

- The web version (`server.c`, `web/`) — unchanged in this change.
- Saving bots' results, saving a game halfway, user accounts.

## Anti-goals

- A missing, empty or hand-edited `scores.txt` crashes the game or loses the current result.
- A name with a comma breaks the file format.
- Money in the file gets rounding errors (it stays whole cents in a `long`).

## Approaches considered

- **A. Text file, rewritten each game — chosen.** `fgets` + `sscanf` to read, `fopen "w"` + `fprintf` to write. Readable in any editor during the demo; matches "read from and write over a file".
- B. Binary `fread`/`fwrite` of the array — shortest, but unreadable and breaks if the struct changes.
- C. Append-only log (`fopen "a"`) — never loses history, but the file grows forever and must be sorted on every display.

## Design

**File format** — `scores.txt` in the folder the game is run from, one entry per line, best first:

```
name,net_worth_cents,seed,YYYY-MM-DD
Peem,1254300,42,2026-12-01
```

**Module `src/scores.h` / `src/scores.c`** (new, owned by Member 4):

```c
#define MAX_SCORES  10
#define SCORES_FILE "scores.txt"

typedef struct {
    char name[24];          /* same size as Portfolio.name */
    long net_worth;         /* cents */
    unsigned long seed;
    char date[11];          /* "YYYY-MM-DD" */
} ScoreEntry;

int  scores_parse_line(const char *line, ScoreEntry *out);              /* 1 ok, 0 bad line */
int  scores_load(const char *path, ScoreEntry list[], int max);         /* entries read; missing file = 0 */
int  scores_add(ScoreEntry list[], int *n, int max, const ScoreEntry *e); /* place 1..max, or 0 if not in */
int  scores_save(const char *path, const ScoreEntry list[], int n);     /* 1 ok, 0 couldn't write */
void scores_print(const ScoreEntry list[], int n);
```

- Bad lines (wrong field count, letters in numbers, name too long) are skipped, not fatal.
- `scores_add` keeps the list sorted, highest net worth first. On a tie the older entry stays above the new one.
- `scores_print` uses `format_money` from `display.h`, so `scores` depends on `display`.

**Name input** — new `read_name(prompt, out, size)` in `input.c` (Member 3's module): trims spaces, replaces commas with spaces, cuts to 23 characters, and uses `Player` when left empty. Returns 0 on end of input, like `read_long`.

**main.c changes** — read the name before the first day and put it in `game.you.name`; load and print the top 10; after the final standings, if the game reached day 30, add the result (today's date from `time` + `strftime`), save, and print the place. A player who quits early is not recorded, so a lucky early spike can't be locked in. If saving fails, print a warning and end normally.

**Other files** — add `scores.txt` to `.gitignore`. The Makefile already picks up every `src/*.c` and `tests/test_*.c`, so it needs no change.

## Left to the builder

Exact on-screen wording and the column layout of the top-10 table.
