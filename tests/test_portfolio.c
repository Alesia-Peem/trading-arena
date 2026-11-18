#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/market.h"
#include "../src/portfolio.h"

/* Every test starts with a fresh market and a player with $10,000.
   Stock 0 (GRF) costs $50.00 = 5000 cents.
   Every trade pays a 0.15% fee, rounded UP to the next cent. */
static void setup(Portfolio *p, Market *m) {
    market_init(m);
    portfolio_init(p, "You", START_CASH);
}

static void test_buy_ok(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_buy(&p, &m, 0, 10) == TRADE_OK);
    assert(p.cash == 949925);       /* paid 10 x $50 = $500, plus a $0.75 fee */
    assert(p.shares[0] == 10);
    assert(p.fees_paid == 75);
}

static void test_sell_ok(void) {
    Portfolio p; Market m;
    setup(&p, &m);
    portfolio_buy(&p, &m, 0, 10);

    assert(portfolio_sell(&p, &m, 0, 4) == TRADE_OK);
    assert(p.cash == 969895);       /* got 4 x $50 = $200, minus a $0.30 fee */
    assert(p.shares[0] == 6);
    assert(p.fees_paid == 105);
}

static void test_buy_rejects_no_cash(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_buy(&p, &m, 0, 201) == TRADE_NO_CASH);   /* 201 x $50 > $10,000 */
    assert(p.cash == START_CASH);
    assert(p.shares[0] == 0);
}

static void test_buy_rejects_bad_qty_and_stock(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_buy(&p, &m, 0, 0) == TRADE_BAD_INPUT);
    assert(portfolio_buy(&p, &m, 0, -5) == TRADE_BAD_INPUT);
    assert(portfolio_buy(&p, &m, 5, 1) == TRADE_BAD_INPUT);
    assert(portfolio_buy(&p, &m, -1, 1) == TRADE_BAD_INPUT);
}

static void test_buy_rejects_huge_qty(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_buy(&p, &m, 0, 999999999L) == TRADE_BAD_INPUT);
    assert(p.cash == START_CASH);
}

static void test_sell_rejects_more_than_owned(void) {
    Portfolio p; Market m;
    setup(&p, &m);
    portfolio_buy(&p, &m, 0, 3);

    assert(portfolio_sell(&p, &m, 0, 4) == TRADE_NO_SHARES);
    assert(p.shares[0] == 3);
}

static void test_sell_rejects_bad_input(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_sell(&p, &m, 0, 0) == TRADE_BAD_INPUT);
    assert(portfolio_sell(&p, &m, 9, 1) == TRADE_BAD_INPUT);
}

static void test_value_counts_shares_at_current_price(void) {
    Portfolio p; Market m;
    setup(&p, &m);
    portfolio_buy(&p, &m, 0, 10);

    m.stocks[0].price = 6000;   /* price goes up to $60 */

    assert(portfolio_value(&p, &m) == 949925 + 60000);
}

static void test_max_affordable(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(max_affordable(&p, &m, 0) == 199);   /* 200 x $50 = all the cash, leaving nothing for the fee */
    assert(max_affordable(&p, &m, 7) == 0);     /* no stock 7 */
    assert(max_affordable(&p, &m, -1) == 0);
}

static void test_init_name_truncates(void) {
    Portfolio p;
    portfolio_init(&p, "AVeryLongPlayerNameThatDoesNotFit", 5);

    assert(strlen(p.name) == 23);
    assert(p.cash == 5);
    assert(p.shares[3] == 0);
}

static void test_buy_must_cover_fee(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_buy(&p, &m, 0, 200) == TRADE_NO_CASH);   /* $10,000 + $15 fee > $10,000 */
    assert(p.cash == START_CASH);
    assert(portfolio_buy(&p, &m, 0, 199) == TRADE_OK);        /* $9,950 + $14.93 fee */
    assert(p.cash == 3507);
    assert(p.shares[0] == 199);
}

static void test_trade_fee(void) {
    assert(trade_fee(0) == 0);
    assert(trade_fee(10000) == 15);    /* $100 -> $0.15 */
    assert(trade_fee(50000) == 75);    /* $500 -> $0.75 */
    assert(trade_fee(1) == 1);         /* rounded up: every trade pays at least 1 cent */
    assert(trade_fee(6667) == 11);     /* 10.0005 cents -> 11 */
}

static void test_buy_max_qty_no_cash(void) {
    Portfolio p; Market m;
    setup(&p, &m);

    assert(portfolio_buy(&p, &m, 0, MAX_ORDER_QTY) == TRADE_NO_CASH);
    assert(p.cash == START_CASH);
    assert(p.shares[0] == 0);
}

static void test_failed_sell_keeps_cash(void) {
    Portfolio p; Market m;
    setup(&p, &m);
    portfolio_buy(&p, &m, 0, 3);

    assert(portfolio_sell(&p, &m, 0, 4) == TRADE_NO_SHARES);
    assert(p.cash == START_CASH - 15023);   /* 3 x $50 + $0.23 fee */
    assert(p.shares[0] == 3);
}

int main(void) {
    test_buy_ok();
    test_sell_ok();
    test_buy_rejects_no_cash();
    test_buy_rejects_bad_qty_and_stock();
    test_buy_rejects_huge_qty();
    test_sell_rejects_more_than_owned();
    test_sell_rejects_bad_input();
    test_value_counts_shares_at_current_price();
    test_max_affordable();
    test_init_name_truncates();
    test_buy_must_cover_fee();
    test_trade_fee();
    test_buy_max_qty_no_cash();
    test_failed_sell_keeps_cash();

    printf("test_portfolio: ALL PASSED\n");
    return 0;
}
