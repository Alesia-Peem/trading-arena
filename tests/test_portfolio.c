#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/market.h"
#include "../src/portfolio.h"

static void setup(Portfolio *p, Market *m) { market_init(m); portfolio_init(p, "You", START_CASH); }
static void test_buy_ok(void)  { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,10) == TRADE_OK); assert(p.cash == 950000 && p.shares[0] == 10); }
static void test_sell_ok(void) { Portfolio p; Market m; setup(&p,&m); portfolio_buy(&p,&m,0,10);
    assert(portfolio_sell(&p,&m,0,4) == TRADE_OK); assert(p.cash == 970000 && p.shares[0] == 6); }
static void test_buy_rejects_no_cash(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,201) == TRADE_NO_CASH); assert(p.cash == START_CASH && p.shares[0] == 0); }
static void test_buy_rejects_bad_qty_and_stock(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,0) == TRADE_BAD_INPUT); assert(portfolio_buy(&p,&m,0,-5) == TRADE_BAD_INPUT);
    assert(portfolio_buy(&p,&m,5,1) == TRADE_BAD_INPUT); assert(portfolio_buy(&p,&m,-1,1) == TRADE_BAD_INPUT); }
static void test_buy_rejects_huge_qty(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,999999999L) == TRADE_BAD_INPUT); assert(p.cash == START_CASH); }
static void test_sell_rejects_more_than_owned(void) { Portfolio p; Market m; setup(&p,&m); portfolio_buy(&p,&m,0,3);
    assert(portfolio_sell(&p,&m,0,4) == TRADE_NO_SHARES); assert(p.shares[0] == 3); }
static void test_sell_rejects_bad_input(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_sell(&p,&m,0,0) == TRADE_BAD_INPUT); assert(portfolio_sell(&p,&m,9,1) == TRADE_BAD_INPUT); }
static void test_value_counts_shares_at_current_price(void) { Portfolio p; Market m; setup(&p,&m);
    portfolio_buy(&p,&m,0,10); m.stocks[0].price = 6000; assert(portfolio_value(&p,&m) == 950000 + 60000); }
static void test_max_affordable(void) { Portfolio p; Market m; setup(&p,&m);
    assert(max_affordable(&p,&m,0) == 200); assert(max_affordable(&p,&m,7) == 0); assert(max_affordable(&p,&m,-1) == 0); }
static void test_buy_exact_cash_ok(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,200) == TRADE_OK); assert(p.cash == 0 && p.shares[0] == 200); }
static void test_buy_max_qty_no_cash(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,MAX_ORDER_QTY) == TRADE_NO_CASH);
    assert(p.cash == START_CASH && p.shares[0] == 0); }
static void test_failed_sell_keeps_cash(void) { Portfolio p; Market m; setup(&p,&m); portfolio_buy(&p,&m,0,3);
    assert(portfolio_sell(&p,&m,0,4) == TRADE_NO_SHARES); assert(p.cash == START_CASH - 15000 && p.shares[0] == 3); }
static void test_init_name_truncates(void) { Portfolio p;
    portfolio_init(&p, "AVeryLongPlayerNameThatDoesNotFit", 5);
    assert(strlen(p.name) == 23 && p.cash == 5 && p.shares[3] == 0); }

int main(void) {
    test_buy_ok(); test_sell_ok(); test_buy_rejects_no_cash(); test_buy_rejects_bad_qty_and_stock();
    test_buy_rejects_huge_qty(); test_sell_rejects_more_than_owned(); test_sell_rejects_bad_input();
    test_value_counts_shares_at_current_price(); test_max_affordable(); test_init_name_truncates();
    test_buy_exact_cash_ok(); test_buy_max_qty_no_cash(); test_failed_sell_keeps_cash();
    printf("test_portfolio: ALL PASSED\n");
    return 0;
}
