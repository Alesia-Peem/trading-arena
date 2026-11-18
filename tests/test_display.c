#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/market.h"
#include "../src/portfolio.h"
#include "../src/display.h"

/* How many times character c appears in text s. */
static int count_char(const char *s, char c) {
    int count = 0;
    for (; *s != '\0'; s++) {
        if (*s == c) count++;
    }
    return count;
}

/* ---------- Money ---------- */

static void test_money_format(void) {
    char b[32];
    format_money(0, b, sizeof b);          assert(strcmp(b, "0.00") == 0);
    format_money(5, b, sizeof b);          assert(strcmp(b, "0.05") == 0);
    format_money(1000000, b, sizeof b);    assert(strcmp(b, "10,000.00") == 0);
    format_money(123456789, b, sizeof b);  assert(strcmp(b, "1,234,567.89") == 0);
}

static void test_money_negative_and_small_buffer(void) {
    char b[32];
    format_money(-5, b, sizeof b);         assert(strcmp(b, "-0.05") == 0);
    format_money(-123456, b, sizeof b);    assert(strcmp(b, "-1,234.56") == 0);

    char tiny[5];
    format_money(123456789, tiny, sizeof tiny);
    assert(strlen(tiny) < sizeof tiny);    /* cut short, but still ends with '\0' */

    format_money(1, b, 0);                 /* size 0: must not write anything or crash */
}

/* ---------- Chart ---------- */

static void test_chart_counts_points(void) {
    long prices[] = {100, 300, 200, 500};
    char buf[1024];

    assert(render_chart(prices, 4, CHART_ROWS, buf, sizeof buf) == 4);
    assert(count_char(buf, '*') == 4);
    assert(count_char(buf, '\n') == CHART_ROWS);
}

static void test_chart_flat_prices(void) {
    long prices[] = {700, 700, 700};
    char buf[1024];
    assert(render_chart(prices, 3, CHART_ROWS, buf, sizeof buf) == 3);
}

static void test_chart_single_point(void) {
    long prices[] = {700};
    char buf[1024];
    assert(render_chart(prices, 1, CHART_ROWS, buf, sizeof buf) == 1);
}

static void test_chart_geometry(void) {
    long prices[] = {100, 500, 300};
    char buf[1024];
    int row_width = 4;   /* 3 columns + '\n' */

    assert(render_chart(prices, 3, CHART_ROWS, buf, sizeof buf) == 3);
    assert(strlen(buf) == (size_t)CHART_ROWS * row_width);

    /* 500 is the highest: top row, column 1 */
    assert(buf[1] == '*');

    /* 100 is the lowest: bottom row, column 0 */
    char *bottom_row = buf + (CHART_ROWS - 1) * row_width;
    assert(bottom_row[0] == '*');

    /* 300: (500 - 300) * 9 / 400 = 4, so row 4, column 2 */
    char *row_4 = buf + 4 * row_width;
    assert(row_4[2] == '*');
}

static void test_chart_flat_middle_row(void) {
    long prices[] = {700, 700};
    char buf[1024];
    int row_width = 3;   /* 2 columns + '\n' */

    render_chart(prices, 2, CHART_ROWS, buf, sizeof buf);

    char *middle_row = buf + (CHART_ROWS / 2) * row_width;
    assert(middle_row[0] == '*' && middle_row[1] == '*');
}

static void test_chart_bad_args(void) {
    long prices[] = {1, 2};
    char buf[8];   /* too small for a 10-row chart */
    buf[0] = 'x';

    assert(render_chart(prices, 2, CHART_ROWS, buf, sizeof buf) == 0);
    assert(render_chart(prices, 0, CHART_ROWS, buf, sizeof buf) == 0);
}

/* ---------- Leaderboard ---------- */

static void test_rank(void) {
    Market m;
    market_init(&m);
    Portfolio a, b, c;
    portfolio_init(&a, "A", 100);
    portfolio_init(&b, "B", 300);
    portfolio_init(&c, "C", 200);
    const Portfolio *players[] = {&a, &b, &c};
    int order[3];

    rank_by_value(players, 3, &m, order);

    assert(order[0] == 1 && order[1] == 2 && order[2] == 0);   /* B, C, A */
}

static void test_rank_ties_stable(void) {
    Market m;
    market_init(&m);
    Portfolio a, b, c;
    portfolio_init(&a, "A", 200);
    portfolio_init(&b, "B", 200);
    portfolio_init(&c, "C", 200);
    const Portfolio *players[] = {&a, &b, &c};
    int order[3];

    rank_by_value(players, 3, &m, order);

    assert(order[0] == 0 && order[1] == 1 && order[2] == 2);   /* unchanged */
}

/* Just checks that printing doesn't crash. */
static void test_print_smoke(void) {
    Market m;
    market_init(&m);
    Portfolio you, bot;
    portfolio_init(&you, "You", START_CASH);
    portfolio_init(&bot, "Bot", 500000);
    portfolio_buy(&you, &m, 0, 5);
    const Portfolio *players[] = {&you, &bot};

    print_market(&m);
    print_chart(&m.stocks[0]);
    print_portfolio(&you, &m);
    print_leaderboard(players, 2, &m);
}

/* ---------- End-of-game lesson ---------- */

static void test_change_bp(void) {
    assert(change_bp(10000, 10500) == 500);     /* +5% */
    assert(change_bp(5000, 4000) == -2000);     /* -20% */
    assert(change_bp(0, 5) == 0);               /* no start price: no change */
}

static void test_hold_value(void) {
    Stock s = {0};
    s.history[0] = 5000;          /* $50 on day 0 */
    s.history_len = 1;
    s.price = 6000;               /* $60 now */
    /* $10,000 buys 199 shares on day 0 (the fee rules out 200), leaving $35.07 cash. */
    assert(hold_value(&s, 1000000) == 3507 + 199 * 6000);
}

static void test_best_and_worst_stock(void) {
    Market m;
    market_init(&m);
    m.stocks[3].price = m.stocks[3].history[0] * 3 / 2;   /* DTEC +50% */
    m.stocks[4].price = m.stocks[4].history[0] / 2;       /* LUCK -50% */
    assert(best_stock(&m) == 3);
    assert(worst_stock(&m) == 4);
}

int main(void) {
    test_money_format();
    test_money_negative_and_small_buffer();
    test_chart_counts_points();
    test_chart_flat_prices();
    test_chart_single_point();
    test_chart_geometry();
    test_chart_flat_middle_row();
    test_chart_bad_args();
    test_rank();
    test_rank_ties_stable();
    test_print_smoke();

    test_change_bp();
    test_hold_value();
    test_best_and_worst_stock();

    printf("test_display: ALL PASSED\n");
    return 0;
}
