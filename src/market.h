#ifndef MARKET_H
#define MARKET_H

/*
 * The stock market: 5 stocks whose prices move randomly each day.
 *
 * Units used everywhere in this program:
 *   - Money is stored in CENTS as a whole number.  5000 means $50.00.
 *     (Whole numbers never have rounding errors, unlike float/double.)
 *   - Percent changes are stored in BASIS POINTS ("bp").  1 bp = 0.01%,
 *     so 100 bp = 1%, and 250 bp = 2.5%.
 */

#define NUM_STOCKS  5
#define NUM_DAYS    30
#define MAX_HISTORY (NUM_DAYS + 1)   /* starting price + one price per day */
#define PRICE_FLOOR 100L             /* a price never drops below $1.00 */

typedef struct {
    char symbol[6];                  /* short code, e.g. "GRF" */
    char name[32];                   /* full name, e.g. "Golden Rice Farms" */
    long price;                      /* today's price, in cents */
    int  volatility_bp;              /* biggest random move in one day, in basis points */
    long history[MAX_HISTORY];       /* every price so far; history[0] is the starting price */
    int  history_len;                /* how many prices are stored in history[] */
} Stock;

typedef struct {
    Stock stocks[NUM_STOCKS];
    int   day;                       /* 0 before the first day, NUM_DAYS at the end */
    char  news[128];                 /* today's headline, or "" if there is none */
} Market;

/* Set up all stocks at their starting prices, day 0. */
void market_init(Market *m);

/* Move `price` by `change_bp` basis points. Never returns less than PRICE_FLOOR. */
long apply_change(long price, int change_bp);

/* Go forward one day: move every price, maybe add a news event, save the prices to history. */
void market_step(Market *m);

/* Average of every price in the stock's history (rounded down). */
long market_average(const Stock *s);

#endif
