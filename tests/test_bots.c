#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/rng.h"
#include "../src/market.h"
#include "../src/portfolio.h"
#include "../src/bots.h"

static void test_careful_buys_dip(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    Stock *s = &m.stocks[1]; s->history[0]=10000; s->history[1]=10000; s->history[2]=9000; s->history_len=3; s->price=9000;
    bot_act(&b,&m); assert(b.p.shares[1] > 0 && b.buy_price[1] == 9000); }
static void test_careful_takes_profit(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    b.p.shares[0] = 10; b.buy_price[0] = 5000; m.stocks[0].price = 5500;
    bot_act(&b,&m); assert(b.p.shares[0] == 0); }
static void test_careful_cuts_loss(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    b.p.shares[0] = 10; b.buy_price[0] = 5000; m.stocks[0].price = 4600;
    bot_act(&b,&m); assert(b.p.shares[0] == 0); }
static void test_trend_buys_two_rises(void) { Market m; market_init(&m); Bot b; bot_init(&b,"T",BOT_TREND);
    Stock *s=&m.stocks[2]; s->history[0]=8000; s->history[1]=8100; s->history[2]=8200; s->history_len=3; s->price=8200;
    bot_act(&b,&m); assert(b.p.shares[2] > 0); }
static void test_trend_sells_on_fall(void) { Market m; market_init(&m); Bot b; bot_init(&b,"T",BOT_TREND);
    b.p.shares[2]=5; Stock *s=&m.stocks[2]; s->history[0]=8000; s->history[1]=7900; s->history_len=2; s->price=7900;
    bot_act(&b,&m); assert(b.p.shares[2] == 0); }
static void test_random_never_breaks_rules(void) { rng_seed(5); Market m; market_init(&m); Bot b; bot_init(&b,"R",BOT_RANDOM);
    for (int d=0; d<NUM_DAYS; d++) { market_step(&m); bot_act(&b,&m);
        assert(b.p.cash >= 0); for (int i=0;i<NUM_STOCKS;i++) assert(b.p.shares[i] >= 0); } }
static void test_bots_differ(void) { rng_seed(42); Market m; market_init(&m); Bot bots[NUM_BOTS]; bots_init_all(bots);
    for (int d=0; d<NUM_DAYS; d++) { market_step(&m); for (int i=0;i<NUM_BOTS;i++) bot_act(&bots[i],&m); }
    long v0=portfolio_value(&bots[0].p,&m), v1=portfolio_value(&bots[1].p,&m), v2=portfolio_value(&bots[2].p,&m);
    assert(!(v0==v1 && v1==v2)); }
static void test_init_all_names_and_state(void) { Bot bots[NUM_BOTS]; bots_init_all(bots);
    assert(strcmp(bots[0].p.name,"Careful Kim")==0 && bots[0].strategy==BOT_CAREFUL);
    assert(strcmp(bots[1].p.name,"Trend Tom")==0 && bots[1].strategy==BOT_TREND);
    assert(strcmp(bots[2].p.name,"Random Rita")==0 && bots[2].strategy==BOT_RANDOM);
    for (int i=0;i<NUM_BOTS;i++) { assert(bots[i].p.cash==START_CASH);
        for (int j=0;j<NUM_STOCKS;j++) assert(bots[i].buy_price[j]==0 && bots[i].p.shares[j]==0); } }
static void test_trend_does_not_pile_in(void) { Market m; market_init(&m); Bot b; bot_init(&b,"T",BOT_TREND);
    b.p.shares[2]=5; Stock *s=&m.stocks[2]; s->history[0]=8000; s->history[1]=8100; s->history[2]=8200; s->history_len=3; s->price=8200;
    bot_act(&b,&m); assert(b.p.shares[2]==5 && b.p.cash==START_CASH); }
static void test_careful_no_rebuy_same_day(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    b.p.shares[0] = 10; b.buy_price[0] = 5000; m.stocks[0].price = 4600;
    bot_act(&b,&m); assert(b.p.shares[0] == 0); }

int main(void) {
    test_careful_buys_dip(); test_careful_takes_profit(); test_careful_cuts_loss();
    test_trend_buys_two_rises(); test_trend_sells_on_fall(); test_random_never_breaks_rules();
    test_bots_differ(); test_init_all_names_and_state(); test_trend_does_not_pile_in();
    test_careful_no_rebuy_same_day();
    printf("test_bots: ALL PASSED\n");
    return 0;
}
