#include <string.h>
#include "portfolio.h"

static int is_valid_stock(int stock) {
    return stock >= 0 && stock < NUM_STOCKS;
}

static int is_valid_order(int stock, long qty) {
    return is_valid_stock(stock) && qty >= 1 && qty <= MAX_ORDER_QTY;
}

void portfolio_init(Portfolio *p, const char *name, long cash) {
    memset(p, 0, sizeof *p);   /* sets every share count to 0 */

    /* Copy the name, cutting it short if it's too long, and always end with '\0'. */
    strncpy(p->name, name, sizeof p->name - 1);
    p->name[sizeof p->name - 1] = '\0';

    p->cash = cash;
}

long trade_fee(long value) {
    if (value <= 0) {
        return 0;
    }
    /* Adding 9999 before dividing rounds UP, so even a tiny trade pays 1 cent. */
    return (long)(((long long)value * FEE_BP + 9999) / 10000);
}

int portfolio_buy(Portfolio *p, const Market *m, int stock, long qty) {
    if (!is_valid_order(stock, qty)) {
        return TRADE_BAD_INPUT;
    }

    long price = m->stocks[stock].price;
    if (price <= 0) {
        return TRADE_BAD_INPUT;
    }

    /* We check "qty > cash / price" instead of "qty * price > cash" on purpose:
       qty * price could be too big for a 32-bit long, but dividing can't overflow. */
    if (qty > p->cash / price) {
        return TRADE_NO_CASH;
    }

    long cost = price * qty;   /* safe now: we know price * qty <= cash */
    long fee = trade_fee(cost);
    if (fee > p->cash - cost) {
        return TRADE_NO_CASH;  /* enough for the shares, but not for the fee */
    }

    p->cash -= cost + fee;
    p->fees_paid += fee;
    p->shares[stock] += qty;
    return TRADE_OK;
}

int portfolio_sell(Portfolio *p, const Market *m, int stock, long qty) {
    if (!is_valid_order(stock, qty)) {
        return TRADE_BAD_INPUT;
    }
    if (qty > p->shares[stock]) {
        return TRADE_NO_SHARES;
    }

    long long value = (long long)m->stocks[stock].price * qty;
    long fee = trade_fee((long)value);

    p->shares[stock] -= qty;
    p->cash = (long)(p->cash + value - fee);
    p->fees_paid += fee;
    return TRADE_OK;
}

long portfolio_value(const Portfolio *p, const Market *m) {
    long long total = p->cash;

    for (int i = 0; i < NUM_STOCKS; i++) {
        total += (long long)p->shares[i] * m->stocks[i].price;
    }
    return (long)total;
}

long shares_affordable(long cash, long price) {
    if (price <= 0 || cash <= 0) {
        return 0;
    }
    /* Start from what the cash buys with no fee, then step down until the fee fits too.
       The fee is only 0.15%, so we step down by at most about 0.15% of the shares. */
    long qty = cash / price;
    while (qty > 0 && price * qty + trade_fee(price * qty) > cash) {
        qty--;
    }
    return qty;
}

long max_affordable(const Portfolio *p, const Market *m, int stock) {
    if (!is_valid_stock(stock)) {
        return 0;
    }
    return shares_affordable(p->cash, m->stocks[stock].price);
}
