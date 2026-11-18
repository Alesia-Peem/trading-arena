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

void clean_name(const char *raw, char *out, size_t size) {
    size_t len = 0;
    int pending_space = 0;   /* saw a space we haven't written yet */

    for (const char *c = raw; *c != '\0' && len + 1 < size; c++) {
        int is_gap = (*c == ',' || isspace((unsigned char)*c));
        if (is_gap) {
            pending_space = (len > 0);   /* ignore spaces before the first letter */
            continue;
        }
        if (pending_space) {
            if (len + 2 >= size) {
                break;                   /* no room for a space plus a letter */
            }
            out[len++] = ' ';
            pending_space = 0;
        }
        out[len++] = *c;
    }
    out[len] = '\0';

    /* Thai and other non-English letters take 2 to 4 bytes in UTF-8. If the cut
       above landed in the middle of one, remove the broken letter. */
    size_t start = len;
    while (start > 0 && ((unsigned char)out[start - 1] & 0xC0) == 0x80) {
        start--;   /* step back over continuation bytes (10xxxxxx) */
    }
    if (start > 0 && ((unsigned char)out[start - 1] & 0xC0) == 0xC0) {
        unsigned char lead = (unsigned char)out[start - 1];
        size_t need = (lead >= 0xF0) ? 4 : (lead >= 0xE0) ? 3 : 2;
        if (len - (start - 1) < need) {
            len = start - 1;
            while (len > 0 && out[len - 1] == ' ') {
                len--;   /* never end on a space */
            }
            out[len] = '\0';
        }
    }

    if (len == 0) {
        snprintf(out, size, "Player");
    }
}

int read_name(const char *prompt, char *out, size_t size) {
    char line[LINE_BUF];

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(line, sizeof line, stdin) == NULL) {
        return 0;   /* input ended */
    }
    if (strchr(line, '\n') == NULL) {
        skip_rest_of_line();   /* name was longer than the buffer: drop the rest */
    }
    clean_name(line, out, size);
    return 1;
}

/* Value of one hex digit, or -1 if it isn't one. */
static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(const char *in, char *out, size_t size) {
    size_t len = 0;
    while (*in != '\0' && len + 1 < size) {
        if (*in == '+') {
            out[len++] = ' ';
            in++;
        } else if (*in == '%' && hex_value(in[1]) >= 0 && hex_value(in[2]) >= 0) {
            out[len++] = (char)(hex_value(in[1]) * 16 + hex_value(in[2]));
            in += 3;
        } else {
            out[len++] = *in;
            in++;
        }
    }
    out[len] = '\0';
}
