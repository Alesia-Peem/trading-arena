# Stock Trading Arena — Design

CPE101 team project (financial systems area), written in C, due about 2026-12-07 (three weeks from 2026-11-16).

## Intent and why

Young people lose real money on trading apps and crypto because they've never felt how risky markets are. Stock Trading Arena is a console game that lets a player trade made-up stocks for 30 simulated days against three computer traders. They can feel price swings, news shocks and the cost of bad decisions without losing real money. The team picked this idea because the usual student projects (budget tracker, quiz) felt boring.

## Goals

- A playable console game: 30 trading days, 5 made-up stocks, and a starting cash balance of 10,000.00.
- Prices move randomly every day, and occasionally a news event hits one stock hard.
- Three computer traders, each with a visibly different strategy, trade in the same market.
- An ASCII price chart for any stock.
- An end-of-game leaderboard ranked by net worth (cash plus the value of shares held).
- A live demo that's fun to watch and easy for a grader to understand.
- Work that splits cleanly between four teammates.

## Non-goals

- Saving or loading a game, user accounts and high-score files.
- Real market data or internet access.
- Graphics beyond plain text, colours and libraries outside the C standard library.
- Short selling, loans, dividends and trading fees.

## Anti-goals (a failure even if it runs)

- The game crashes, or loops forever, on bad keyboard input such as letters where a number is expected.
- Money values show rounding errors like 10499.999999.
- The bots behave identically, so the "AI rivals" idea isn't visible in the demo.
- The code is one giant `main.c` that teammates can't work on in parallel.

## Constraints

- C99, built with `gcc` using only the standard library. It must compile with `-Wall -Wextra` and no warnings.
- It runs in a terminal on macOS, Linux and Windows (MinGW).
- The code is written at the level of an introductory C course: arrays, functions, structs and loops. No dynamic memory is needed.
- The deadline is three weeks, with a team of about four.

## Approaches considered

1. **Single file, `double` money.** This is the fastest start. Teammates would conflict on one file, and floating-point money shows rounding errors (an anti-goal).
2. **Several modules, money stored as whole cents in `long`, with a seeded random-number generator.** Each teammate owns a module. Money is always exact, and a fixed seed makes tests and the demo repeatable. **This was chosen.**
3. **A hard-coded price script** (the same prices every game). This is easy to test, but there's no replay value and the bots have nothing interesting to react to.

## Decided design (Claude's calls, open to the team's correction)

- **Money:** a `long` in cents, shown as `1,234.56`. Percentage changes are in basis points (100 bp = 1%).
- **Stocks** (made-up), with starting price and daily volatility:

  | Symbol | Name | Start | Daily range |
  |---|---|---|---|
  | GRF | Golden Rice Farms | 50.00 | ±2% |
  | BSR | Bangkok Sky Rail | 120.00 | ±3% |
  | MNGO | Mango Energy | 80.00 | ±4% |
  | DTEC | Durian Tech | 200.00 | ±6% |
  | LUCK | Lucky Coin Crypto | 15.00 | ±12% |

- **Daily price move:** a uniform random change between `-volatility` and `+volatility` basis points. A price never drops below 1.00.
- **News:** each day there's a 15% chance that one random stock gets a shock of ±10% to ±25%, with a headline printed on screen.
- **Bots:**
  - *Careful Kim* buys a stock when it's at least 5% below its average price so far, using 20% of their cash. They sell when the stock is up 10% or down 8% from their buy price.
  - *Trend Tom* buys after two rising days, using 25% of their cash. They sell everything in a stock after any falling day.
  - *Random Rita* does something random each day: nothing, a random buy of up to 10 shares, or a random sell.
- **Player menu each day:** view market, view chart, buy, sell, view portfolio, next day, quit.
- **Random seed:** taken from the clock by default. `./arena 42` uses seed 42, so a rehearsed demo can be repeated exactly.
- **Order limit:** at most 1,000,000 shares per order, so the maths can't overflow.

## Left to the builder

The exact wording of on-screen text, the news headline list, the layout of the market table, and how the chart's axes are labelled.
