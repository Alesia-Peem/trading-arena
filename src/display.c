#include <stdio.h>
#include <string.h>
#include "display.h"

void format_money(long cents, char *out, size_t n) {
    char tmp[48];
    char digits[32];
    int nd = 0, pos = 0, i;
    unsigned long long mag;
    int neg = cents < 0;
    if (n == 0) return;
    mag = neg ? (unsigned long long)(-(cents + 1)) + 1ULL : (unsigned long long)cents;
    {
        unsigned long long whole = mag / 100ULL;
        int frac = (int)(mag % 100ULL);
        do { digits[nd++] = (char)('0' + (int)(whole % 10ULL)); whole /= 10ULL; } while (whole > 0);
        if (neg) tmp[pos++] = '-';
        for (i = nd - 1; i >= 0; i--) {
            tmp[pos++] = digits[i];
            if (i > 0 && i % 3 == 0) tmp[pos++] = ',';
        }
        tmp[pos++] = '.';
        tmp[pos++] = (char)('0' + frac / 10);
        tmp[pos++] = (char)('0' + frac % 10);
        tmp[pos] = '\0';
    }
    strncpy(out, tmp, n - 1);
    out[n - 1] = '\0';
}

int render_chart(const long *hist, int len, int rows, char *buf, size_t n) {
    long mn, mx;
    int i, r, c;
    size_t need;
    if (len <= 0 || rows <= 0 || n == 0) { if (n > 0) buf[0] = '\0'; return 0; }
    need = (size_t)rows * ((size_t)len + 1) + 1;
    if (n < need) { buf[0] = '\0'; return 0; }
    mn = mx = hist[0];
    for (i = 1; i < len; i++) { if (hist[i] < mn) mn = hist[i]; if (hist[i] > mx) mx = hist[i]; }
    for (r = 0; r < rows; r++) {
        for (c = 0; c < len; c++) buf[r * (len + 1) + c] = ' ';
        buf[r * (len + 1) + len] = '\n';
    }
    buf[(size_t)rows * (len + 1)] = '\0';
    for (c = 0; c < len; c++) {
        if (mx == mn) r = rows / 2;
        else r = (int)(((long long)mx - hist[c]) * (rows - 1) / ((long long)mx - mn));
        buf[r * (len + 1) + c] = '*';
    }
    return len;
}

/* basis points -> "+2.35%" / "-0.80%" / "0.00%" */
static void format_bp(long long bp_in, char *out, size_t n) {
    long bp = (long)bp_in;
    long a = bp < 0 ? -bp : bp;
    const char *sign = bp > 0 ? "+" : (bp < 0 ? "-" : "");
    snprintf(out, n, "%s%ld.%02ld%%", sign, a / 100, a % 100);
}

void print_market(const Market *m) {
    int i;
    printf("\n=== Market, day %d ===\n", m->day);
    printf("%-6s %-24s %12s %9s\n", "SYM", "NAME", "PRICE", "CHANGE");
    for (i = 0; i < NUM_STOCKS; i++) {
        const Stock *s = &m->stocks[i];
        char price[32], chg[32];
        long long bp = 0;
        format_money(s->price, price, sizeof price);
        if (s->history_len >= 2 && s->history[s->history_len - 2] > 0) {
            long long prev = s->history[s->history_len - 2];
            bp = ((long long)s->price - prev) * 10000 / prev;
        }
        format_bp(bp, chg, sizeof chg);
        printf("%-6s %-24s %12s %9s\n", s->symbol, s->name, price, chg);
    }
}

void print_chart(const Stock *s) {
    char buf[CHART_ROWS * (MAX_HISTORY + 1) + 1];
    char hi[32], lo[32];
    long mn, mx;
    int i;
    if (s->history_len <= 0) return;
    mn = mx = s->history[0];
    for (i = 1; i < s->history_len; i++) {
        if (s->history[i] < mn) mn = s->history[i];
        if (s->history[i] > mx) mx = s->history[i];
    }
    format_money(mx, hi, sizeof hi);
    format_money(mn, lo, sizeof lo);
    printf("\n%s - %s (%d days)\n", s->symbol, s->name, s->history_len);
    printf("high: %s\n", hi);
    if (render_chart(s->history, s->history_len, CHART_ROWS, buf, sizeof buf) > 0) fputs(buf, stdout);
    printf("low:  %s\n", lo);
}

void print_portfolio(const Portfolio *p, const Market *m) {
    int i;
    char money[32];
    printf("\n--- %s's portfolio ---\n", p->name);
    for (i = 0; i < NUM_STOCKS; i++) {
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

void rank_by_value(const Portfolio *ps[], int n, const Market *m, int order[]) {
    long vals[64];
    int i, j;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) { order[i] = i; vals[i] = portfolio_value(ps[i], m); }
    for (i = 1; i < n; i++) {
        int key = order[i];
        j = i - 1;
        while (j >= 0 && vals[order[j]] < vals[key]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = key;
    }
}

void print_leaderboard(const Portfolio *ps[], int n, const Market *m) {
    int order[64], i;
    if (n > 64) n = 64;
    rank_by_value(ps, n, m, order);
    printf("\n=== Leaderboard ===\n");
    printf("%-4s %-16s %16s %10s\n", "RANK", "NAME", "NET WORTH", "PROFIT");
    for (i = 0; i < n; i++) {
        const Portfolio *p = ps[order[i]];
        long v = portfolio_value(p, m);
        char money[32], pct[32];
        format_money(v, money, sizeof money);
        format_bp(((long long)v - START_CASH) * 10000 / START_CASH, pct, sizeof pct);
        printf("%-4d %-16s %16s %10s\n", i + 1, p->name, money, pct);
    }
}
