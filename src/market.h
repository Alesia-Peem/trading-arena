#ifndef MARKET_H
#define MARKET_H

#define NUM_STOCKS 5
#define NUM_DAYS 30
#define MAX_HISTORY (NUM_DAYS + 1)
#define PRICE_FLOOR 100L

typedef struct {
    char symbol[6]; char name[32];
    long price;                 /* cents */
    int volatility_bp;          /* max daily move */
    long history[MAX_HISTORY];  /* history[0] = starting price */
    int history_len;
} Stock;

typedef struct {
    Stock stocks[NUM_STOCKS];
    int day;                    /* 0 before the first step, NUM_DAYS at the end */
    char news[128];             /* "" when there's no news today */
} Market;

void market_init(Market *m);
long apply_change(long price, int change_bp);   /* never below PRICE_FLOOR */
void market_step(Market *m);                    /* move prices, maybe news, append history, day++ */
long market_average(const Stock *s);            /* integer mean of history */

#endif
