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
    long price = m->stocks[stock].price;
    if (price <= 0) return TRADE_BAD_INPUT;
    if (qty > p->cash / price) return TRADE_NO_CASH;   /* divide first: no overflow on 32-bit long */
    p->cash -= price * qty;                            /* safe: price * qty <= cash */
    p->shares[stock] += qty;
    return TRADE_OK;
}

int portfolio_sell(Portfolio *p, const Market *m, int stock, long qty) {
    if (!valid_order(stock, qty)) return TRADE_BAD_INPUT;
    if (qty > p->shares[stock]) return TRADE_NO_SHARES;
    p->shares[stock] -= qty;
    p->cash = (long)((long long)p->cash + (long long)m->stocks[stock].price * qty);
    return TRADE_OK;
}

long portfolio_value(const Portfolio *p, const Market *m) {
    long long total = p->cash;
    for (int i = 0; i < NUM_STOCKS; i++) total += (long long)p->shares[i] * m->stocks[i].price;
    return (long)total;
}

long max_affordable(const Portfolio *p, const Market *m, int stock) {
    if (stock < 0 || stock >= NUM_STOCKS) return 0;
    long price = m->stocks[stock].price;
    return price > 0 ? p->cash / price : 0;
}
