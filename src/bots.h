#ifndef BOTS_H
#define BOTS_H

#include "market.h"
#include "portfolio.h"

/*
 * Three computer players that trade against you, each with a simple strategy:
 *
 *   Careful Kim  - buys when a stock is cheaper than usual,
 *                  sells after a +10% gain or a -8% loss.
 *   Trend Tom    - buys after a stock rises two days in a row,
 *                  sells as soon as it falls.
 *   Random Rita  - each day, randomly does nothing, buys a little, or sells a little.
 */

#define NUM_BOTS 3

typedef enum {
    BOT_CAREFUL,
    BOT_TREND,
    BOT_RANDOM,
} BotStrategy;

typedef struct {
    Portfolio   p;                       /* the bot's cash and shares */
    BotStrategy strategy;
    long        buy_price[NUM_STOCKS];   /* price the bot last paid for each stock (0 = never bought) */
} Bot;

/* A bot with START_CASH, no shares, and the given strategy. */
void bot_init(Bot *b, const char *name, BotStrategy s);

/* Create Careful Kim, Trend Tom and Random Rita (in that order). */
void bots_init_all(Bot bots[NUM_BOTS]);

/* Let the bot make its trades for today. Call once per day, after market_step(). */
void bot_act(Bot *b, const Market *m);

#endif
