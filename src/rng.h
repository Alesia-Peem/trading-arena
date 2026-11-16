#ifndef RNG_H
#define RNG_H

void rng_seed(unsigned long seed);
unsigned long rng_next(void);          /* 0..32767 */
int rng_range(int lo, int hi);         /* uniform in [lo, hi]; lo if lo > hi */

#endif
