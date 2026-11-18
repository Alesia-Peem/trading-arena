#include <stdio.h>
#include <string.h>
#include "display.h"

#define MAX_PLAYERS 64    /* the leaderboard handles at most this many players */

/* ---------- Money and percent text ---------- */

/* Make a number positive without overflowing.
   (Writing -cents would overflow for the most negative long, so we do -(cents + 1) + 1.) */
static unsigned long long absolute_value(long cents) {
    if (cents >= 0) {
        return (unsigned long long)cents;
    }
    return (unsigned long long)(-(cents + 1)) + 1ULL;
}

void format_money(long cents, char *out, size_t n) {
    if (n == 0) {
        return;
    }

    unsigned long long amount = absolute_value(cents);
    unsigned long long dollars = amount / 100;
    int pennies = (int)(amount % 100);

    /* Step 1: write the dollar digits BACKWARDS (ones digit first),
       putting a comma before every group of 3.  1234567 -> "765,432,1" */
    char backwards[48];
    int length = 0;
    int digit_count = 0;
    do {
        if (digit_count > 0 && digit_count % 3 == 0) {
            backwards[length++] = ',';
        }
        backwards[length++] = (char)('0' + (int)(dollars % 10));
        dollars /= 10;
        digit_count++;
    } while (dollars > 0);

    if (cents < 0) {
        backwards[length++] = '-';
    }

    /* Step 2: flip it around.  "765,432,1" -> "1,234,567" */
    char dollar_text[48];
    for (int i = 0; i < length; i++) {
        dollar_text[i] = backwards[length - 1 - i];
    }
    dollar_text[length] = '\0';

    /* Step 3: add the cents.  snprintf cuts the text short if `out` is too small. */
    snprintf(out, n, "%s.%02d", dollar_text, pennies);
}

/* Basis points -> percent text:  235 -> "+2.35%",  -80 -> "-0.80%",  0 -> "0.00%" */
static void format_percent(long long bp, char *out, size_t n) {
    long value = (long)bp;
    long size = value < 0 ? -value : value;

    const char *sign = "";
    if (value > 0) {
        sign = "+";
    } else if (value < 0) {
        sign = "-";
    }

    snprintf(out, n, "%s%ld.%02ld%%", sign, size / 100, size % 100);
}

/* ---------- Chart ---------- */

static void find_low_and_high(const long *prices, int count, long *low, long *high) {
    *low = prices[0];
    *high = prices[0];
    for (int i = 1; i < count; i++) {
        if (prices[i] < *low) {
            *low = prices[i];
        }
        if (prices[i] > *high) {
            *high = prices[i];
        }
    }
}

/* Which row (0 = top) a price belongs on. The highest price goes on row 0,
   the lowest on the last row, everything else in between. */
static int row_for_price(long price, long low, long high, int rows) {
    if (high == low) {
        return rows / 2;   /* flat line: use the middle row */
    }
    long long distance_from_top = (long long)high - price;
    long long full_range = (long long)high - low;
    return (int)(distance_from_top * (rows - 1) / full_range);
}

int render_chart(const long *hist, int len, int rows, char *buf, size_t n) {
    if (len <= 0 || rows <= 0 || n == 0) {
        if (n > 0) {
            buf[0] = '\0';
        }
        return 0;
    }

    int row_width = len + 1;   /* `len` characters plus '\n' */
    size_t space_needed = (size_t)rows * (size_t)row_width + 1;   /* + 1 for '\0' */
    if (n < space_needed) {
        buf[0] = '\0';
        return 0;
    }

    /* Start with a blank grid: every row is spaces followed by '\n'. */
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < len; col++) {
            buf[row * row_width + col] = ' ';
        }
        buf[row * row_width + len] = '\n';
    }
    buf[rows * row_width] = '\0';

    /* Put one '*' in each column, at the height of that day's price. */
    long low, high;
    find_low_and_high(hist, len, &low, &high);
    for (int col = 0; col < len; col++) {
        int row = row_for_price(hist[col], low, high, rows);
        buf[row * row_width + col] = '*';
    }

    return len;
}

/* ---------- Screens ---------- */

/* Percent change from yesterday's price to today's, in basis points. */
static long long change_since_yesterday_bp(const Stock *s) {
    if (s->history_len < 2) {
        return 0;   /* day 0: no yesterday yet */
    }
    long long yesterday = s->history[s->history_len - 2];
    if (yesterday <= 0) {
        return 0;
    }
    return ((long long)s->price - yesterday) * 10000 / yesterday;
}

void print_market(const Market *m) {
    printf("\n=== Market, day %d ===\n", m->day);
    printf("%-6s %-24s %12s %9s\n", "SYM", "NAME", "PRICE", "CHANGE");

    for (int i = 0; i < NUM_STOCKS; i++) {
        const Stock *stock = &m->stocks[i];
        char price[32];
        char change[32];

        format_money(stock->price, price, sizeof price);
        format_percent(change_since_yesterday_bp(stock), change, sizeof change);
        printf("%-6s %-24s %12s %9s\n", stock->symbol, stock->name, price, change);
    }
}

/* ---------- Printing the chart nicely ---------- */

#define CHART_COLUMN_WIDTH 2    /* each day is drawn 2 characters wide, so it's easier to see */
#define CHART_DAY_LABEL_EVERY 5 /* put a day number under every 5th day */
#define CHART_LINE_MAX (CHART_COLUMN_WIDTH * MAX_HISTORY + 8)

/* Remove spaces at the end of a string (they're invisible anyway). */
static void trim_trailing_spaces(char *text) {
    size_t len = strlen(text);
    while (len > 0 && text[len - 1] == ' ') {
        len--;
    }
    text[len] = '\0';
}

/* The price written at the left of a chart row: the top edge of that row's price band.
   Row 0 gets the highest price, the last row gets the lowest. */
static long row_label_price(long low, long high, int row, int rows) {
    return high - (long)((long long)(high - low) * row / (rows - 1));
}

/* Print one row of the grid, with every character repeated CHART_COLUMN_WIDTH times. */
static void print_chart_row(const char *grid_row, int days) {
    char line[CHART_LINE_MAX];
    int length = 0;
    for (int day = 0; day < days; day++) {
        for (int k = 0; k < CHART_COLUMN_WIDTH; k++) {
            line[length++] = grid_row[day];
        }
    }
    line[length] = '\0';
    trim_trailing_spaces(line);
    printf("%s\n", line);
}

/* The bottom of the chart:
         +--------------------
          0         5         10   day        */
static void print_day_axis(int days) {
    printf("%10s +", "");
    for (int i = 0; i < days * CHART_COLUMN_WIDTH; i++) {
        putchar('-');
    }
    printf("\n");

    char labels[CHART_LINE_MAX];
    int width = days * CHART_COLUMN_WIDTH + 2;   /* + 2 so a last 2-digit number still fits */
    memset(labels, ' ', (size_t)width);
    labels[width] = '\0';

    for (int day = 0; day < days; day += CHART_DAY_LABEL_EVERY) {
        char number[8];
        int digits = snprintf(number, sizeof number, "%d", day);
        memcpy(&labels[day * CHART_COLUMN_WIDTH], number, (size_t)digits);
    }
    trim_trailing_spaces(labels);
    printf("%10s  %s   day\n", "", labels);
}

/* "Start 80.00   Now 58.89 (-26.39%)   High 80.00   Low 53.21" */
static void print_chart_summary(const Stock *s, long low, long high) {
    long start = s->history[0];
    long now = s->history[s->history_len - 1];
    long long change_bp = 0;
    if (start > 0) {
        change_bp = ((long long)now - start) * 10000 / start;
    }

    char start_text[32], now_text[32], change_text[32], high_text[32], low_text[32];
    format_money(start, start_text, sizeof start_text);
    format_money(now, now_text, sizeof now_text);
    format_percent(change_bp, change_text, sizeof change_text);
    format_money(high, high_text, sizeof high_text);
    format_money(low, low_text, sizeof low_text);

    printf("Start %s   Now %s (%s)   High %s   Low %s\n",
           start_text, now_text, change_text, high_text, low_text);
}

void print_chart(const Stock *s) {
    int days = s->history_len;
    if (days <= 0) {
        return;
    }

    /* Step 1: build the grid of '*' (one character per day). */
    char grid[CHART_ROWS * (MAX_HISTORY + 1) + 1];
    if (render_chart(s->history, days, CHART_ROWS, grid, sizeof grid) == 0) {
        return;
    }

    long low, high;
    find_low_and_high(s->history, days, &low, &high);

    printf("\n%s - %s (day 0 to %d)\n\n", s->symbol, s->name, days - 1);

    /* Step 2: print each row with its price on the left. */
    int grid_row_width = days + 1;   /* days characters + '\n' */
    for (int row = 0; row < CHART_ROWS; row++) {
        char label[32] = "";
        int flat = (high == low);
        if (!flat) {
            format_money(row_label_price(low, high, row, CHART_ROWS), label, sizeof label);
        } else if (row == CHART_ROWS / 2) {
            format_money(high, label, sizeof label);   /* flat line: label only its row */
        }
        printf("%10s |", label);
        print_chart_row(&grid[row * grid_row_width], days);
    }

    /* Step 3: the day numbers and the summary line. */
    print_day_axis(days);
    print_chart_summary(s, low, high);
}

void print_portfolio(const Portfolio *p, const Market *m) {
    char money[32];

    printf("\n--- %s's portfolio ---\n", p->name);

    for (int i = 0; i < NUM_STOCKS; i++) {
        if (p->shares[i] > 0) {
            format_money(m->stocks[i].price, money, sizeof money);
            printf("%-6s %8ld shares @ %s\n", m->stocks[i].symbol, p->shares[i], money);
        }
    }

    format_money(p->cash, money, sizeof money);
    printf("Cash:      %s\n", money);

    format_money(portfolio_value(p, m), money, sizeof money);
    printf("Net worth: %s\n", money);
}

/* ---------- Leaderboard ---------- */

void rank_by_value(const Portfolio *ps[], int n, const Market *m, int order[]) {
    if (n > MAX_PLAYERS) {
        n = MAX_PLAYERS;
    }

    long worth[MAX_PLAYERS];
    for (int i = 0; i < n; i++) {
        order[i] = i;
        worth[i] = portfolio_value(ps[i], m);
    }

    /* Insertion sort, richest first. Like sorting cards in your hand: take each
       player and slide them left past everyone poorer than them.
       Using "<" (not "<=") means equal players never swap, so ties keep their order. */
    for (int i = 1; i < n; i++) {
        int player = order[i];
        int j = i - 1;
        while (j >= 0 && worth[order[j]] < worth[player]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = player;
    }
}

void print_leaderboard(const Portfolio *ps[], int n, const Market *m) {
    if (n > MAX_PLAYERS) {
        n = MAX_PLAYERS;
    }

    int order[MAX_PLAYERS];
    rank_by_value(ps, n, m, order);

    printf("\n=== Leaderboard ===\n");
    printf("%-4s %-16s %16s %10s\n", "RANK", "NAME", "NET WORTH", "PROFIT");

    for (int rank = 0; rank < n; rank++) {
        const Portfolio *p = ps[order[rank]];
        long worth = portfolio_value(p, m);
        long long profit_bp = ((long long)worth - START_CASH) * 10000 / START_CASH;

        char money[32];
        char profit[32];
        format_money(worth, money, sizeof money);
        format_percent(profit_bp, profit, sizeof profit);
        printf("%-4d %-16s %16s %10s\n", rank + 1, p->name, money, profit);
    }
}
