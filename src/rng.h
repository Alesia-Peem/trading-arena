#ifndef RNG_H
#define RNG_H

/*
 * Our own random number generator.
 *
 * We don't use rand() because it gives different numbers on different computers.
 * With this one, the same seed always gives the same game everywhere.
 */

/* Start the sequence. Same seed -> same numbers. */
void rng_seed(unsigned long seed);

/* Next random number, from 0 to 32767. */
unsigned long rng_next(void);

/* Random whole number from lo to hi (both included). Returns lo if lo > hi. */
int rng_range(int lo, int hi);

#endif
