/*
 * Stock Trading Arena - web server
 *
 * A tiny HTTP server written with plain POSIX sockets. It runs the same game as
 * ./arena (through game.c) and lets a web page in your browser play it.
 *
 * Run:   ./arena-server          then open http://localhost:8080
 *        ./arena-server 9000     use another port
 *
 * What the browser can ask for:
 *   GET  /                      the web page   (web/index.html)
 *   GET  /style.css, /app.js    the page's style and code
 *   GET  /api/state             the whole game as JSON
 *   POST /api/new?seed=42       start a new game (seed is optional)
 *   POST /api/buy?stock=0&qty=5
 *   POST /api/sell?stock=0&qty=5
 *   POST /api/next              go to the next day
 *
 * Every /api answer has the same shape:
 *   { "ok": true/false, "message": "...", "state": { ...the whole game... } }
 *
 * Safety: the server only listens on 127.0.0.1 (this computer), and it only
 * serves the files listed in STATIC_FILES, so nobody can read other files.
 */

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "display.h"
#include "input.h"
#include "scores.h"

#define DEFAULT_PORT      8080
#define REQUEST_MAX       8192     /* biggest request we read */
#define RESPONSE_MAX      32768    /* biggest JSON answer we build */
#define MAX_SEED          2147483647L
#define READ_TIMEOUT_SECS 5        /* drop clients that connect but send nothing */

static Game game;   /* the one game being played */

/* The all-time top 10 (scores.txt), and whether this game's result is saved yet. */
static ScoreEntry board[MAX_SCORES];
static int board_count = 0;
static int result_saved = 0;
static int your_place = 0;   /* your place on the board once saved, 0 = didn't make it */

/* ======================================================================
 * Building text (a growing string with overflow protection)
 * ==================================================================== */

typedef struct {
    char   data[RESPONSE_MAX];
    size_t length;
    int    overflowed;     /* set to 1 if we ran out of room */
} Text;

static void text_add(Text *t, const char *format, ...) {
    if (t->overflowed) {
        return;
    }
    size_t room = sizeof t->data - t->length;

    va_list args;
    va_start(args, format);
    int written = vsnprintf(t->data + t->length, room, format, args);
    va_end(args);

    if (written < 0 || (size_t)written >= room) {
        t->overflowed = 1;
        return;
    }
    t->length += (size_t)written;
}

/* Add a JSON string with quotes, escaping characters that would break JSON. */
static void text_add_json_string(Text *t, const char *s) {
    text_add(t, "\"");
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            text_add(t, "\\%c", c);
        } else if (c < 0x20) {
            text_add(t, "\\u%04x", c);
        } else {
            text_add(t, "%c", c);
        }
    }
    text_add(t, "\"");
}

/* ======================================================================
 * The game as JSON
 * ==================================================================== */

static const char *BOT_STRATEGY_TEXT[NUM_BOTS] = {
    "Buys dips below the average, sells at +10% or -8%",
    "Buys after two up days, sells on the first down day",
    "Trades small random amounts",
};

static void add_shares_array(Text *t, const long shares[NUM_STOCKS]) {
    text_add(t, "[");
    for (int s = 0; s < NUM_STOCKS; s++) {
        text_add(t, "%s%ld", s > 0 ? "," : "", shares[s]);
    }
    text_add(t, "]");
}

static void add_portfolio(Text *t, const Portfolio *p, const Market *m) {
    text_add(t, "\"name\":");
    text_add_json_string(t, p->name);
    text_add(t, ",\"cash\":%ld,\"netWorth\":%ld,\"feesPaid\":%ld,\"shares\":",
             p->cash, portfolio_value(p, m), p->fees_paid);
    add_shares_array(t, p->shares);

    /* How many shares of each stock this player can buy right now, fee included.
       Sent from C so the page's Max button uses exactly the game's rule. */
    text_add(t, ",\"maxAffordable\":[");
    for (int s = 0; s < NUM_STOCKS; s++) {
        text_add(t, "%s%ld", s > 0 ? "," : "", max_affordable(p, m, s));
    }
    text_add(t, "]");
}

/* Per stock: what you paid for the shares you hold, the average price, and profit/loss. */
static void add_holdings_pl(Text *t, const Portfolio *p, const Market *m) {
    text_add(t, ",\"avgCost\":[");
    for (int s = 0; s < NUM_STOCKS; s++) {
        text_add(t, "%s%ld", s > 0 ? "," : "", average_cost(p, s));
    }
    text_add(t, "],\"profitLoss\":[");
    for (int s = 0; s < NUM_STOCKS; s++) {
        text_add(t, "%s%ld", s > 0 ? "," : "", profit_loss(p, m, s));
    }
    text_add(t, "],\"costBasis\":");
    add_shares_array(t, p->cost_basis);
}

/* Your trades, oldest first. qty: + = bought, - = sold. */
static void add_trades(Text *t, const Game *g) {
    text_add(t, "\"trades\":[");
    for (int i = 0; i < g->trade_count; i++) {
        const Trade *tr = &g->trades[i];
        text_add(t, "%s{\"day\":%d,\"stock\":%d,\"qty\":%ld,\"price\":%ld,\"fee\":%ld}",
                 i > 0 ? "," : "", tr->day, tr->stock, tr->qty, tr->price, tr->fee);
    }
    text_add(t, "]");
}

/* The race: every player's net worth on each day so far. Player 0 is you. */
static void add_race(Text *t, const Game *g) {
    text_add(t, "\"race\":[");
    for (int pl = 0; pl < NUM_PLAYERS; pl++) {
        const Portfolio *p = pl == 0 ? &g->you : &g->bots[pl - 1].p;
        text_add(t, "%s{\"name\":", pl > 0 ? "," : "");
        text_add_json_string(t, p->name);
        text_add(t, ",\"isYou\":%s,\"worth\":[", pl == 0 ? "true" : "false");
        for (int d = 0; d < g->worth_len; d++) {
            text_add(t, "%s%ld", d > 0 ? "," : "", g->worth_history[pl][d]);
        }
        text_add(t, "]}");
    }
    text_add(t, "]");
}

/* The all-time top 10 from scores.txt. */
static void add_all_time(Text *t) {
    text_add(t, "\"allTime\":[");
    for (int i = 0; i < board_count; i++) {
        text_add(t, "%s{\"name\":", i > 0 ? "," : "");
        text_add_json_string(t, board[i].name);
        text_add(t, ",\"netWorth\":%ld,\"seed\":%lu,\"date\":", board[i].net_worth, board[i].seed);
        text_add_json_string(t, board[i].date);
        text_add(t, "}");
    }
    text_add(t, "],\"resultSaved\":%s,\"yourPlace\":%d", result_saved ? "true" : "false", your_place);
}

static void add_stocks(Text *t, const Market *m) {
    text_add(t, "\"stocks\":[");
    for (int i = 0; i < NUM_STOCKS; i++) {
        const Stock *s = &m->stocks[i];
        text_add(t, "%s{\"symbol\":", i > 0 ? "," : "");
        text_add_json_string(t, s->symbol);
        text_add(t, ",\"name\":");
        text_add_json_string(t, s->name);
        text_add(t, ",\"price\":%ld,\"volatilityBp\":%d,\"betaPct\":%d,\"history\":[",
                 s->price, s->volatility_bp, s->beta_pct);
        for (int d = 0; d < s->history_len; d++) {
            text_add(t, "%s%ld", d > 0 ? "," : "", s->history[d]);
        }
        text_add(t, "]}");
    }
    text_add(t, "]");
}

static void add_leaderboard(Text *t, const Game *g) {
    const Portfolio *players[NUM_BOTS + 1];
    players[0] = &g->you;
    for (int i = 0; i < NUM_BOTS; i++) {
        players[i + 1] = &g->bots[i].p;
    }

    int order[NUM_BOTS + 1];
    rank_by_value(players, NUM_BOTS + 1, &g->market, order);   /* same sort as the terminal */

    text_add(t, "\"leaderboard\":[");
    for (int rank = 0; rank < NUM_BOTS + 1; rank++) {
        const Portfolio *p = players[order[rank]];
        text_add(t, "%s{\"name\":", rank > 0 ? "," : "");
        text_add_json_string(t, p->name);
        text_add(t, ",\"isYou\":%s,\"netWorth\":%ld}",
                 order[rank] == 0 ? "true" : "false", portfolio_value(p, &g->market));
    }
    text_add(t, "]");
}

/* The market index and today's mood. */
static void add_market(Text *t, const Market *m) {
    text_add(t, "\"index\":%ld,\"indexStart\":%ld,\"marketEvent\":%d,\"indexHistory\":[",
             m->index, INDEX_START, m->market_event);
    for (int d = 0; d < m->index_history_len; d++) {
        text_add(t, "%s%ld", d > 0 ? "," : "", m->index_history[d]);
    }
    text_add(t, "]");
}

/* The end-of-game lesson, worked out by the same C code as the terminal version. */
static void add_lesson(Text *t, const Game *g) {
    const Market *m = &g->market;
    int best = best_stock(m);
    text_add(t, "\"lesson\":{\"bestStock\":%d,\"worstStock\":%d,\"holdBestValue\":%ld,"
                "\"followIndexValue\":%ld}",
             best, worst_stock(m), hold_value(&m->stocks[best], START_CASH),
             (long)((long long)START_CASH * m->index / INDEX_START));
}

static void add_state(Text *t, const Game *g) {
    const Market *m = &g->market;

    text_add(t, "{\"seed\":%lu,\"day\":%d,\"numDays\":%d,\"gameOver\":%s,\"startCash\":%ld,\"feeBp\":%d,",
             g->seed, m->day, NUM_DAYS, game_is_over(g) ? "true" : "false", START_CASH, FEE_BP);
    add_market(t, m);
    text_add(t, ",");
    add_lesson(t, g);
    text_add(t, ",");
    text_add(t, "\"news\":");
    text_add_json_string(t, m->news);
    text_add(t, ",");

    add_stocks(t, m);

    text_add(t, ",\"you\":{");
    add_portfolio(t, &g->you, m);
    add_holdings_pl(t, &g->you, m);
    text_add(t, "},\"bots\":[");
    for (int i = 0; i < NUM_BOTS; i++) {
        text_add(t, "%s{", i > 0 ? "," : "");
        add_portfolio(t, &g->bots[i].p, m);
        text_add(t, ",\"strategy\":");
        text_add_json_string(t, BOT_STRATEGY_TEXT[i]);
        text_add(t, ",\"lastTrades\":");
        add_shares_array(t, g->bot_trades[i]);
        text_add(t, "}");
    }
    text_add(t, "],");

    add_leaderboard(t, g);
    text_add(t, ",");
    add_trades(t, g);
    text_add(t, ",");
    add_race(t, g);
    text_add(t, ",");
    add_all_time(t);
    text_add(t, "}");
}

/* ======================================================================
 * Sending HTTP answers
 * ==================================================================== */

/* write() may send only part of the data, so keep going until it's all sent. */
static void send_all(int client, const char *data, size_t length) {
    while (length > 0) {
        ssize_t sent = write(client, data, length);
        if (sent <= 0) {
            return;   /* the browser hung up; nothing more to do */
        }
        data += sent;
        length -= (size_t)sent;
    }
}

static void send_response(int client, int status, const char *status_text,
                          const char *content_type, const char *body, size_t body_length) {
    char header[512];
    int header_length = snprintf(header, sizeof header,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, body_length);

    send_all(client, header, (size_t)header_length);
    send_all(client, body, body_length);
}

static void send_text(int client, int status, const char *status_text, const char *message) {
    send_response(client, status, status_text, "text/plain; charset=utf-8", message, strlen(message));
}

/* Send { "ok": ..., "message": "...", "state": {...} } */
static void send_api_answer(int client, int ok, const char *message) {
    static Text t;
    t.length = 0;
    t.overflowed = 0;

    text_add(&t, "{\"ok\":%s,\"message\":", ok ? "true" : "false");
    text_add_json_string(&t, message);
    text_add(&t, ",\"state\":");
    add_state(&t, &game);
    text_add(&t, "}");

    if (t.overflowed) {
        send_text(client, 500, "Internal Server Error", "Answer too big.");
        return;
    }
    send_response(client, 200, "OK", "application/json", t.data, t.length);
}

/* ======================================================================
 * Web page files
 * ==================================================================== */

typedef struct {
    const char *url;
    const char *file;
    const char *content_type;
} StaticFile;

static const StaticFile STATIC_FILES[] = {
    { "/",          "web/index.html", "text/html; charset=utf-8" },
    { "/style.css", "web/style.css",  "text/css; charset=utf-8" },
    { "/app.js",    "web/app.js",     "text/javascript; charset=utf-8" },
    { "/vue.js",    "web/vue.global.prod.js", "text/javascript; charset=utf-8" },   /* Vue 3, saved in the repo */
};
#define NUM_STATIC_FILES (int)(sizeof STATIC_FILES / sizeof STATIC_FILES[0])

static int serve_static_file(int client, const char *path) {
    for (int i = 0; i < NUM_STATIC_FILES; i++) {
        if (strcmp(path, STATIC_FILES[i].url) != 0) {
            continue;
        }

        FILE *f = fopen(STATIC_FILES[i].file, "rb");
        if (f == NULL) {
            send_text(client, 404, "Not Found",
                      "Page file missing. Start ./arena-server from the trading-arena folder.");
            return 1;
        }

        static char body[256 * 1024];
        size_t length = fread(body, 1, sizeof body, f);
        fclose(f);
        send_response(client, 200, "OK", STATIC_FILES[i].content_type, body, length);
        return 1;
    }
    return 0;   /* not one of our files */
}

/* ======================================================================
 * Reading the request
 * ==================================================================== */

/* Find "name=value" in a query like "stock=2&qty=10" and copy the value.
   Returns 1 if found. */
static int query_value(const char *query, const char *name, char *out, size_t out_size) {
    size_t name_length = strlen(name);
    const char *p = query;

    while (p != NULL && *p != '\0') {
        if (strncmp(p, name, name_length) == 0 && p[name_length] == '=') {
            const char *value = p + name_length + 1;
            size_t length = strcspn(value, "&");
            if (length >= out_size) {
                length = out_size - 1;
            }
            memcpy(out, value, length);
            out[length] = '\0';
            return 1;
        }
        p = strchr(p, '&');
        if (p != NULL) {
            p++;   /* skip the '&' */
        }
    }
    return 0;
}

/* Read a whole-number query value that must be from min to max. */
static int query_number(const char *query, const char *name, long min, long max, long *out) {
    char text[32];
    return query_value(query, name, text, sizeof text)
        && parse_long_in_range(text, min, max, out);
}

/* ======================================================================
 * The API actions
 * ==================================================================== */

static void api_new(int client, const char *query) {
    long seed;
    if (!query_number(query, "seed", 0, MAX_SEED, &seed)) {
        seed = (long)(time(NULL) % MAX_SEED);
    }
    game_new(&game, (unsigned long)seed);
    result_saved = 0;
    your_place = 0;

    /* The player's name, sent URL-encoded, e.g. name=Mew%20TA */
    char raw[96];
    char decoded[96];
    if (query_value(query, "name", raw, sizeof raw)) {
        url_decode(raw, decoded, sizeof decoded);
        clean_name(decoded, game.you.name, sizeof game.you.name);
    }

    char message[64];
    snprintf(message, sizeof message, "New game started with seed %ld.", seed);
    send_api_answer(client, 1, message);
}

static void api_trade(int client, const char *query, int is_buy) {
    if (game_is_over(&game)) {
        send_api_answer(client, 0, "The game is over. Start a new one.");
        return;
    }

    long stock, qty;
    if (!query_number(query, "stock", 0, NUM_STOCKS - 1, &stock)
        || !query_number(query, "qty", 1, MAX_ORDER_QTY, &qty)) {
        send_api_answer(client, 0, "Pick a stock and a whole number of shares.");
        return;
    }

    const char *symbol = game.market.stocks[stock].symbol;
    long fees_before = game.you.fees_paid;
    int result = is_buy ? game_buy(&game, (int)stock, qty)
                        : game_sell(&game, (int)stock, qty);

    char message[96];
    switch (result) {
        case TRADE_OK:
        {
            char fee[32];
            format_money(game.you.fees_paid - fees_before, fee, sizeof fee);
            snprintf(message, sizeof message, "%s %ld %s (fee %s).",
                     is_buy ? "Bought" : "Sold", qty, symbol, fee);
        }
            break;
        case TRADE_NO_CASH:
            snprintf(message, sizeof message, "Not enough cash for %ld %s and the fee.", qty, symbol);
            break;
        case TRADE_NO_SHARES:
            snprintf(message, sizeof message, "You only own %ld %s.", game.you.shares[stock], symbol);
            break;
        default:
            snprintf(message, sizeof message, "Invalid order.");
            break;
    }
    send_api_answer(client, result == TRADE_OK, message);
}

static void api_next(int client) {
    if (game_is_over(&game)) {
        send_api_answer(client, 0, "The game is over. Start a new one.");
        return;
    }
    game_next_day(&game);

    /* The game just ended: add your result to the all-time top 10 and save the file. */
    if (game_is_over(&game) && !result_saved) {
        ScoreEntry e;
        scores_make_entry(&e, game.you.name, portfolio_value(&game.you, &game.market), game.seed);
        your_place = scores_add(board, &board_count, MAX_SCORES, &e);
        result_saved = 1;
        if (!scores_save(SCORES_FILE, board, board_count)) {
            send_api_answer(client, 0, "Game over, but scores.txt could not be saved.");
            return;
        }
    }
    send_api_answer(client, 1, "");
}

/* ======================================================================
 * Handling one browser request
 * ==================================================================== */

static void handle_client(int client) {
    char request[REQUEST_MAX];
    ssize_t length = read(client, request, sizeof request - 1);
    if (length <= 0) {
        return;
    }
    request[length] = '\0';

    /* First line looks like:  "POST /api/buy?stock=0&qty=5 HTTP/1.1" */
    char method[8];
    char target[1024];
    if (sscanf(request, "%7s %1023s", method, target) != 2) {
        send_text(client, 400, "Bad Request", "Bad request.");
        return;
    }

    /* Split "/api/buy?stock=0&qty=5" into path "/api/buy" and query "stock=0&qty=5". */
    char *query = strchr(target, '?');
    if (query != NULL) {
        *query = '\0';
        query++;
    } else {
        query = "";
    }
    const char *path = target;

    int is_get = (strcmp(method, "GET") == 0);
    int is_post = (strcmp(method, "POST") == 0);

    if (is_get && strcmp(path, "/api/state") == 0) {
        send_api_answer(client, 1, "");
    } else if (is_post && strcmp(path, "/api/new") == 0) {
        api_new(client, query);
    } else if (is_post && strcmp(path, "/api/buy") == 0) {
        api_trade(client, query, 1);
    } else if (is_post && strcmp(path, "/api/sell") == 0) {
        api_trade(client, query, 0);
    } else if (is_post && strcmp(path, "/api/next") == 0) {
        api_next(client);
    } else if (is_get && serve_static_file(client, path)) {
        /* sent */
    } else {
        send_text(client, 404, "Not Found", "Not found.");
    }
}

/* ======================================================================
 * Starting the server
 * ==================================================================== */

static int open_listening_socket(int port) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("socket");
        return -1;
    }

    /* Lets you restart the server right away without "Address already in use". */
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);   /* this computer only */

    if (bind(server, (struct sockaddr *)&address, sizeof address) < 0) {
        perror("bind (is the port already in use?)");
        close(server);
        return -1;
    }
    if (listen(server, 16) < 0) {
        perror("listen");
        close(server);
        return -1;
    }
    return server;
}

int main(int argc, char **argv) {
    long port = DEFAULT_PORT;
    if (argc > 1 && !parse_long_in_range(argv[1], 1024, 65535, &port)) {
        fprintf(stderr, "Port must be a number from 1024 to 65535.\n");
        return 1;
    }

    /* If the browser closes the connection early, don't let that kill the server. */
    signal(SIGPIPE, SIG_IGN);

    int server = open_listening_socket((int)port);
    if (server < 0) {
        return 1;
    }

    game_new(&game, (unsigned long)(time(NULL) % MAX_SEED));
    board_count = scores_load(SCORES_FILE, board, MAX_SCORES);

    printf("Stock Trading Arena is running.\n");
    printf("Open http://localhost:%ld in your browser. Press Ctrl+C to stop.\n", port);
    fflush(stdout);

    for (;;) {
        int client = accept(server, NULL, NULL);
        if (client < 0) {
            continue;
        }

        struct timeval timeout = { READ_TIMEOUT_SECS, 0 };
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        handle_client(client);
        close(client);
    }
}
