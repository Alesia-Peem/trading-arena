#include <string.h>
#include "game.h"
#include "rng.h"

void game_new(Game *g, unsigned long seed) {
    memset(g, 0, sizeof *g);
    g->seed = seed;

    rng_seed(seed);
    market_init(&g->market);
    portfolio_init(&g->you, "You", START_CASH);
    bots_init_all(g->bots);
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
}
