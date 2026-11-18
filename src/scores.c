#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "scores.h"
#include "display.h"

#define FILE_LINE_BUF 128   /* longest line we read from scores.txt */

void scores_make_entry(ScoreEntry *e, const char *name, long net_worth, unsigned long seed) {
    snprintf(e->name, sizeof e->name, "%s", name);
    e->net_worth = net_worth;
    e->seed = seed;

    time_t now = time(NULL);
    strftime(e->date, sizeof e->date, "%Y-%m-%d", localtime(&now));
}

int scores_parse_line(const char *line, ScoreEntry *out) {
    ScoreEntry e;
    int used = 0;   /* %n stores how many characters sscanf has read */

    /* %23[^,] = up to 23 letters that aren't a comma (fits name[24]).
       %10[0-9-] = up to 10 digits or dashes (fits date[11]). */
    int fields = sscanf(line, "%23[^,],%ld,%lu,%10[0-9-]%n",
                        e.name, &e.net_worth, &e.seed, e.date, &used);
    if (fields != 4 || used == 0 || strlen(e.date) != 10) {
        return 0;
    }

    /* Only spaces or the newline may come after the date. */
    for (const char *rest = line + used; *rest != '\0'; rest++) {
        if (!isspace((unsigned char)*rest)) {
            return 0;
        }
    }

    *out = e;
    return 1;
}

int scores_load(const char *path, ScoreEntry list[], int max) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return 0;   /* no file yet: an empty board */
    }

    char line[FILE_LINE_BUF];
    int n = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        /* No '\n' and not at the end of the file: the line was too long for
           our buffer. Throw away the rest of it and treat the line as bad,
           so its tail is never read as a line of its own. */
        if (strchr(line, '\n') == NULL && !feof(f)) {
            int c;
            while ((c = fgetc(f)) != '\n' && c != EOF) {
            }
            continue;
        }

        /* Insert in rank order, so a hand-edited file in any order still
           gives the best `max` scores, best first. */
        ScoreEntry e;
        if (scores_parse_line(line, &e)) {
            scores_add(list, &n, max, &e);
        }
    }

    fclose(f);
    return n;
}

int scores_add(ScoreEntry list[], int *n, int max, const ScoreEntry *e) {
    /* Find the first entry that is strictly worse. Equal scores stay above. */
    int place = 0;
    while (place < *n && list[place].net_worth >= e->net_worth) {
        place++;
    }
    if (place >= max) {
        return 0;   /* the board is full of better scores */
    }

    /* Move everything below `place` down one row. If the board is full,
       the last entry falls off the end. */
    int last = (*n < max) ? *n : max - 1;
    for (int i = last; i > place; i--) {
        list[i] = list[i - 1];
    }
    list[place] = *e;

    if (*n < max) {
        (*n)++;
    }
    return place + 1;
}

int scores_save(const char *path, const ScoreEntry list[], int n) {
    FILE *f = fopen(path, "w");   /* "w" empties the file, then we write it all again */
    if (f == NULL) {
        return 0;
    }

    for (int i = 0; i < n; i++) {
        fprintf(f, "%s,%ld,%lu,%s\n", list[i].name, list[i].net_worth, list[i].seed, list[i].date);
    }

    return fclose(f) == 0;   /* fclose can fail if the disk is full */
}

void scores_print(const ScoreEntry list[], int n) {
    printf("\n=== All-time top %d ===\n", MAX_SCORES);
    if (n == 0) {
        printf("No scores yet - be the first!\n");
        return;
    }

    /* The heading and the rows use the same column widths, so they line up. */
    printf("%-4s %-23s %12s  %-10s  %s\n", "RANK", "NAME", "NET WORTH", "SEED", "DATE");
    for (int i = 0; i < n; i++) {
        char money[32];
        format_money(list[i].net_worth, money, sizeof money);
        printf("%-4d %-23s %12s  %-10lu  %s\n",
               i + 1, list[i].name, money, list[i].seed, list[i].date);
    }
}
