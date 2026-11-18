#include "rng.h"

/*
 * A "linear congruential generator" (LCG), the classic textbook formula:
 *
 *     next_state = (state * 1103515245 + 12345), kept to 32 bits
 *
 * The "& 0xFFFFFFFF" keeps only the low 32 bits, so the result is the same
 * whether `unsigned long` is 32 or 64 bits on this computer.
 */

#define KEEP_32_BITS 0xFFFFFFFFUL

static unsigned long state = 1;

void rng_seed(unsigned long seed) {
    state = seed & KEEP_32_BITS;
}

unsigned long rng_next(void) {
    state = (state * 1103515245UL + 12345UL) & KEEP_32_BITS;

    /* The low bits of an LCG are not very random, so use bits 16..30 instead. */
    return (state / 65536UL) % 32768UL;
}

int rng_range(int lo, int hi) {
    if (lo > hi) {
        return lo;
    }
    unsigned long how_many_values = (unsigned long)(hi - lo + 1);
    return lo + (int)(rng_next() % how_many_values);
}
