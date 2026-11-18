#include <string.h>
#include "bots.h"
#include "rng.h"

/* Careful Kim's rules */
#define CAREFUL_SELL_GAIN_PERCENT 110   /* sell when price >= 110% of what she paid (+10%) */
#define CAREFUL_SELL_LOSS_PERCENT  92   /* sell when price <=  92% of what she paid (-8%)  */
#define CAREFUL_BUY_DIP_PERCENT    95   /* buy when price <= 95% of the stock's average     */
#define CAREFUL_SPEND_PERCENT      20   /* each buy uses 20% of her cash */

/* Trend Tom's rules */
#define TREND_SPEND_PERCENT 25          /* each buy uses 25% of his cash */

/* Random Rita's rules */
#define RANDOM_MAX_BUY 10               /* buys 1 to 10 shares at a time */
#define RNG_MAX_SPAN   32768            /* rng_next() only has 32768 different values */

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

/* ---------- Helpers shared by the bots ---------- */

/* Is `price` at least `percent`% of `reference`?  (e.g. percent = 110 means "up 10% or more")
   We multiply instead of dividing so there are no rounding errors. */
static int is_at_least_percent_of(long price, long reference, int percent) {
    return (long long)price * 100 >= (long long)reference * percent;
}

static int is_at_most_percent_of(long price, long reference, int percent) {
    return (long long)price * 100 <= (long long)reference * percent;
}

/* Spend `percent`% of the bot's cash on stock `i`. Does nothing if that's not enough for 1 share. */
static void buy_with_percent_of_cash(Bot *b, const Market *m, int i, int percent) {
    long price = m->stocks[i].price;
    if (price <= 0) {
        return;
    }

    long budget = b->p.cash * percent / 100;
    long qty = budget / price;
    if (qty <= 0) {
        return;
    }

    if (portfolio_buy(&b->p, m, i, qty) == TRADE_OK) {
        b->buy_price[i] = price;
    }
}

static void sell_everything(Bot *b, const Market *m, int i) {
    portfolio_sell(&b->p, m, i, b->p.shares[i]);
}

/* ---------- Careful Kim ---------- */

static void act_careful(Bot *b, const Market *m, const int owned_at_start[NUM_STOCKS]) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        const Stock *stock = &m->stocks[i];
        long paid = b->buy_price[i];

        if (owned_at_start[i]) {
            int big_gain = is_at_least_percent_of(stock->price, paid, CAREFUL_SELL_GAIN_PERCENT);
            int big_loss = is_at_most_percent_of(stock->price, paid, CAREFUL_SELL_LOSS_PERCENT);
            if (big_gain || big_loss) {
                sell_everything(b, m, i);
            }
        } else {
            int cheap = is_at_most_percent_of(stock->price, market_average(stock),
                                              CAREFUL_BUY_DIP_PERCENT);
            if (cheap) {
                buy_with_percent_of_cash(b, m, i, CAREFUL_SPEND_PERCENT);
            }
        }
    }
}

/* ---------- Trend Tom ---------- */

/* Did the price go down from yesterday to today? */
static int fell_today(const Stock *s) {
    int n = s->history_len;
    if (n < 2) {
        return 0;   /* not enough days yet */
    }
    long today = s->history[n - 1];
    long yesterday = s->history[n - 2];
    return today < yesterday;
}

/* Did the price go up yesterday AND go up again today? */
static int rose_two_days_in_a_row(const Stock *s) {
    int n = s->history_len;
    if (n < 3) {
        return 0;   /* not enough days yet */
    }
    long today = s->history[n - 1];
    long yesterday = s->history[n - 2];
    long day_before = s->history[n - 3];
    return today > yesterday && yesterday > day_before;
}

static void act_trend(Bot *b, const Market *m, const int owned_at_start[NUM_STOCKS]) {
    for (int i = 0; i < NUM_STOCKS; i++) {
        const Stock *stock = &m->stocks[i];

        if (owned_at_start[i]) {
            if (fell_today(stock)) {
                sell_everything(b, m, i);
            }
        } else {
            if (rose_two_days_in_a_row(stock)) {
                buy_with_percent_of_cash(b, m, i, TREND_SPEND_PERCENT);
            }
        }
    }
}

/* ---------- Random Rita ---------- */

static void random_buy(Bot *b, const Market *m) {
    int i = rng_range(0, NUM_STOCKS - 1);
    long qty = rng_range(1, RANDOM_MAX_BUY);

    long can_afford = max_affordable(&b->p, m, i);
    if (qty > can_afford) {
        qty = can_afford;
    }
    if (qty <= 0) {
        return;
    }

    if (portfolio_buy(&b->p, m, i, qty) == TRADE_OK) {
        b->buy_price[i] = m->stocks[i].price;
    }
}

static void random_sell(Bot *b, const Market *m) {
    /* Make a list of the stocks she owns. */
    int owned[NUM_STOCKS];
    int owned_count = 0;
    for (int i = 0; i < NUM_STOCKS; i++) {
        if (b->p.shares[i] > 0) {
            owned[owned_count] = i;
            owned_count++;
        }
    }
    if (owned_count == 0) {
        return;   /* nothing to sell */
    }

    /* Pick one of them, and sell a random amount of it. */
    int i = owned[rng_range(0, owned_count - 1)];

    long most = b->p.shares[i];
    if (most > MAX_ORDER_QTY) {
        most = MAX_ORDER_QTY;
    }
    if (most > RNG_MAX_SPAN) {
        most = RNG_MAX_SPAN;
    }

    long qty = rng_range(1, (int)most);
    portfolio_sell(&b->p, m, i, qty);
}

static void act_random(Bot *b, const Market *m) {
    int action = rng_range(0, 2);

    if (action == 1) {
        random_buy(b, m);
    } else if (action == 2) {
        random_sell(b, m);
    }
    /* action == 0: do nothing today */
}

/* ---------- Main entry point ---------- */

void bot_act(Bot *b, const Market *m) {
    /* Remember what the bot owned BEFORE trading today, so a stock it buys
       today isn't immediately judged as "owned" (and sold) in the same turn. */
    int owned_at_start[NUM_STOCKS];
    for (int i = 0; i < NUM_STOCKS; i++) {
        owned_at_start[i] = (b->p.shares[i] > 0);
    }

    switch (b->strategy) {
        case BOT_CAREFUL:
            act_careful(b, m, owned_at_start);
            break;
        case BOT_TREND:
            act_trend(b, m, owned_at_start);
            break;
        case BOT_RANDOM:
            act_random(b, m);
            break;
    }
}
