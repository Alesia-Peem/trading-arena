#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/rng.h"
#include "../src/market.h"

static void test_init_matches_spec(void) {
    Market m;
    market_init(&m);

    assert(m.day == 0);

    assert(strcmp(m.stocks[0].symbol, "GRF") == 0);
    assert(m.stocks[0].price == 5000);
    assert(m.stocks[0].volatility_bp == 200);

    assert(strcmp(m.stocks[4].symbol, "LUCK") == 0);
    assert(m.stocks[4].price == 1500);
    assert(m.stocks[4].volatility_bp == 1200);

    assert(m.stocks[0].history_len == 1);
    assert(m.stocks[0].history[0] == 5000);
}

static void test_apply_change_up_and_down(void) {
    assert(apply_change(10000, 500) == 10500);    /* +5%   */
    assert(apply_change(10000, -250) == 9750);    /* -2.5% */
}

static void test_apply_change_floor(void) {
    assert(apply_change(150, -5000) == PRICE_FLOOR);
}

static void test_step_advances_day_and_history(void) {
    rng_seed(3);
    Market m;
    market_init(&m);

    market_step(&m);

    assert(m.day == 1);
    assert(m.stocks[2].history_len == 2);
    assert(m.stocks[2].history[1] == m.stocks[2].price);
}

static void test_price_floor_holds_over_many_days(void) {
    rng_seed(9);
    Market m;
    market_init(&m);

    /* Deliberately run far past NUM_DAYS; history only grows up to MAX_HISTORY. */
    for (int day = 0; day < 2000; day++) {
        market_step(&m);
        for (int i = 0; i < NUM_STOCKS; i++) {
            assert(m.stocks[i].price >= PRICE_FLOOR);
        }
    }
}

static void test_news_appears_sometimes(void) {
    rng_seed(11);
    int news_days = 0;

    for (int game = 0; game < 20; game++) {
        Market m;
        market_init(&m);
        for (int day = 0; day < NUM_DAYS; day++) {
            market_step(&m);
            if (m.news[0] != '\0') news_days++;
        }
    }

    assert(news_days > 30 && news_days < 150);   /* about 15% of 600 days = around 90 */
}

static void test_average(void) {
    Stock s = {0};
    s.history[0] = 100;
    s.history[1] = 200;
    s.history[2] = 301;
    s.history_len = 3;

    assert(market_average(&s) == 200);   /* 601 / 3 = 200 (rounded down) */
}

/* ---------- The whole market ---------- */

static void test_init_has_beta_and_index(void) {
    Market m;
    market_init(&m);
    assert(m.stocks[0].beta_pct == 50);      /* GRF: rice farms barely follow the market */
    assert(m.stocks[4].beta_pct == 200);     /* LUCK: crypto moves twice as much as the market */
    assert(m.index == INDEX_START);
    assert(m.index_history_len == 1);
    assert(m.index_history[0] == INDEX_START);
    assert(m.market_event == 0);
}

static void test_stock_move_combines_market_and_own(void) {
    assert(stock_move_bp(200, 150, -100) == 200);    /* +2% market x 1.5 = +3%, own -1% */
    assert(stock_move_bp(-800, 50, 0) == -400);      /* a -8% crash only half-hits GRF */
    assert(stock_move_bp(0, 200, 37) == 37);         /* flat market: only its own move */
}

static void test_index_follows_market_move(void) {
    rng_seed(3);
    Market m;
    market_init(&m);
    market_step(&m);
    assert(m.index == apply_change(INDEX_START, m.market_move_bp));
    assert(m.index_history_len == 2);
    assert(m.index_history[1] == m.index);
}

static void test_market_events_crash_and_rally(void) {
    rng_seed(11);
    int crashes = 0, rallies = 0;
    for (int game = 0; game < 20; game++) {
        Market m;
        market_init(&m);
        for (int day = 0; day < NUM_DAYS; day++) {
            market_step(&m);
            if (m.market_event < 0) {
                crashes++;
                assert(m.market_move_bp <= -400 && m.market_move_bp >= -800);
                assert(m.news[0] != '\0');
            } else if (m.market_event > 0) {
                rallies++;
                assert(m.market_move_bp >= 300 && m.market_move_bp <= 600);
                assert(m.news[0] != '\0');
            } else {
                assert(m.market_move_bp >= -100 && m.market_move_bp <= 100);   /* a normal day */
            }
        }
    }
    assert(crashes > 0 && rallies > 0);
    assert(crashes + rallies < 60);    /* about 4% of 600 days = around 24 */
}

int main(void) {
    test_init_matches_spec();
    test_apply_change_up_and_down();
    test_apply_change_floor();
    test_step_advances_day_and_history();
    test_price_floor_holds_over_many_days();
    test_news_appears_sometimes();
    test_average();
    test_init_has_beta_and_index();
    test_stock_move_combines_market_and_own();
    test_index_follows_market_move();
    test_market_events_crash_and_rally();

    printf("test_market: ALL PASSED\n");
    return 0;
}
