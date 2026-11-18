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

int main(void) {
    test_init_matches_spec();
    test_apply_change_up_and_down();
    test_apply_change_floor();
    test_step_advances_day_and_history();
    test_price_floor_holds_over_many_days();
    test_news_appears_sometimes();
    test_average();

    printf("test_market: ALL PASSED\n");
    return 0;
}
