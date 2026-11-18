#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

/* Safe keyboard input: typing letters or huge numbers never crashes the game. */

/*
 * Is text `s` a whole number from `min` to `max`?
 * Spaces and the newline at the end are ignored.
 * Returns 1 and stores the number in *out if yes, returns 0 if no.
 */
int parse_long_in_range(const char *s, long min, long max, long *out);

/*
 * Show `prompt` and read a number from the keyboard. Keeps asking until the
 * player types a valid number from `min` to `max`.
 * Returns 1 on success, or 0 if input ended (EOF, e.g. Ctrl+D).
 */
int read_long(const char *prompt, long min, long max, long *out);

/*
 * Make a typed name safe to show and to save in scores.txt:
 *   - commas become spaces (a comma would break the file's format)
 *   - several spaces in a row become one, and spaces at both ends are removed
 *   - the name is cut to fit `out` (size - 1 letters)
 *   - an empty name becomes "Player"
 */
void clean_name(const char *raw, char *out, size_t size);

/*
 * Show `prompt`, read one line, and clean it with clean_name.
 * Returns 1 on success, or 0 if input ended (EOF).
 */
int read_name(const char *prompt, char *out, size_t size);

#endif
