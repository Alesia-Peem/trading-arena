#include <stdio.h>
#include <string.h>
#include "market.h"
#include "rng.h"

/* How often news happens, and how strongly it moves the price. */
#define NEWS_CHANCE_PERCENT 15      /* 15% chance of news each day */
#define NEWS_MIN_MOVE_BP    1000    /* news moves a price at least 10% ... */
#define NEWS_MAX_MOVE_BP    2500    /* ... and at most 25% */
#define NUM_HEADLINES       4

/* The whole market's mood. On a normal day it moves up to +/-1%.
   About 1 day in 25 is a crash or a rally that hits every stock at once. */
#define MARKET_DAILY_BP      100
#define EVENT_CHANCE_PERCENT 4
#define CRASH_MIN_BP         400     /* a crash: the market falls 4% to 8% */
#define CRASH_MAX_BP         800
#define RALLY_MIN_BP         300     /* a rally: the market rises 3% to 6% */
#define RALLY_MAX_BP         600

/* The 5 stocks and how they start. Bigger volatility = wilder price swings. */
typedef struct {
    const char *symbol;
    const char *name;
    long start_price_cents;
    int  volatility_bp;
    int  beta_pct;
} StockInfo;

static const StockInfo STARTING_STOCKS[NUM_STOCKS] = {
    /* symbol  name                  price   volatility beta                                   */
    { "GRF",  "Golden Rice Farms",   5000L,   200,   50 },   /* $50,  +/-2%  a day, calm: half the market */
    { "BSR",  "Bangkok Sky Rail",   12000L,   300,   80 },   /* $120, +/-3%  a day                       */
    { "MNGO", "Mango Energy",        8000L,   400,  100 },   /* $80,  +/-4%  a day, moves with the market */
    { "DTEC", "Durian Tech",        20000L,   600,  150 },   /* $200, +/-6%  a day, tech: 1.5x the market */
    { "LUCK", "Lucky Coin Crypto",   1500L,  1200,  200 },   /* $15,  +/-12% a day, crypto: 2x the market */
};

/* "%s" gets replaced by the company name. */
static const char *GOOD_NEWS[NUM_HEADLINES] = {
    "%s wins a huge contract! Shares soar",
    "%s posts record profits! Investors cheer",
    "%s announces a breakthrough! Shares jump",
    "%s gets a big upgrade from analysts! Shares rally",
};

static const char *CRASH_NEWS = "Market crash! Panic selling hits every stock";
static const char *RALLY_NEWS = "Market rally! Buyers pour into every stock";

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
        stock->beta_pct = info->beta_pct;

        stock->history[0] = stock->price;
        stock->history_len = 1;
    }

    m->day = 0;
    m->news[0] = '\0';
    m->index = INDEX_START;
    m->index_history[0] = INDEX_START;
    m->index_history_len = 1;
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

int stock_move_bp(int market_bp, int beta_pct, int own_bp) {
    return market_bp * beta_pct / 100 + own_bp;
}

/* Pick today's mood for the whole market: usually a small move, sometimes a crash or rally. */
static void pick_market_move(Market *m) {
    m->market_event = 0;
    if (rng_range(1, 100) <= EVENT_CHANCE_PERCENT) {
        if (rng_range(0, 1) == 0) {
            m->market_event = -1;
            m->market_move_bp = -rng_range(CRASH_MIN_BP, CRASH_MAX_BP);
        } else {
            m->market_event = 1;
            m->market_move_bp = rng_range(RALLY_MIN_BP, RALLY_MAX_BP);
        }
    } else {
        m->market_move_bp = rng_range(-MARKET_DAILY_BP, MARKET_DAILY_BP);
    }
    m->index = apply_change(m->index, m->market_move_bp);
}

/* Every stock follows the market (by its beta), plus a random move of its own. */
static void move_all_prices(Market *m) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *stock = &m->stocks[i];
        int own_bp = rng_range(-stock->volatility_bp, stock->volatility_bp);
        int change_bp = stock_move_bp(m->market_move_bp, stock->beta_pct, own_bp);
        stock->price = apply_change(stock->price, change_bp);
    }
}

/* Sometimes a big news event hits one stock. */
static void maybe_make_news(Market *m) {
    m->news[0] = '\0';

    /* A crash or rally is the day's big story, so there's no company news on top. */
    if (m->market_event != 0) {
        snprintf(m->news, sizeof m->news, "%s", m->market_event < 0 ? CRASH_NEWS : RALLY_NEWS);
        return;
    }

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

/* Save today's prices and index so charts and bots can look back at them. */
static void record_prices(Market *m) {
    if (m->index_history_len < MAX_HISTORY) {
        m->index_history[m->index_history_len] = m->index;
        m->index_history_len++;
    }
    for (int i = 0; i < NUM_STOCKS; i++) {
        Stock *stock = &m->stocks[i];
        if (stock->history_len < MAX_HISTORY) {
            stock->history[stock->history_len] = stock->price;
            stock->history_len++;
        }
    }
}

void market_step(Market *m) {
    pick_market_move(m);
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
