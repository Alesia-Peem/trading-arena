#include <stdio.h>
#include <time.h>
#include "rng.h"
#include "market.h"
#include "portfolio.h"
#include "bots.h"
#include "display.h"
#include "input.h"

static void show_day(const Market *m, const Portfolio *you) {
    char cash[32], worth[32];
    printf("\n=== Day %d/%d ===\n", m->day, NUM_DAYS);
    if (m->news[0]) printf("NEWS: %s\n", m->news);
    print_market(m);
    format_money(you->cash, cash, sizeof cash);
    format_money(portfolio_value(you, m), worth, sizeof worth);
    printf("Cash: $%s   Net worth: $%s\n", cash, worth);
}

static void print_menu(void) {
    printf("\n1 Market  2 Chart  3 Buy  4 Sell  5 Portfolio  6 Next day  0 Quit\n");
}

/* returns 0 on EOF */
static int pick_stock(long *idx) {
    long c;
    if (!read_long("Stock (1-5): ", 1, NUM_STOCKS, &c)) return 0;
    *idx = c - 1;
    return 1;
}

static int do_buy(Portfolio *you, const Market *m) {
    long s, qty, maxq;
    char prompt[96];
    if (!pick_stock(&s)) return 0;
    maxq = max_affordable(you, m, (int)s);
    if (maxq <= 0) { printf("You can't afford any %s right now.\n", m->stocks[s].symbol); return 1; }
    if (maxq > MAX_ORDER_QTY) maxq = MAX_ORDER_QTY;
    snprintf(prompt, sizeof prompt, "Quantity to buy (max you can afford: %ld): ", maxq);
    if (!read_long(prompt, 1, MAX_ORDER_QTY, &qty)) return 0;
    switch (portfolio_buy(you, m, (int)s, qty)) {
        case TRADE_OK: printf("Bought %ld %s.\n", qty, m->stocks[s].symbol); break;
        case TRADE_NO_CASH: printf("Not enough cash for that order.\n"); break;
        case TRADE_BAD_INPUT: printf("Invalid order.\n"); break;
        default: printf("Trade failed.\n"); break;
    }
    return 1;
}

static int do_sell(Portfolio *you, const Market *m) {
    long s, qty;
    char prompt[96];
    if (!pick_stock(&s)) return 0;
    if (you->shares[s] <= 0) { printf("You don't own any %s.\n", m->stocks[s].symbol); return 1; }
    snprintf(prompt, sizeof prompt, "Quantity to sell (you own %ld): ", you->shares[s]);
    if (!read_long(prompt, 1, MAX_ORDER_QTY, &qty)) return 0;
    switch (portfolio_sell(you, m, (int)s, qty)) {
        case TRADE_OK: printf("Sold %ld %s.\n", qty, m->stocks[s].symbol); break;
        case TRADE_NO_SHARES: printf("You don't own that many shares.\n"); break;
        case TRADE_BAD_INPUT: printf("Invalid order.\n"); break;
        default: printf("Trade failed.\n"); break;
    }
    return 1;
}

static void bots_turn(Bot bots[NUM_BOTS], const Market *m) {
    int i, s;
    printf("\n-- Bots --\n");
    for (i = 0; i < NUM_BOTS; i++) {
        long before[NUM_STOCKS];
        char line[256];
        size_t len;
        int any = 0;
        for (s = 0; s < NUM_STOCKS; s++) before[s] = bots[i].p.shares[s];
        bot_act(&bots[i], m);
        len = (size_t)snprintf(line, sizeof line, "%s:", bots[i].p.name);
        for (s = 0; s < NUM_STOCKS && len < sizeof line; s++) {
            long d = bots[i].p.shares[s] - before[s];
            if (d == 0) continue;
            len += (size_t)snprintf(line + len, sizeof line - len, "%s %s %ld %s",
                                    any ? "," : "", d > 0 ? "bought" : "sold",
                                    d > 0 ? d : -d, m->stocks[s].symbol);
            any = 1;
        }
        if (any) printf("%s\n", line);
        else printf("%s held\n", line);
    }
}

int main(int argc, char **argv) {
    Market m;
    Portfolio you;
    Bot bots[NUM_BOTS];
    const Portfolio *all[NUM_BOTS + 1];
    long seed_val = 0, choice;
    unsigned long seed;
    int i, running = 1;

    if (argc > 1 && parse_long_in_range(argv[1], 0, 2147483647L, &seed_val)) seed = (unsigned long)seed_val;
    else seed = (unsigned long)time(NULL);
    rng_seed(seed);
    printf("Stock Trading Arena\nSeed: %lu\n", seed);

    market_init(&m);
    portfolio_init(&you, "You", START_CASH);
    bots_init_all(bots);
    market_step(&m);   /* day 1 */
    show_day(&m, &you);

    while (running) {
        print_menu();
        if (!read_long("Choice: ", 0, 6, &choice)) break;
        switch (choice) {
            case 0: running = 0; break;
            case 1: show_day(&m, &you); break;
            case 2: { long s; if (!pick_stock(&s)) running = 0; else print_chart(&m.stocks[s]); break; }
            case 3: if (!do_buy(&you, &m)) running = 0; break;
            case 4: if (!do_sell(&you, &m)) running = 0; break;
            case 5: print_portfolio(&you, &m); break;
            case 6:
                if (m.day >= NUM_DAYS) { running = 0; break; }
                market_step(&m);
                bots_turn(bots, &m);
                show_day(&m, &you);
                if (m.day >= NUM_DAYS) running = 0;
                break;
        }
    }

    printf("\n=== Final standings (day %d/%d) ===\n", m.day, NUM_DAYS);
    all[0] = &you;
    for (i = 0; i < NUM_BOTS; i++) all[i + 1] = &bots[i].p;
    print_leaderboard(all, NUM_BOTS + 1, &m);
    {
        int best = 0;
        long v, bv = -1;
        for (i = 0; i < NUM_BOTS + 1; i++) {
            v = portfolio_value(all[i], &m);
            if (v > bv) { bv = v; best = i; }
        }
        if (best == 0) printf("Did skill or luck win this one? Try another seed.\n");
        else printf("Markets are risky — that's the point.\n");
    }
    return 0;
}
