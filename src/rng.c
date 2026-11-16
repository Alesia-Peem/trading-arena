#include "rng.h"

static unsigned long state = 1;

void rng_seed(unsigned long seed) {
    state = seed & 0xFFFFFFFFUL;
}

unsigned long rng_next(void) {
    state = (state * 1103515245UL + 12345UL) & 0xFFFFFFFFUL;
    return (state / 65536UL) % 32768UL;
}

int rng_range(int lo, int hi) {
    if (lo > hi) return lo;
    return lo + (int)(rng_next() % (unsigned long)(hi - lo + 1));
}
