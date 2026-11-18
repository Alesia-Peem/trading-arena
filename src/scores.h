#ifndef SCORES_H
#define SCORES_H

/*
 * The all-time leaderboard, saved in a text file so it survives between games.
 *
 * scores.txt has one result per line, best first:
 *     name,net_worth_in_cents,seed,YYYY-MM-DD
 *     Peem,1254300,42,2026-12-01
 */

#define MAX_SCORES  10              /* the board keeps the best 10 */
#define SCORES_FILE "scores.txt"    /* saved in the folder you run the game from */

typedef struct {
    char name[24];          /* same size as Portfolio.name */
    long net_worth;         /* in cents, like all money in the game */
    unsigned long seed;     /* so friends can replay the same game */
    char date[11];          /* "YYYY-MM-DD" */
} ScoreEntry;

/* Read one line of scores.txt into *out. Returns 1 if the line is valid, 0 if not. */
int scores_parse_line(const char *line, ScoreEntry *out);

/*
 * Read the file at `path` into list[], keeping the best `max` entries, best first.
 * Broken or too-long lines are skipped. Returns how many were kept (0 if the file doesn't exist).
 */
int scores_load(const char *path, ScoreEntry list[], int max);

/*
 * Put *e into list[] (n entries, sorted best first), keeping at most `max`.
 * On a tie the older entry stays above. Updates *n.
 * Returns the place it got (1 = top), or 0 if it didn't make the board.
 */
int scores_add(ScoreEntry list[], int *n, int max, const ScoreEntry *e);

/* Write over the file at `path` with the n entries. Returns 1 if it worked, 0 if not. */
int scores_save(const char *path, const ScoreEntry list[], int n);

/* Show the board on screen as a table. */
void scores_print(const ScoreEntry list[], int n);

#endif
