#include <stdio.h>
#include <string.h>
#include "market.h"
#include "rng.h"

static const struct { const char *sym, *name; long price; int vol; } TABLE[NUM_STOCKS] = {
    {"GRF",  "Golden Rice Farms",  5000L,  200},
    {"BSR",  "Bangkok Sky Rail",  12000L,  300},
    {"MNGO", "Mango Energy",       8000L,  400},
    {"DTEC", "Durian Tech",       20000L,  600},
    {"LUCK", "Lucky Coin Crypto",  1500L, 1200}
};

static const char *GOOD_NEWS[4] = {
    "%s wins a huge contract! Shares soar",
    "%s posts record profits! Investors cheer",
    "%s announces a breakthrough! Shares jump",
    "%s gets a big upgrade from analysts! Shares rally"
};
static const char *BAD_NEWS[4] = {
    "%s factory fire! Shares tumble",
    "%s CEO scandal! Investors flee",
    "%s misses earnings badly! Shares drop",
    "%s hit by a major recall! Shares slide"
};

void market_init(Market *m) {
    memset(m, 0, sizeof *m);
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *s = &m->stocks[i];
        snprintf(s->symbol, sizeof s->symbol, "%s", TABLE[i].sym);
        snprintf(s->name, sizeof s->name, "%s", TABLE[i].name);
        s->price = TABLE[i].price;
        s->volatility_bp = TABLE[i].vol;
        s->history[0] = s->price;
        s->history_len = 1;
    }
    m->day = 0;
    m->news[0] = '\0';
}

long apply_change(long price, int change_bp) {
    long p = (long)((long long)price * (10000LL + change_bp) / 10000LL);
    return p < PRICE_FLOOR ? PRICE_FLOOR : p;
}

void market_step(Market *m) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *s = &m->stocks[i];
        s->price = apply_change(s->price, rng_range(-s->volatility_bp, s->volatility_bp));
    }
    m->news[0] = '\0';
    if (rng_range(1, 100) <= 15) {
        int idx = rng_range(0, NUM_STOCKS - 1);
        int shock = rng_range(1000, 2500);
        int down = (rng_range(0, 1) == 0);
        const char *tpl = down ? BAD_NEWS[rng_range(0, 3)] : GOOD_NEWS[rng_range(0, 3)];
        Stock *s = &m->stocks[idx];
        s->price = apply_change(s->price, down ? -shock : shock);
        snprintf(m->news, sizeof m->news, tpl, s->name);
    }
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *s = &m->stocks[i];
        if (s->history_len < MAX_HISTORY) s->history[s->history_len++] = s->price;
    }
    m->day++;
}

long market_average(const Stock *s) {
    long sum = 0;
    if (s->history_len <= 0) return 0;
    for (int i = 0; i < s->history_len; i++) sum += s->history[i];
    return sum / s->history_len;
}
