#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include "market.h"

/*
 * A portfolio = one player's cash plus how many shares they own of each stock.
 * You and every bot each have one.
 */

#define START_CASH    1000000L   /* $10,000.00 in cents */
#define MAX_ORDER_QTY 1000000    /* the most shares you can buy or sell in one order */
#define FEE_BP        15         /* every trade pays a 0.15% fee, like a Thai online broker */

/* What portfolio_buy / portfolio_sell return. */
enum {
    TRADE_OK        =  0,   /* it worked */
    TRADE_BAD_INPUT = -1,   /* no such stock, or a silly quantity like 0 or -5 */
    TRADE_NO_CASH   = -2,   /* not enough money to buy that many */
    TRADE_NO_SHARES = -3,   /* trying to sell more shares than you own */
};

typedef struct {
    char name[24];
    long cash;                   /* in cents */
    long shares[NUM_STOCKS];     /* shares[i] = how many shares of stock i you own */
    long fees_paid;              /* all trading fees paid so far, in cents */
    long cost_basis[NUM_STOCKS]; /* what you paid (fees included) for the shares you hold now */
} Portfolio;

/* Empty portfolio: just a name and some cash. Long names are cut to 23 letters. */
void portfolio_init(Portfolio *p, const char *name, long cash);

/* The fee for a trade worth `value` cents: FEE_BP of it, rounded UP to the next cent. */
long trade_fee(long value);

/* Buy or sell `qty` shares of stock number `stock` at today's price, paying the fee.
   Buying needs enough cash for the shares AND the fee. Returns a TRADE_ code. */
int portfolio_buy(Portfolio *p, const Market *m, int stock, long qty);
int portfolio_sell(Portfolio *p, const Market *m, int stock, long qty);

/* Average price you paid per share of `stock` you hold now, fees included (0 if none). */
long average_cost(const Portfolio *p, int stock);

/* How much you are up (+) or down (-) on the shares of `stock` you hold now,
   if you valued them at today's price. */
long profit_loss(const Portfolio *p, const Market *m, int stock);

/* Net worth: cash + (shares x today's price) for every stock. */
long portfolio_value(const Portfolio *p, const Market *m);

/* How many shares at `price` cents you can buy with `cash` cents, fee included. */
long shares_affordable(long cash, long price);

/* How many shares of `stock` you can afford right now, fee included (0 if `stock` isn't valid). */
long max_affordable(const Portfolio *p, const Market *m, int stock);

#endif
