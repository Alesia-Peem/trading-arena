#ifndef GAME_H
#define GAME_H

#include "market.h"
#include "portfolio.h"
#include "bots.h"

/*
 * One whole game: the market, you, and the 3 bots.
 *
 * Both front ends use this:
 *   - main.c   (the terminal version, ./arena)
 *   - server.c (the web version, ./arena-server)
 * so the rules are written once and behave the same everywhere.
 */

#define NUM_PLAYERS (NUM_BOTS + 1)   /* you + the bots; player 0 is always you */
#define MAX_TRADES  200              /* the trade history keeps this many of your trades */

/* One of your trades, for the history list and the chart marks. */
typedef struct {
    int  day;       /* the day it happened */
    int  stock;     /* which stock (0-4) */
    long qty;       /* shares: + = bought, - = sold */
    long price;     /* price per share, in cents */
    long fee;       /* fee paid, in cents */
} Trade;

typedef struct {
    unsigned long seed;
    Market market;
    Portfolio you;
    Bot bots[NUM_BOTS];

    /* What each bot did on the latest day: shares bought (+) or sold (-) per stock. */
    long bot_trades[NUM_BOTS][NUM_STOCKS];

    /* Your trades, oldest first. Once MAX_TRADES is reached you can still trade,
       but new trades are not added to the history. */
    Trade trades[MAX_TRADES];
    int   trade_count;

    /* The race: every player's net worth at the end of each day.
       worth_history[0] is you, worth_history[1..3] are the bots. */
    long worth_history[NUM_PLAYERS][MAX_HISTORY];
    int  worth_len;
} Game;

/* Start a fresh game. The same seed always gives the same game. */
void game_new(Game *g, unsigned long seed);

/* Buy or sell for you at today's price, and add the trade to the history.
   Returns a TRADE_ code from portfolio.h. */
int game_buy(Game *g, int stock, long qty);
int game_sell(Game *g, int stock, long qty);

/* 1 once the last day has been reached. */
int game_is_over(const Game *g);

/* Move to the next day: prices change, then every bot trades.
   Does nothing if the game is already over. */
void game_next_day(Game *g);

#endif
