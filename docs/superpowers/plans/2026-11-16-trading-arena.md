# Stock Trading Arena Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C console game where a player trades 5 made-up stocks for 30 days against 3 computer traders, then sees a leaderboard ranked by net worth.

**Architecture:** There are six small modules, each with its own `.h`/`.c` pair and test file: `rng`, `market`, `portfolio`, `bots`, `display` and `input`. `main.c` wires them together in the game loop. All money is stored as `long` cents. Randomness comes from one seeded generator, so tests and demos can be repeated exactly.

**Tech Stack:** C99, `gcc`, `make`, C standard library only. Tests are plain C programs that use `assert` and are run by `make test`.

**Spec:** `docs/superpowers/specs/2026-11-16-trading-arena-design.md`

## Global Constraints

- C99, standard library only, compiled with `gcc -std=c99 -Wall -Wextra -Werror`.
- Money is always a `long` in cents. Never use `float` or `double` for money.
- Percentage changes are `int` basis points (100 bp = 1%).
- `NUM_STOCKS 5`, `NUM_DAYS 30`, starting cash `1000000` cents (10,000.00), price floor `100` cents (1.00).
- `MAX_ORDER_QTY 1000000` shares per order.
- News chance is 15% per day, with a shock between 1000 and 2500 bp, up or down.
- No `malloc`. Use fixed-size arrays and structs only.
- All randomness goes through `rng.h`. Never call `rand()` directly.

## Review Focus

1. **Letters or empty lines typed where a number is expected.** Re-prompt; never crash or loop forever. EOF (Ctrl-D) quits cleanly. *Test: Task 6 `test_parse_rejects_garbage`.*
2. **Buying more than you can afford, selling more than you own, or a quantity of 0 or less.** Refuse with a message, and leave cash and shares unchanged. *Test: Task 3 `test_buy_rejects_*`, `test_sell_rejects_*`.*
3. **A huge order quantity (for example 999999999).** Refuse; no `long` overflow. *Test: Task 3 `test_buy_rejects_huge_qty`.*
4. **A chart where every price is equal, or there's only one day of history.** No divide-by-zero, and the chart still draws. *Test: Task 5 `test_chart_flat_prices`, `test_chart_single_point`.*
5. **Repeated crashes driving a price toward zero.** The price stays at least 1.00 forever. *Test: Task 2 `test_price_floor_holds_over_many_days`.*

## File Structure

```
trading-arena/
  Makefile
  src/
    rng.h / rng.c             seeded random numbers
    market.h / market.c       stocks, daily price moves, news
    portfolio.h / portfolio.c cash, shares, buy/sell, net worth
    bots.h / bots.c           three computer trader strategies
    display.h / display.c     money formatting, ASCII chart, ranking
    input.h / input.c         safe number parsing and prompts
    main.c                    menu and game loop
  tests/
    test_rng.c test_market.c test_portfolio.c test_bots.c test_display.c test_input.c
```

**Suggested team split:** Member A takes Tasks 1–2, B takes Task 3, C takes Task 4 and D takes Task 5. Everyone works on Task 6 together. Commit the headers from Tasks 1–5 on the first day, so everyone can code against them in parallel.

**Suggested schedule:** Week 1 covers Tasks 1–3 and the headers. Week 2 covers Tasks 4–5. Week 3 covers Task 6, play-testing, and rehearsing the demo with a fixed seed.

---

### Task 1: Project setup and random numbers

**Files:**
- Create: `Makefile`, `src/rng.h`, `src/rng.c`, `tests/test_rng.c`, `.gitignore` (ignores `arena` and `tests/*_test`)

**Interfaces:**
- Produces:
  - `void rng_seed(unsigned long seed);`
  - `unsigned long rng_next(void);`: a 32-bit linear congruential generator, `state = state * 1103515245 + 12345`, returning `(state / 65536) % 32768`.
  - `int rng_range(int lo, int hi);`: a uniform integer in `[lo, hi]` inclusive. If `lo > hi`, return `lo`.
  - Makefile targets: `make` builds `./arena` from `src/*.c`; `make test` builds `tests/<name>_test` for each `tests/test_<name>.c`, linking every `src/*.c` except `main.c`, runs each one, and stops at the first failure. `make clean` removes the build output.

- [ ] **Step 1: `git init` in `trading-arena/` and add the Makefile and `.gitignore`**

- [ ] **Step 2: Write the failing tests in `tests/test_rng.c`**

```c
#include <assert.h>
#include <stdio.h>
#include "../src/rng.h"

static void test_same_seed_same_sequence(void) {
    rng_seed(42); unsigned long a1 = rng_next(), a2 = rng_next();
    rng_seed(42); assert(rng_next() == a1); assert(rng_next() == a2);
}
static void test_range_stays_inside(void) {
    rng_seed(7);
    for (int i = 0; i < 10000; i++) { int r = rng_range(-300, 300); assert(r >= -300 && r <= 300); }
}
static void test_range_hits_both_ends(void) {
    rng_seed(1); int lo = 0, hi = 0;
    for (int i = 0; i < 10000; i++) { int r = rng_range(1, 3); if (r == 1) lo = 1; if (r == 3) hi = 1; }
    assert(lo && hi);
}
static void test_range_single_value(void) { assert(rng_range(5, 5) == 5); }

int main(void) {
    test_same_seed_same_sequence(); test_range_stays_inside();
    test_range_hits_both_ends(); test_range_single_value();
    printf("test_rng: ALL PASSED\n"); return 0;
}
```

- [ ] **Step 3: Run `make test`.** Expected: a compile or link error, because `rng_seed` is undefined.
- [ ] **Step 4: Implement `rng.c` using the formula in the Interfaces block.**
- [ ] **Step 5: Run `make test`.** Expected: `test_rng: ALL PASSED`.
- [ ] **Step 6: Commit** with `git add -A && git commit -m "feat: project setup and seeded rng"`.

---

### Task 2: Market (stocks, daily moves, news)

**Files:**
- Create: `src/market.h`, `src/market.c`, `tests/test_market.c`

**Interfaces:**
- Consumes: `rng_range`, `rng_seed` (Task 1).
- Produces:
```c
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
void market_init(Market *m);              /* stocks from the spec table, in that order, day 0, history_len 1 */
long apply_change(long price, int change_bp);  /* price*(10000+bp)/10000, never below PRICE_FLOOR */
void market_step(Market *m);              /* move every price, maybe add news, append history, day++ */
long market_average(const Stock *s);      /* integer mean of history[0..history_len-1] */
```

- [ ] **Step 1: Write the failing tests in `tests/test_market.c`**

```c
static void test_init_matches_spec(void) {
    Market m; market_init(&m);
    assert(m.day == 0);
    assert(strcmp(m.stocks[0].symbol, "GRF") == 0 && m.stocks[0].price == 5000 && m.stocks[0].volatility_bp == 200);
    assert(strcmp(m.stocks[4].symbol, "LUCK") == 0 && m.stocks[4].price == 1500 && m.stocks[4].volatility_bp == 1200);
    assert(m.stocks[0].history_len == 1 && m.stocks[0].history[0] == 5000);
}
static void test_apply_change_up_and_down(void) {
    assert(apply_change(10000, 500) == 10500);
    assert(apply_change(10000, -250) == 9750);
}
static void test_apply_change_floor(void) { assert(apply_change(150, -5000) == PRICE_FLOOR); }
static void test_step_advances_day_and_history(void) {
    rng_seed(3); Market m; market_init(&m); market_step(&m);
    assert(m.day == 1 && m.stocks[2].history_len == 2 && m.stocks[2].history[1] == m.stocks[2].price);
}
static void test_price_floor_holds_over_many_days(void) {
    rng_seed(9); Market m; market_init(&m);
    for (int d = 0; d < 2000; d++) {          /* deliberately run past NUM_DAYS; append is guarded */
        market_step(&m);
        for (int i = 0; i < NUM_STOCKS; i++) assert(m.stocks[i].price >= PRICE_FLOOR);
    }
}
static void test_news_appears_sometimes(void) {
    rng_seed(11); int seen = 0;
    for (int g = 0; g < 20; g++) { Market m; market_init(&m);
        for (int d = 0; d < NUM_DAYS; d++) { market_step(&m); if (m.news[0]) seen++; } }
    assert(seen > 30 && seen < 150);   /* about 15% of 600 days, around 90 */
}
static void test_average(void) {
    Stock s = {0}; s.history[0] = 100; s.history[1] = 200; s.history[2] = 301; s.history_len = 3;
    assert(market_average(&s) == 200);
}
```
(Add the `main` and includes in the same style as Task 1.)

- [ ] **Step 2: Run `make test`.** Expected: FAIL to compile, because `market_init` is undefined.
- [ ] **Step 3: Implement `market.c`.** In `market_step`, each stock gets `rng_range(-vol, vol)`. Then, if `rng_range(1,100) <= 15`, pick `rng_range(0, NUM_STOCKS-1)` and apply a shock of `rng_range(1000,2500)` bp, negative when `rng_range(0,1) == 0`. Write a headline into `news` with `snprintf`. Choose 4 or more templates by direction, for example `"%s factory fire! Shares tumble"`. Clear `news` on days without news. Guard the history append with `history_len < MAX_HISTORY`. Steps after day `NUM_DAYS` still move prices but don't append history.
- [ ] **Step 4: Run `make test`.** Expected: `test_market: ALL PASSED`.
- [ ] **Step 5: Commit** with `git add -A && git commit -m "feat: market prices and news"`.

---

### Task 3: Portfolio (cash, shares, buy/sell, net worth)

**Files:**
- Create: `src/portfolio.h`, `src/portfolio.c`, `tests/test_portfolio.c`

**Interfaces:**
- Consumes: `Market`, `NUM_STOCKS`, `market_init` (Task 2).
- Produces:
```c
#define START_CASH 1000000L
#define MAX_ORDER_QTY 1000000
enum { TRADE_OK = 0, TRADE_BAD_INPUT = -1, TRADE_NO_CASH = -2, TRADE_NO_SHARES = -3 };
typedef struct { char name[24]; long cash; long shares[NUM_STOCKS]; } Portfolio;
void portfolio_init(Portfolio *p, const char *name, long cash);
int  portfolio_buy (Portfolio *p, const Market *m, int stock, long qty);
int  portfolio_sell(Portfolio *p, const Market *m, int stock, long qty);
long portfolio_value(const Portfolio *p, const Market *m);   /* cash + shares × current price */
long max_affordable(const Portfolio *p, const Market *m, int stock);  /* cash / price */
```

- [ ] **Step 1: Write the failing tests** (the market comes from `market_init`, so GRF is 5000 cents)

```c
static void setup(Portfolio *p, Market *m) { market_init(m); portfolio_init(p, "You", START_CASH); }
static void test_buy_ok(void)  { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,10) == TRADE_OK); assert(p.cash == 950000 && p.shares[0] == 10); }
static void test_sell_ok(void) { Portfolio p; Market m; setup(&p,&m); portfolio_buy(&p,&m,0,10);
    assert(portfolio_sell(&p,&m,0,4) == TRADE_OK); assert(p.cash == 970000 && p.shares[0] == 6); }
static void test_buy_rejects_no_cash(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,201) == TRADE_NO_CASH); assert(p.cash == START_CASH && p.shares[0] == 0); }
static void test_buy_rejects_bad_qty_and_stock(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,0) == TRADE_BAD_INPUT); assert(portfolio_buy(&p,&m,0,-5) == TRADE_BAD_INPUT);
    assert(portfolio_buy(&p,&m,5,1) == TRADE_BAD_INPUT); assert(portfolio_buy(&p,&m,-1,1) == TRADE_BAD_INPUT); }
static void test_buy_rejects_huge_qty(void) { Portfolio p; Market m; setup(&p,&m);
    assert(portfolio_buy(&p,&m,0,999999999L) == TRADE_BAD_INPUT); assert(p.cash == START_CASH); }
static void test_sell_rejects_more_than_owned(void) { Portfolio p; Market m; setup(&p,&m); portfolio_buy(&p,&m,0,3);
    assert(portfolio_sell(&p,&m,0,4) == TRADE_NO_SHARES); assert(p.shares[0] == 3); }
static void test_value_counts_shares_at_current_price(void) { Portfolio p; Market m; setup(&p,&m);
    portfolio_buy(&p,&m,0,10); m.stocks[0].price = 6000; assert(portfolio_value(&p,&m) == 950000 + 60000); }
static void test_max_affordable(void) { Portfolio p; Market m; setup(&p,&m); assert(max_affordable(&p,&m,0) == 200); }
```

- [ ] **Step 2: Run `make test`.** Expected: FAIL, because `portfolio_init` is undefined.
- [ ] **Step 3: Implement `portfolio.c`.** Check the stock index and quantity (`1..MAX_ORDER_QTY`) first, then check cash or shares, then change state. A failed trade must never change state.
- [ ] **Step 4: Run `make test`.** Expected: `test_portfolio: ALL PASSED`.
- [ ] **Step 5: Commit** with `git add -A && git commit -m "feat: portfolio buy/sell/value"`.

---

### Task 4: Bots (three strategies)

**Files:**
- Create: `src/bots.h`, `src/bots.c`, `tests/test_bots.c`

**Interfaces:**
- Consumes: `Portfolio`, `portfolio_buy`, `portfolio_sell`, `max_affordable` (Task 3), and `Market`, `market_average` (Task 2), and `rng_range` (Task 1).
- Produces:
```c
#define NUM_BOTS 3
typedef enum { BOT_CAREFUL, BOT_TREND, BOT_RANDOM } BotStrategy;
typedef struct { Portfolio p; BotStrategy strategy; long buy_price[NUM_STOCKS]; } Bot;
void bot_init(Bot *b, const char *name, BotStrategy s);   /* START_CASH, buy_price all 0 */
void bot_act(Bot *b, const Market *m);                    /* called once per day, after market_step */
void bots_init_all(Bot bots[NUM_BOTS]);   /* "Careful Kim", "Trend Tom", "Random Rita" in that order */
```
- Rules (from the spec). The quantity bought is `(cash × percent / 100) / price`; skip the buy when that comes to 0. Set `buy_price[i]` to the price on every buy.
  - **CAREFUL:** for each stock with 0 shares, buy with 20% of cash if `price * 100 <= market_average * 95`. For each stock held, sell all if `price * 100 >= buy_price * 110` or `price * 100 <= buy_price * 92`.
  - **TREND:** with history `h` and length `n`, buy with 25% of cash if `n >= 3 && h[n-1] > h[n-2] && h[n-2] > h[n-3]`. Sell all of a held stock if `n >= 2 && h[n-1] < h[n-2]`.
  - **RANDOM:** `rng_range(0,2)`. On 0, do nothing. On 1, buy `rng_range(1,10)` of a random stock, capped at `max_affordable`. On 2, sell `rng_range(1, owned)` of a random stock it owns, if it owns any.

- [ ] **Step 1: Write the failing tests.** Build each scenario by setting `m.stocks[i].history`, `history_len` and `price` directly.

```c
static void test_careful_buys_dip(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    Stock *s = &m.stocks[1]; s->history[0]=10000; s->history[1]=10000; s->history[2]=9000; s->history_len=3; s->price=9000;
    bot_act(&b,&m); assert(b.p.shares[1] > 0 && b.buy_price[1] == 9000); }
static void test_careful_takes_profit(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    b.p.shares[0] = 10; b.buy_price[0] = 5000; m.stocks[0].price = 5500;
    bot_act(&b,&m); assert(b.p.shares[0] == 0); }
static void test_careful_cuts_loss(void) { Market m; market_init(&m); Bot b; bot_init(&b,"K",BOT_CAREFUL);
    b.p.shares[0] = 10; b.buy_price[0] = 5000; m.stocks[0].price = 4600;
    bot_act(&b,&m); assert(b.p.shares[0] == 0); }
static void test_trend_buys_two_rises(void) { Market m; market_init(&m); Bot b; bot_init(&b,"T",BOT_TREND);
    Stock *s=&m.stocks[2]; s->history[0]=8000; s->history[1]=8100; s->history[2]=8200; s->history_len=3; s->price=8200;
    bot_act(&b,&m); assert(b.p.shares[2] > 0); }
static void test_trend_sells_on_fall(void) { Market m; market_init(&m); Bot b; bot_init(&b,"T",BOT_TREND);
    b.p.shares[2]=5; Stock *s=&m.stocks[2]; s->history[0]=8000; s->history[1]=7900; s->history_len=2; s->price=7900;
    bot_act(&b,&m); assert(b.p.shares[2] == 0); }
static void test_random_never_breaks_rules(void) { rng_seed(5); Market m; market_init(&m); Bot b; bot_init(&b,"R",BOT_RANDOM);
    for (int d=0; d<NUM_DAYS; d++) { market_step(&m); bot_act(&b,&m);
        assert(b.p.cash >= 0); for (int i=0;i<NUM_STOCKS;i++) assert(b.p.shares[i] >= 0); } }
static void test_bots_differ(void) { rng_seed(42); Market m; market_init(&m); Bot bots[NUM_BOTS]; bots_init_all(bots);
    for (int d=0; d<NUM_DAYS; d++) { market_step(&m); for (int i=0;i<NUM_BOTS;i++) bot_act(&bots[i],&m); }
    long v0=portfolio_value(&bots[0].p,&m), v1=portfolio_value(&bots[1].p,&m), v2=portfolio_value(&bots[2].p,&m);
    assert(!(v0==v1 && v1==v2)); }
```

- [ ] **Step 2: Run `make test`.** Expected: FAIL, because `bot_init` is undefined.
- [ ] **Step 3: Implement `bots.c`** using the rules above, with one static function per strategy.
- [ ] **Step 4: Run `make test`.** Expected: `test_bots: ALL PASSED`.
- [ ] **Step 5: Commit** with `git add -A && git commit -m "feat: three bot strategies"`.

---

### Task 5: Display (money, chart, leaderboard)

**Files:**
- Create: `src/display.h`, `src/display.c`, `tests/test_display.c`

**Interfaces:**
- Consumes: `Market`, `Stock` (Task 2), `Portfolio`, `portfolio_value` (Task 3).
- Produces:
```c
#define CHART_ROWS 10
void format_money(long cents, char *out, size_t n);   /* 123456789 -> "1,234,567.89", 5 -> "0.05" */
int  render_chart(const long *hist, int len, int rows, char *buf, size_t n);
     /* one column per day, '*' at each price's row, top row = max, bottom = min; every row ends with '\n';
        returns the number of '*' drawn (equal to len); every '*' goes on the middle row when max == min */
void print_market(const Market *m);   /* table: symbol, name, price, change vs yesterday in % */
void print_chart(const Stock *s);     /* title, max/min labels, render_chart output */
void print_portfolio(const Portfolio *p, const Market *m);
void rank_by_value(const Portfolio *ps[], int n, const Market *m, int order[]);
     /* order[0] = index of the richest; ties keep the original order */
void print_leaderboard(const Portfolio *ps[], int n, const Market *m);  /* ranks, names, net worth, % profit */
```

- [ ] **Step 1: Write the failing tests**

```c
static void test_money_format(void) { char b[32];
    format_money(0,b,sizeof b); assert(strcmp(b,"0.00")==0);
    format_money(5,b,sizeof b); assert(strcmp(b,"0.05")==0);
    format_money(1000000,b,sizeof b); assert(strcmp(b,"10,000.00")==0);
    format_money(123456789,b,sizeof b); assert(strcmp(b,"1,234,567.89")==0); }
static int count(const char *s, char c) { int k=0; for(;*s;s++) if(*s==c) k++; return k; }
static void test_chart_counts_points(void) { long h[] = {100,300,200,500}; char buf[1024];
    assert(render_chart(h,4,CHART_ROWS,buf,sizeof buf) == 4 && count(buf,'*') == 4 && count(buf,'\n') == CHART_ROWS); }
static void test_chart_flat_prices(void) { long h[] = {700,700,700}; char buf[1024];
    assert(render_chart(h,3,CHART_ROWS,buf,sizeof buf) == 3); }
static void test_chart_single_point(void) { long h[] = {700}; char buf[1024];
    assert(render_chart(h,1,CHART_ROWS,buf,sizeof buf) == 1); }
static void test_rank(void) { Market m; market_init(&m); Portfolio a,b,c;
    portfolio_init(&a,"A",100); portfolio_init(&b,"B",300); portfolio_init(&c,"C",200);
    const Portfolio *ps[] = {&a,&b,&c}; int o[3]; rank_by_value(ps,3,&m,o);
    assert(o[0]==1 && o[1]==2 && o[2]==0); }
```

- [ ] **Step 2: Run `make test`.** Expected: FAIL, because `format_money` is undefined.
- [ ] **Step 3: Implement `display.c`.** For the chart, a price's row is `(max - price) * (rows - 1) / (max - min)`, with the zero case handled first. The `print_*` functions just call `printf` around the testable helpers and aren't unit-tested.
- [ ] **Step 4: Run `make test`.** Expected: `test_display: ALL PASSED`.
- [ ] **Step 5: Commit** with `git add -A && git commit -m "feat: money formatting, chart, leaderboard"`.

---

### Task 6: Input handling and the game loop

**Files:**
- Create: `src/input.h`, `src/input.c`, `tests/test_input.c`, `src/main.c`

**Interfaces:**
- Consumes: everything above.
- Produces:
```c
int parse_long_in_range(const char *s, long min, long max, long *out);
    /* 1 if s (ignoring surrounding spaces and the trailing newline) is a whole number in [min,max], else 0 */
int read_long(const char *prompt, long min, long max, long *out);
    /* fgets loop, re-prompts on bad input, returns 0 on EOF */
```
- `main.c` behaviour:
  - Seed from `argv[1]` if it's given and valid, otherwise from `time(NULL)`. Print the seed so a good game can be replayed.
  - Each day, show the day number (`Day N/30`), today's news if there is any, the market table, and the player's cash and net worth.
  - Menu: `1 Market  2 Chart  3 Buy  4 Sell  5 Portfolio  6 Next day  0 Quit`.
  - Buy and sell ask for the stock (1–5) and then the quantity. For buys, show the maximum you can afford. Print a clear message for each `TRADE_*` result.
  - On "Next day", run `market_step`, then every `bot_act`, then print a one-line summary of what each bot did. Track this by comparing each bot's shares before and after.
  - After day 30, or on Quit, print the leaderboard (the player plus 3 bots) and a one-line lesson. If the player lost, show "Markets are risky — that's the point." If they won, show "Did skill or luck win this one? Try another seed."
  - EOF at any prompt ends the game with the leaderboard.

- [ ] **Step 1: Write the failing tests in `tests/test_input.c`**

```c
static void test_parse_ok(void) { long v;
    assert(parse_long_in_range("42\n",1,100,&v) && v==42);
    assert(parse_long_in_range("  7  ",1,100,&v) && v==7); }
static void test_parse_rejects_garbage(void) { long v;
    assert(!parse_long_in_range("abc",1,100,&v)); assert(!parse_long_in_range("",1,100,&v));
    assert(!parse_long_in_range("12abc",1,100,&v)); assert(!parse_long_in_range("\n",1,100,&v)); }
static void test_parse_rejects_out_of_range(void) { long v;
    assert(!parse_long_in_range("0",1,100,&v)); assert(!parse_long_in_range("101",1,100,&v));
    assert(!parse_long_in_range("99999999999999999999",1,100,&v)); }
```

- [ ] **Step 2: Run `make test`.** Expected: FAIL, because `parse_long_in_range` is undefined.
- [ ] **Step 3: Implement `input.c`** using `strtol` with an `errno` and end-pointer check.
- [ ] **Step 4: Run `make test`.** Expected: all six test programs print `ALL PASSED`.
- [ ] **Step 5: Implement `main.c`** as described above.
- [ ] **Step 6: Build and smoke-test.** Run `make && printf '6\n%.0s' $(seq 30) | ./arena 42`. Expected: it reaches `Day 30/30` and prints a leaderboard with 4 names, and nothing goes wrong. Then run `printf 'abc\n3\n1\nxyz\n5\n6\n' | ./arena 42`. Expected: a re-prompt after each bad entry, and a successful buy of 5 GRF.
- [ ] **Step 7: Play one full game by hand with seed 42**, and write down the seed and moments that look good for the presentation demo.
- [ ] **Step 8: Commit** with `git add -A && git commit -m "feat: game loop and safe input"`.
