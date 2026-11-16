#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/rng.h"
#include "../src/market.h"

static void test_init_matches_spec(void) {
    Market m; market_init(&m);
    assert(m.day == 0);
    assert(strcmp(m.stocks[0].symbol, "GRF") == 0 && m.stocks[0].price == 5000 && m.stocks[0].volatility_bp == 200);
    assert(strcmp(m.stocks[4].symbol, "LUCK") == 0 && m.stocks[4].price == 1500 && m.stocks[4].volatility_bp == 1200);
    assert(m.stocks[0].history_len == 1 && m.stocks[0].history[0] == 5000);
}
static void test_apply_change_up_and_down(void) {
    assert(apply_change(10000, 500) == 10500);
    assert(apply_change(10000, -250) == 9750);
}
static void test_apply_change_floor(void) { assert(apply_change(150, -5000) == PRICE_FLOOR); }
static void test_step_advances_day_and_history(void) {
    rng_seed(3); Market m; market_init(&m); market_step(&m);
    assert(m.day == 1 && m.stocks[2].history_len == 2 && m.stocks[2].history[1] == m.stocks[2].price);
}
static void test_price_floor_holds_over_many_days(void) {
    rng_seed(9); Market m; market_init(&m);
    for (int d = 0; d < 2000; d++) {          /* deliberately run past NUM_DAYS; append is guarded */
        market_step(&m);
        for (int i = 0; i < NUM_STOCKS; i++) assert(m.stocks[i].price >= PRICE_FLOOR);
    }
}
static void test_news_appears_sometimes(void) {
    rng_seed(11); int seen = 0;
    for (int g = 0; g < 20; g++) { Market m; market_init(&m);
        for (int d = 0; d < NUM_DAYS; d++) { market_step(&m); if (m.news[0]) seen++; } }
    assert(seen > 30 && seen < 150);   /* about 15% of 600 days, around 90 */
}
static void test_average(void) {
    Stock s = {0}; s.history[0] = 100; s.history[1] = 200; s.history[2] = 301; s.history_len = 3;
    assert(market_average(&s) == 200);
}

int main(void) {
    test_init_matches_spec(); test_apply_change_up_and_down(); test_apply_change_floor();
    test_step_advances_day_and_history(); test_price_floor_holds_over_many_days();
    test_news_appears_sometimes(); test_average();
    printf("test_market: ALL PASSED\n"); return 0;
}
