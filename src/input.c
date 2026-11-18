#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"

#define LINE_BUF 64    /* longest line we read at once */

int parse_long_in_range(const char *s, long min, long max, long *out) {
    if (s == NULL) {
        return 0;
    }

    /* strtol reads a number and points `end` at the first character after it. */
    char *end;
    errno = 0;
    long value = strtol(s, &end, 10);

    if (end == s) {
        return 0;   /* no number at all, e.g. "abc" or "" */
    }
    if (errno == ERANGE) {
        return 0;   /* number too big to fit in a long */
    }

    /* Only spaces/newline may follow the number. "12abc" is rejected. */
    while (*end != '\0' && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }

    if (value < min || value > max) {
        return 0;
    }

    *out = value;
    return 1;
}

/* Throw away the rest of a line that was too long for our buffer.
   Returns 1 if the input ended (EOF) before a newline. */
static int skip_rest_of_line(void) {
    int c;
    while ((c = getchar()) != '\n') {
        if (c == EOF) {
            return 1;
        }
    }
    return 0;
}

int read_long(const char *prompt, long min, long max, long *out) {
    char line[LINE_BUF];

    for (;;) {
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(line, sizeof line, stdin) == NULL) {
            return 0;   /* input ended */
        }

        /* fgets keeps the '\n' when the whole line fit. No '\n' means either the
           line was too long, or it was the very last line with no '\n' after it. */
        int got_whole_line = (strchr(line, '\n') != NULL);
        if (!got_whole_line) {
            int buffer_was_full = (strlen(line) == sizeof line - 1);
            int hit_eof = skip_rest_of_line();
            got_whole_line = hit_eof && !buffer_was_full;
        }

        if (got_whole_line && parse_long_in_range(line, min, max, out)) {
            return 1;
        }
        printf("Please enter a number from %ld to %ld.\n", min, max);
    }
}
