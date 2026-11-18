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
#define INDEX_START 100000L          /* the market index starts at 1,000.00 (stored like money, x100) */

typedef struct {
    char symbol[6];                  /* short code, e.g. "GRF" */
    char name[32];                   /* full name, e.g. "Golden Rice Farms" */
    long price;                      /* today's price, in cents */
    int  volatility_bp;              /* biggest random move of its own in one day, in basis points */
    int  beta_pct;                   /* how much it follows the whole market: 100 = the same, 200 = twice as much */
    long history[MAX_HISTORY];       /* every price so far; history[0] is the starting price */
    int  history_len;                /* how many prices are stored in history[] */
} Stock;

typedef struct {
    Stock stocks[NUM_STOCKS];
    int   day;                       /* 0 before the first day, NUM_DAYS at the end */
    char  news[128];                 /* today's headline, or "" if there is none */

    /* The whole market: every day has one "mood" that pushes all stocks the same way. */
    int   market_move_bp;            /* today's market move, in basis points */
    int   market_event;              /* -1 = crash day, +1 = rally day, 0 = a normal day */
    long  index;                     /* market index, like the SET index: starts at INDEX_START */
    long  index_history[MAX_HISTORY];
    int   index_history_len;
} Market;

/* Set up all stocks at their starting prices, day 0. */
void market_init(Market *m);

/* Move `price` by `change_bp` basis points. Never returns less than PRICE_FLOOR. */
long apply_change(long price, int change_bp);

/* A stock's move for the day: the market's move scaled by its beta, plus its own random move. */
int stock_move_bp(int market_bp, int beta_pct, int own_bp);

/* Go forward one day: pick the market's mood, move every price and the index,
   maybe add news, and save everything to history. */
void market_step(Market *m);

/* Average of every price in the stock's history (rounded down). */
long market_average(const Stock *s);

#endif
