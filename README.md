# Stock Trading Arena

A CPE101 project in C. You trade five made-up stocks for 30 days against three computer traders, and the game shows how risky trading is without you losing real money.

## How the market works

- **The whole market has a mood each day.** Every stock follows it by its *beta*: Golden Rice Farms moves half as much as the market, Lucky Coin Crypto twice as much.
- **Crashes and rallies.** About 1 day in 25 the whole market falls 4-8% or rises 3-6%.
- **Company news.** Some days one company jumps or drops 10-25% on a headline.
- **Trading fees.** Every buy and sell costs 0.15%, like a Thai online broker.
- **A market index** starts at 1,000.00, so you can check whether you beat the market.

At the end, the game ranks you against the bots, then shows what you would have had if you had kept your cash, followed the market index, or bought the best stock on day 0 and held it. Your result goes into an all-time top 10, saved in `scores.txt`.

## Build and play

You need `gcc` and `make`. The game uses only the C standard library.

```bash
make            # build the terminal game
./arena         # play a random game
./arena 42      # the same game every time you use seed 42
make test       # run the unit tests
```

There is also a browser version, built from the same C game code. It needs macOS or Linux:

```bash
make web        # build and start the server, then open http://localhost:8080
```

## Code layout

Each module is a `.h` file (what it offers) plus a `.c` file (how it works).

| Module | What it does |
|---|---|
| `rng` | Random numbers from a seed, so a game can be replayed |
| `market` | Prices, market mood, crashes and rallies, news, the market index |
| `portfolio` | Cash, shares, buying and selling, fees, net worth |
| `bots` | The three computer traders and their strategies |
| `game` | One whole game: the market, you and the bots, day by day |
| `display` | Money formatting, tables, charts, the leaderboard, the end-of-game lesson |
| `input` | Safe keyboard input: numbers and names |
| `scores` | The all-time top 10, read from and written to `scores.txt` |
| `main.c` | The terminal game |
| `server.c` + `web/` | The browser version |

## The leaderboard file

`scores.txt` keeps the best 10 finished games, one per line, best first:

```
name,net_worth_in_cents,seed,YYYY-MM-DD
Peem,1254300,42,2026-12-01
```

You can open and edit it in any text editor. Broken lines are skipped, and lines in the wrong order are sorted when the game loads them.
