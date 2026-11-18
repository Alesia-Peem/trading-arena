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

typedef struct {
    unsigned long seed;
    Market market;
    Portfolio you;
    Bot bots[NUM_BOTS];

    /* What each bot did on the latest day: shares bought (+) or sold (-) per stock. */
    long bot_trades[NUM_BOTS][NUM_STOCKS];
} Game;

/* Start a fresh game. The same seed always gives the same game. */
void game_new(Game *g, unsigned long seed);

/* 1 once the last day has been reached. */
int game_is_over(const Game *g);

/* Move to the next day: prices change, then every bot trades.
   Does nothing if the game is already over. */
void game_next_day(Game *g);

#endif
