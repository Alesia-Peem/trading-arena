#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include "market.h"
#include "portfolio.h"

/* Everything that prints to the screen: tables, charts and the leaderboard. */

#define CHART_ROWS 10     /* height of a price chart, in lines */

/*
 * Turn cents into readable money text:
 *     123456789 -> "1,234,567.89"
 *     5         -> "0.05"
 *     -5        -> "-0.05"
 * `out` is always ended with '\0'. If it is too small, the text is cut short.
 */
void format_money(long cents, char *out, size_t n);

/*
 * Draw a price chart made of '*' into `buf`.
 *   - One column per day, one '*' per column.
 *   - The highest price is on the top row, the lowest on the bottom row.
 *   - If every price is the same, all '*' go on the middle row.
 *   - Each row is `len` characters followed by '\n'.
 *
 * Returns how many '*' were drawn (= len).
 * Returns 0 and leaves `buf` empty if len or rows is 0 or less, or if `buf` is too small.
 */
int render_chart(const long *hist, int len, int rows, char *buf, size_t n);

void print_market(const Market *m);

/* Price chart with prices on the left, day numbers along the bottom, and a
   summary line (start, now, % change, high, low). Uses render_chart() inside. */
void print_chart(const Stock *s);
void print_portfolio(const Portfolio *p, const Market *m);

/*
 * Sort players from richest to poorest.
 * After this, order[0] is the index of the richest player in ps[].
 * Players with the same net worth stay in their original order.
 */
void rank_by_value(const Portfolio *ps[], int n, const Market *m, int order[]);

void print_leaderboard(const Portfolio *ps[], int n, const Market *m);

/* ---------- End-of-game lesson ---------- */

/* Percent change from `from` to `to`, in basis points (0 if `from` is 0 or less). */
long change_bp(long from, long to);

/* What `cash` would be worth now if you had spent it all on stock `s` on day 0
   (fee included) and never traded again. */
long hold_value(const Stock *s, long cash);

/* Index of the stock that went up the most / down the most since day 0. */
int best_stock(const Market *m);
int worst_stock(const Market *m);

/* The lesson: how you did against the market, against holding cash,
   and against buying the best stock on day 0 and holding it. */
void print_lesson(const Market *m, const Portfolio *you);

#endif
