#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"

#define LINE_BUF 64

int parse_long_in_range(const char *s, long min, long max, long *out) {
    char *end;
    long v;
    if (!s) return 0;
    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || errno == ERANGE) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    if (v < min || v > max) return 0;
    *out = v;
    return 1;
}

int read_long(const char *prompt, long min, long max, long *out) {
    char buf[LINE_BUF];
    for (;;) {
        int too_long;
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) return 0;
        too_long = (strchr(buf, '\n') == NULL);
        if (too_long) {
            int c;   /* discard the rest of the line; a final line without '\n' at EOF is fine */
            int hit_eof = 0;
            while ((c = getchar()) != '\n') {
                if (c == EOF) { hit_eof = 1; break; }
            }
            if (hit_eof && strlen(buf) < sizeof buf - 1) too_long = 0;
        }
        if (!too_long && parse_long_in_range(buf, min, max, out)) return 1;
        printf("Please enter a number from %ld to %ld.\n", min, max);
    }
}
