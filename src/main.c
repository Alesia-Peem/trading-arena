/*
 * Stock Trading Arena
 *
 * You start with $10,000 and trade 5 stocks for 30 days against 3 computer bots.
 * Whoever has the highest net worth at the end wins.
 *
 * Run:  ./arena          (random game)
 *       ./arena 42       (same game every time you use seed 42)
 */

#include <stdio.h>
#include <time.h>
#include "game.h"
#include "display.h"
#include "input.h"
#include "scores.h"

#define MAX_SEED 2147483647L

/* The numbers the player types in the menu. */
enum {
    MENU_QUIT      = 0,
    MENU_MARKET    = 1,
    MENU_CHART     = 2,
    MENU_BUY       = 3,
    MENU_SELL      = 4,
    MENU_PORTFOLIO = 5,
    MENU_NEXT_DAY  = 6,
};

/* Note: many functions below return 0 when input ended (EOF), which means "stop the game". */

/* ---------- Showing things ---------- */

static void show_day(const Market *m, const Portfolio *you) {
    printf("\n=== Day %d/%d ===\n", m->day, NUM_DAYS);
    if (m->news[0] != '\0') {
        printf("NEWS: %s\n", m->news);
    }

    print_market(m);

    char cash[32];
    char worth[32];
    format_money(you->cash, cash, sizeof cash);
    format_money(portfolio_value(you, m), worth, sizeof worth);
    printf("Cash: $%s   Net worth: $%s\n", cash, worth);
}

static void print_menu(void) {
    printf("\n1 Market  2 Chart  3 Buy  4 Sell  5 Portfolio  6 Next day  0 Quit\n");
}

/* ---------- Player actions ---------- */

/* Ask which stock (1-5) and store it as an index (0-4). */
static int pick_stock(long *index) {
    long number;
    if (!read_long("Stock (1-5): ", 1, NUM_STOCKS, &number)) {
        return 0;
    }
    *index = number - 1;
    return 1;
}

static int do_chart(const Market *m) {
    long s;
    if (!pick_stock(&s)) {
        return 0;
    }
    print_chart(&m->stocks[s]);
    return 1;
}

static int do_buy(Portfolio *you, const Market *m) {
    long s;
    if (!pick_stock(&s)) {
        return 0;
    }
    const char *symbol = m->stocks[s].symbol;

    long can_afford = max_affordable(you, m, (int)s);
    if (can_afford <= 0) {
        printf("You can't afford any %s right now.\n", symbol);
        return 1;
    }
    if (can_afford > MAX_ORDER_QTY) {
        can_afford = MAX_ORDER_QTY;
    }

    char prompt[96];
    snprintf(prompt, sizeof prompt, "Quantity to buy (max you can afford: %ld): ", can_afford);

    long qty;
    if (!read_long(prompt, 1, MAX_ORDER_QTY, &qty)) {
        return 0;
    }

    switch (portfolio_buy(you, m, (int)s, qty)) {
        case TRADE_OK:        printf("Bought %ld %s.\n", qty, symbol);         break;
        case TRADE_NO_CASH:   printf("Not enough cash for that order.\n");    break;
        case TRADE_BAD_INPUT: printf("Invalid order.\n");                     break;
        default:              printf("Trade failed.\n");                      break;
    }
    return 1;
}

static int do_sell(Portfolio *you, const Market *m) {
    long s;
    if (!pick_stock(&s)) {
        return 0;
    }
    const char *symbol = m->stocks[s].symbol;

    if (you->shares[s] <= 0) {
        printf("You don't own any %s.\n", symbol);
        return 1;
    }

    char prompt[96];
    snprintf(prompt, sizeof prompt, "Quantity to sell (you own %ld): ", you->shares[s]);

    long qty;
    if (!read_long(prompt, 1, MAX_ORDER_QTY, &qty)) {
        return 0;
    }

    switch (portfolio_sell(you, m, (int)s, qty)) {
        case TRADE_OK:        printf("Sold %ld %s.\n", qty, symbol);           break;
        case TRADE_NO_SHARES: printf("You don't own that many shares.\n");    break;
        case TRADE_BAD_INPUT: printf("Invalid order.\n");                     break;
        default:              printf("Trade failed.\n");                      break;
    }
    return 1;
}

/* ---------- Bots ---------- */

/* Print what each bot did today, e.g. "Trend Tom: bought 14 BSR, sold 3 GRF". */
static void print_bot_trades(const Game *g) {
    printf("\n-- Bots (day %d) --\n", g->market.day);

    for (int i = 0; i < NUM_BOTS; i++) {
        printf("%s:", g->bots[i].p.name);

        int traded = 0;
        for (int s = 0; s < NUM_STOCKS; s++) {
            long change = g->bot_trades[i][s];
            if (change == 0) {
                continue;
            }
            const char *separator = traded ? "," : "";
            const char *symbol = g->market.stocks[s].symbol;
            if (change > 0) {
                printf("%s bought %ld %s", separator, change, symbol);
            } else {
                printf("%s sold %ld %s", separator, -change, symbol);
            }
            traded = 1;
        }

        if (!traded) {
            printf(" no trades");
        }
        printf("\n");
    }
}

/* ---------- Game ---------- */

/* Use the seed typed after ./arena if there is one, otherwise the current time. */
static unsigned long choose_seed(int argc, char **argv) {
    long seed;
    if (argc > 1 && parse_long_in_range(argv[1], 0, MAX_SEED, &seed)) {
        return (unsigned long)seed;
    }
    return (unsigned long)time(NULL);
}

static void next_day(Game *g) {
    game_next_day(g);
    show_day(&g->market, &g->you);
    print_bot_trades(g);
}

static void show_final_standings(const Game *g) {
    const Market *m = &g->market;
    printf("\n=== Final standings (day %d/%d) ===\n", m->day, NUM_DAYS);

    /* Put you and all bots in one list for the leaderboard. You are first. */
    const Portfolio *players[NUM_BOTS + 1];
    players[0] = &g->you;
    for (int i = 0; i < NUM_BOTS; i++) {
        players[i + 1] = &g->bots[i].p;
    }
    print_leaderboard(players, NUM_BOTS + 1, m);

    if (portfolio_value(&g->you, m) < START_CASH) {
        printf("Markets are risky - that's the point.\n");
    } else {
        printf("Did skill or luck win this one? Try another seed.\n");
    }
}

/* ---------- Leaderboard file ---------- */

/* After a finished game: add your result to the all-time board and save it. */
static void save_result(const Game *g, ScoreEntry board[], int *n) {
    if (!game_is_over(g)) {
        printf("\nYou quit early, so this game isn't saved to the leaderboard.\n");
        return;
    }

    ScoreEntry e;
    snprintf(e.name, sizeof e.name, "%s", g->you.name);
    e.net_worth = portfolio_value(&g->you, &g->market);
    e.seed = g->seed;
    time_t now = time(NULL);
    strftime(e.date, sizeof e.date, "%Y-%m-%d", localtime(&now));

    int place = scores_add(board, n, MAX_SCORES, &e);
    if (!scores_save(SCORES_FILE, board, *n)) {
        printf("Warning: could not save %s\n", SCORES_FILE);
    }

    if (place > 0) {
        printf("\nYou placed #%d on the all-time top %d!\n", place, MAX_SCORES);
    } else {
        printf("\nNot in the top %d this time.\n", MAX_SCORES);
    }
    scores_print(board, *n);
}

int main(int argc, char **argv) {
    static Game game;   /* static: keeps this big struct off the stack */
    game_new(&game, choose_seed(argc, argv));
    printf("Stock Trading Arena\nSeed: %lu\n", game.seed);

    char name[sizeof game.you.name] = "Player";
    if (!read_name("Your name: ", name, sizeof name)) {
        snprintf(name, sizeof name, "Player");   /* input ended: keep the default */
    }
    snprintf(game.you.name, sizeof game.you.name, "%s", name);

    static ScoreEntry board[MAX_SCORES];
    int board_count = scores_load(SCORES_FILE, board, MAX_SCORES);
    scores_print(board, board_count);

    Market *m = &game.market;
    Portfolio *you = &game.you;

    show_day(m, you);

    int running = 1;
    while (running) {
        print_menu();

        long choice;
        if (!read_long("Choice: ", MENU_QUIT, MENU_NEXT_DAY, &choice)) {
            break;   /* input ended */
        }

        switch (choice) {
            case MENU_QUIT:
                running = 0;
                break;
            case MENU_MARKET:
                show_day(m, you);
                break;
            case MENU_CHART:
                running = do_chart(m);
                break;
            case MENU_BUY:
                running = do_buy(you, m);
                break;
            case MENU_SELL:
                running = do_sell(you, m);
                break;
            case MENU_PORTFOLIO:
                print_portfolio(you, m);
                break;
            case MENU_NEXT_DAY:
                next_day(&game);
                if (game_is_over(&game)) {
                    running = 0;   /* last day reached: game over */
                }
                break;
        }
    }

    show_final_standings(&game);
    save_result(&game, board, &board_count);
    return 0;
}
