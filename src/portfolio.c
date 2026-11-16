#include <string.h>
#include "portfolio.h"

static int valid_order(int stock, long qty) {
    return stock >= 0 && stock < NUM_STOCKS && qty >= 1 && qty <= MAX_ORDER_QTY;
}

void portfolio_init(Portfolio *p, const char *name, long cash) {
    memset(p, 0, sizeof *p);
    strncpy(p->name, name, sizeof p->name - 1);
    p->name[sizeof p->name - 1] = '\0';
    p->cash = cash;
}

int portfolio_buy(Portfolio *p, const Market *m, int stock, long qty) {
    if (!valid_order(stock, qty)) return TRADE_BAD_INPUT;
    long cost = m->stocks[stock].price * qty;
    if (cost > p->cash) return TRADE_NO_CASH;
    p->cash -= cost;
    p->shares[stock] += qty;
    return TRADE_OK;
}

int portfolio_sell(Portfolio *p, const Market *m, int stock, long qty) {
    if (!valid_order(stock, qty)) return TRADE_BAD_INPUT;
    if (qty > p->shares[stock]) return TRADE_NO_SHARES;
    p->shares[stock] -= qty;
    p->cash += m->stocks[stock].price * qty;
    return TRADE_OK;
}

long portfolio_value(const Portfolio *p, const Market *m) {
    long total = p->cash;
    for (int i = 0; i < NUM_STOCKS; i++) total += p->shares[i] * m->stocks[i].price;
    return total;
}

long max_affordable(const Portfolio *p, const Market *m, int stock) {
    if (stock < 0 || stock >= NUM_STOCKS) return 0;
    long price = m->stocks[stock].price;
    return price > 0 ? p->cash / price : 0;
}
