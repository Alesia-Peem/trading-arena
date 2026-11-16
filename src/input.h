#ifndef INPUT_H
#define INPUT_H

/* 1 if s (ignoring surrounding spaces and the trailing newline) is a whole number in [min,max], else 0 */
int parse_long_in_range(const char *s, long min, long max, long *out);

/* Prints prompt, reads a line with fgets, re-prompts on bad input. Returns 1 on success, 0 on EOF. */
int read_long(const char *prompt, long min, long max, long *out);

#endif
