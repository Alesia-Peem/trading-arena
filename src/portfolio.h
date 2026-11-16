#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include "market.h"

#define START_CASH 1000000L
#define MAX_ORDER_QTY 1000000

enum { TRADE_OK = 0, TRADE_BAD_INPUT = -1, TRADE_NO_CASH = -2, TRADE_NO_SHARES = -3 };

typedef struct { char name[24]; long cash; long shares[NUM_STOCKS]; } Portfolio;

void portfolio_init(Portfolio *p, const char *name, long cash);
int  portfolio_buy (Portfolio *p, const Market *m, int stock, long qty);
int  portfolio_sell(Portfolio *p, const Market *m, int stock, long qty);
long portfolio_value(const Portfolio *p, const Market *m);            /* cash + shares x current price */
long max_affordable(const Portfolio *p, const Market *m, int stock);  /* cash / price; 0 if bad stock */

#endif
