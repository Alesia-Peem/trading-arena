#ifndef BOTS_H
#define BOTS_H

#include "market.h"
#include "portfolio.h"

#define NUM_BOTS 3

typedef enum { BOT_CAREFUL, BOT_TREND, BOT_RANDOM } BotStrategy;

typedef struct {
    Portfolio p;
    BotStrategy strategy;
    long buy_price[NUM_STOCKS];
} Bot;

void bot_init(Bot *b, const char *name, BotStrategy s);   /* START_CASH, buy_price all 0 */
void bot_act(Bot *b, const Market *m);                    /* called once per day, after market_step */
void bots_init_all(Bot bots[NUM_BOTS]);                   /* Careful Kim, Trend Tom, Random Rita */

#endif
