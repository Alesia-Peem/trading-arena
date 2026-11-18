#include <string.h>
#include "game.h"
#include "rng.h"

/* Save every player's net worth for today, for the race chart. */
static void record_worth(Game *g) {
    if (g->worth_len >= MAX_HISTORY) {
        return;
    }
    g->worth_history[0][g->worth_len] = portfolio_value(&g->you, &g->market);
    for (int i = 0; i < NUM_BOTS; i++) {
        g->worth_history[i + 1][g->worth_len] = portfolio_value(&g->bots[i].p, &g->market);
    }
    g->worth_len++;
}

void game_new(Game *g, unsigned long seed) {
    memset(g, 0, sizeof *g);
    g->seed = seed;

    rng_seed(seed);
    market_init(&g->market);
    portfolio_init(&g->you, "You", START_CASH);
    bots_init_all(g->bots);
    record_worth(g);   /* day 0: everyone starts with the same cash */
}

/* Add a successful trade to the history (if there is room). */
static void record_trade(Game *g, int stock, long qty, long fee) {
    if (g->trade_count >= MAX_TRADES) {
        return;
    }
    Trade *t = &g->trades[g->trade_count];
    t->day = g->market.day;
    t->stock = stock;
    t->qty = qty;
    t->price = g->market.stocks[stock].price;
    t->fee = fee;
    g->trade_count++;
}

int game_buy(Game *g, int stock, long qty) {
    long fees_before = g->you.fees_paid;
    int result = portfolio_buy(&g->you, &g->market, stock, qty);
    if (result == TRADE_OK) {
        record_trade(g, stock, qty, g->you.fees_paid - fees_before);
    }
    return result;
}

int game_sell(Game *g, int stock, long qty) {
    long fees_before = g->you.fees_paid;
    int result = portfolio_sell(&g->you, &g->market, stock, qty);
    if (result == TRADE_OK) {
        record_trade(g, stock, -qty, g->you.fees_paid - fees_before);
    }
    return result;
}

int game_is_over(const Game *g) {
    return g->market.day >= NUM_DAYS;
}

/* Let one bot trade and remember how its share counts changed. */
static void run_bot(Bot *bot, const Market *m, long trades[NUM_STOCKS]) {
    long shares_before[NUM_STOCKS];
    for (int s = 0; s < NUM_STOCKS; s++) {
        shares_before[s] = bot->p.shares[s];
    }

    bot_act(bot, m);

    for (int s = 0; s < NUM_STOCKS; s++) {
        trades[s] = bot->p.shares[s] - shares_before[s];
    }
}

void game_next_day(Game *g) {
    if (game_is_over(g)) {
        return;
    }

    market_step(&g->market);

    for (int i = 0; i < NUM_BOTS; i++) {
        run_bot(&g->bots[i], &g->market, g->bot_trades[i]);
    }
    record_worth(g);
}
