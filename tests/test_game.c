#include <assert.h>
#include <stdio.h>
#include "../src/game.h"

/* ---------- Trade history ---------- */

static void test_trades_are_recorded(void) {
    static Game g;   /* static: Game is big */
    game_new(&g, 42);
    long price = g.market.stocks[1].price;

    assert(game_buy(&g, 1, 5) == TRADE_OK);
    assert(g.trade_count == 1);
    assert(g.trades[0].day == 0);
    assert(g.trades[0].stock == 1);
    assert(g.trades[0].qty == 5);                       /* + = bought */
    assert(g.trades[0].price == price);
    assert(g.trades[0].fee == trade_fee(5 * price));

    game_next_day(&g);
    assert(game_sell(&g, 1, 2) == TRADE_OK);
    assert(g.trade_count == 2);
    assert(g.trades[1].day == 1);
    assert(g.trades[1].qty == -2);                      /* - = sold */
    assert(g.trades[1].price == g.market.stocks[1].price);
}

static void test_failed_trades_are_not_recorded(void) {
    static Game g;
    game_new(&g, 42);

    assert(game_sell(&g, 0, 1) == TRADE_NO_SHARES);
    assert(game_buy(&g, 0, MAX_ORDER_QTY) == TRADE_NO_CASH);
    assert(game_buy(&g, 9, 1) == TRADE_BAD_INPUT);
    assert(g.trade_count == 0);
}

static void test_trade_log_full_still_trades(void) {
    static Game g;
    game_new(&g, 42);
    for (int i = 0; i < MAX_TRADES + 5; i++) {
        assert(game_buy(&g, 4, 1) == TRADE_OK);          /* LUCK is cheap */
    }
    assert(g.trade_count == MAX_TRADES);                 /* the log stops growing ... */
    assert(g.you.shares[4] == MAX_TRADES + 5);           /* ... but trading still works */
}

/* ---------- The race: net worth every day ---------- */

static void test_worth_history_every_day(void) {
    static Game g;
    game_new(&g, 7);
    assert(g.worth_len == 1);
    for (int player = 0; player < NUM_PLAYERS; player++) {
        assert(g.worth_history[player][0] == START_CASH);
    }

    for (int day = 0; day < NUM_DAYS; day++) {
        game_next_day(&g);
    }
    assert(g.worth_len == NUM_DAYS + 1);
    assert(g.worth_history[0][NUM_DAYS] == portfolio_value(&g.you, &g.market));
    assert(g.worth_history[1][NUM_DAYS] == portfolio_value(&g.bots[0].p, &g.market));

    game_next_day(&g);                                    /* game over: nothing more is added */
    assert(g.worth_len == NUM_DAYS + 1);
}

int main(void) {
    test_trades_are_recorded();
    test_failed_trades_are_not_recorded();
    test_trade_log_full_still_trades();
    test_worth_history_every_day();

    printf("test_game: ALL PASSED\n");
    return 0;
}
