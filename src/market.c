#include <stdio.h>
#include <string.h>
#include "market.h"
#include "rng.h"

/* How often news happens, and how strongly it moves the price. */
#define NEWS_CHANCE_PERCENT 15      /* 15% chance of news each day */
#define NEWS_MIN_MOVE_BP    1000    /* news moves a price at least 10% ... */
#define NEWS_MAX_MOVE_BP    2500    /* ... and at most 25% */
#define NUM_HEADLINES       4

/* The 5 stocks and how they start. Bigger volatility = wilder price swings. */
typedef struct {
    const char *symbol;
    const char *name;
    long start_price_cents;
    int  volatility_bp;
} StockInfo;

static const StockInfo STARTING_STOCKS[NUM_STOCKS] = {
    /* symbol  name                  price    volatility       */
    { "GRF",  "Golden Rice Farms",   5000L,   200 },   /* $50,  +/-2%  a day */
    { "BSR",  "Bangkok Sky Rail",   12000L,   300 },   /* $120, +/-3%  a day */
    { "MNGO", "Mango Energy",        8000L,   400 },   /* $80,  +/-4%  a day */
    { "DTEC", "Durian Tech",        20000L,   600 },   /* $200, +/-6%  a day */
    { "LUCK", "Lucky Coin Crypto",   1500L,  1200 },   /* $15,  +/-12% a day */
};

/* "%s" gets replaced by the company name. */
static const char *GOOD_NEWS[NUM_HEADLINES] = {
    "%s wins a huge contract! Shares soar",
    "%s posts record profits! Investors cheer",
    "%s announces a breakthrough! Shares jump",
    "%s gets a big upgrade from analysts! Shares rally",
};

static const char *BAD_NEWS[NUM_HEADLINES] = {
    "%s factory fire! Shares tumble",
    "%s CEO scandal! Investors flee",
    "%s misses earnings badly! Shares drop",
    "%s hit by a major recall! Shares slide",
};

void market_init(Market *m) {
    memset(m, 0, sizeof *m);

    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *stock = &m->stocks[i];
        const StockInfo *info = &STARTING_STOCKS[i];

        snprintf(stock->symbol, sizeof stock->symbol, "%s", info->symbol);
        snprintf(stock->name, sizeof stock->name, "%s", info->name);
        stock->price = info->start_price_cents;
        stock->volatility_bp = info->volatility_bp;

        stock->history[0] = stock->price;
        stock->history_len = 1;
    }

    m->day = 0;
    m->news[0] = '\0';
}

long apply_change(long price, int change_bp) {
    /* new price = price * (100% + change).  In basis points, 100% is 10000.
       "long long" is used so the multiplication can't overflow. */
    long long new_price = (long long)price * (10000LL + change_bp) / 10000LL;

    if (new_price < PRICE_FLOOR) {
        return PRICE_FLOOR;
    }
    return (long)new_price;
}

/* Every stock moves up or down by a random amount, up to its volatility. */
static void move_all_prices(Market *m) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *stock = &m->stocks[i];
        int change_bp = rng_range(-stock->volatility_bp, stock->volatility_bp);
        stock->price = apply_change(stock->price, change_bp);
    }
}

/* Sometimes a big news event hits one stock. */
static void maybe_make_news(Market *m) {
    m->news[0] = '\0';

    int roll = rng_range(1, 100);
    if (roll > NEWS_CHANCE_PERCENT) {
        return;   /* no news today */
    }

    /* Note: the order of these rng calls matters - changing it changes every game. */
    Stock *stock = &m->stocks[rng_range(0, NUM_STOCKS - 1)];
    int move_bp = rng_range(NEWS_MIN_MOVE_BP, NEWS_MAX_MOVE_BP);
    int is_bad_news = (rng_range(0, 1) == 0);

    const char *headline;
    if (is_bad_news) {
        headline = BAD_NEWS[rng_range(0, NUM_HEADLINES - 1)];
        stock->price = apply_change(stock->price, -move_bp);
    } else {
        headline = GOOD_NEWS[rng_range(0, NUM_HEADLINES - 1)];
        stock->price = apply_change(stock->price, move_bp);
    }

    snprintf(m->news, sizeof m->news, headline, stock->name);
}

/* Save today's prices so charts and bots can look back at them. */
static void record_prices(Market *m) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *stock = &m->stocks[i];
        if (stock->history_len < MAX_HISTORY) {
            stock->history[stock->history_len] = stock->price;
            stock->history_len++;
        }
    }
}

void market_step(Market *m) {
    move_all_prices(m);
    maybe_make_news(m);
    record_prices(m);
    m->day++;
}

long market_average(const Stock *s) {
    if (s->history_len <= 0) {
        return 0;
    }

    long sum = 0;
    for (int i = 0; i < s->history_len; i++) {
        sum += s->history[i];
    }
    return sum / s->history_len;
}
