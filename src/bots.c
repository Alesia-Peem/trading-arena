#include <string.h>
#include "bots.h"
#include "rng.h"

void bot_init(Bot *b, const char *name, BotStrategy s) {
    portfolio_init(&b->p, name, START_CASH);
    b->strategy = s;
    memset(b->buy_price, 0, sizeof b->buy_price);
}

void bots_init_all(Bot bots[NUM_BOTS]) {
    bot_init(&bots[0], "Careful Kim", BOT_CAREFUL);
    bot_init(&bots[1], "Trend Tom", BOT_TREND);
    bot_init(&bots[2], "Random Rita", BOT_RANDOM);
}

/* Buy with `percent` of current cash; skip if that buys 0 shares. */
static void buy_percent(Bot *b, const Market *m, int i, int percent) {
    long price = m->stocks[i].price;
    long qty;
    if (price <= 0) return;
    qty = (b->p.cash * percent / 100) / price;
    if (qty <= 0) return;
    if (portfolio_buy(&b->p, m, i, qty) == TRADE_OK) b->buy_price[i] = price;
}

static void sell_all(Bot *b, const Market *m, int i) {
    portfolio_sell(&b->p, m, i, b->p.shares[i]);
}

static void act_careful(Bot *b, const Market *m, const int held[NUM_STOCKS]) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        const Stock *s = &m->stocks[i];
        long long price100 = (long long)s->price * 100;
        if (held[i]) {
            long long bp = b->buy_price[i];
            if (price100 >= bp * 110 || price100 <= bp * 92) sell_all(b, m, i);
        } else if (price100 <= (long long)market_average(s) * 95) {
            buy_percent(b, m, i, 20);
        }
    }
}

static void act_trend(Bot *b, const Market *m, const int held[NUM_STOCKS]) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        const Stock *s = &m->stocks[i];
        int n = s->history_len;
        if (held[i]) {
            if (n >= 2 && s->history[n - 1] < s->history[n - 2]) sell_all(b, m, i);
        } else if (n >= 3 && s->history[n - 1] > s->history[n - 2]
                          && s->history[n - 2] > s->history[n - 3]) {
            buy_percent(b, m, i, 25);
        }
    }
}

static void act_random(Bot *b, const Market *m) {
    int action = rng_range(0, 2);
    if (action == 1) {
        int i = rng_range(0, NUM_STOCKS - 1);
        long qty = rng_range(1, 10);
        long cap = max_affordable(&b->p, m, i);
        if (qty > cap) qty = cap;
        if (qty <= 0) return;
        if (portfolio_buy(&b->p, m, i, qty) == TRADE_OK) b->buy_price[i] = m->stocks[i].price;
    } else if (action == 2) {
        int owned[NUM_STOCKS], count = 0;
        for (int i = 0; i < NUM_STOCKS; i++)
            if (b->p.shares[i] > 0) owned[count++] = i;
        if (count == 0) return;
        int i = owned[rng_range(0, count - 1)];
        long span = b->p.shares[i] > MAX_ORDER_QTY ? MAX_ORDER_QTY : b->p.shares[i];
        portfolio_sell(&b->p, m, i, rng_range(1, (int)(span > 32768 ? 32768 : span)));
    }
}

void bot_act(Bot *b, const Market *m) {
    int held[NUM_STOCKS];
    for (int i = 0; i < NUM_STOCKS; i++) held[i] = b->p.shares[i] > 0;
    switch (b->strategy) {
        case BOT_CAREFUL: act_careful(b, m, held); break;
        case BOT_TREND:   act_trend(b, m, held);   break;
        case BOT_RANDOM:  act_random(b, m);        break;
    }
}
