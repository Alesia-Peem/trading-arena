#ifndef INPUT_H
#define INPUT_H

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

#endif
