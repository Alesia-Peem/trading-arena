#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include "market.h"
#include "portfolio.h"

#define CHART_ROWS 10

/* 123456789 -> "1,234,567.89", 5 -> "0.05", -5 -> "-0.05". Always NUL-terminates (truncates if n is small). */
void format_money(long cents, char *out, size_t n);

/* One column per day, '*' at each price's row, top row = max, bottom = min. Every row is len chars
   wide and ends with '\n'. Returns the number of '*' drawn (== len), or 0 if len <= 0, rows <= 0
   or the buffer is too small (then buf is left as an empty string when n > 0). When max == min
   every '*' goes on the middle row. */
int render_chart(const long *hist, int len, int rows, char *buf, size_t n);

void print_market(const Market *m);
void print_chart(const Stock *s);
void print_portfolio(const Portfolio *p, const Market *m);

/* order[0] = index of the richest; ties keep the original order. */
void rank_by_value(const Portfolio *ps[], int n, const Market *m, int order[]);
void print_leaderboard(const Portfolio *ps[], int n, const Market *m);

#endif
