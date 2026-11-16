#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/market.h"
#include "../src/portfolio.h"
#include "../src/display.h"

static void test_money_format(void) { char b[32];
    format_money(0,b,sizeof b); assert(strcmp(b,"0.00")==0);
    format_money(5,b,sizeof b); assert(strcmp(b,"0.05")==0);
    format_money(1000000,b,sizeof b); assert(strcmp(b,"10,000.00")==0);
    format_money(123456789,b,sizeof b); assert(strcmp(b,"1,234,567.89")==0); }
static void test_money_negative_and_small_buffer(void) { char b[32]; char s[5];
    format_money(-5,b,sizeof b); assert(strcmp(b,"-0.05")==0);
    format_money(-123456,b,sizeof b); assert(strcmp(b,"-1,234.56")==0);
    format_money(123456789,s,sizeof s); assert(strlen(s) < sizeof s);
    format_money(1,b,0); }
static int count(const char *s, char c) { int k=0; for(;*s;s++) if(*s==c) k++; return k; }
static void test_chart_counts_points(void) { long h[] = {100,300,200,500}; char buf[1024];
    assert(render_chart(h,4,CHART_ROWS,buf,sizeof buf) == 4 && count(buf,'*') == 4 && count(buf,'\n') == CHART_ROWS); }
static void test_chart_flat_prices(void) { long h[] = {700,700,700}; char buf[1024];
    assert(render_chart(h,3,CHART_ROWS,buf,sizeof buf) == 3); }
static void test_chart_single_point(void) { long h[] = {700}; char buf[1024];
    assert(render_chart(h,1,CHART_ROWS,buf,sizeof buf) == 1); }
static void test_chart_geometry(void) { long h[] = {100,500,300}; char buf[1024]; char *line;
    assert(render_chart(h,3,CHART_ROWS,buf,sizeof buf) == 3);
    assert(strlen(buf) == (size_t)CHART_ROWS * 4);
    assert(buf[1] == '*');                          /* max on top row, column 1 */
    line = buf + (CHART_ROWS-1) * 4; assert(line[0] == '*'); /* min on bottom row */
    line = buf + 4 * 4; assert(line[2] == '*'); }    /* 300: (500-300)*9/400 = 4 -> row 4 */
static void test_chart_flat_middle_row(void) { long h[] = {700,700}; char buf[1024];
    render_chart(h,2,CHART_ROWS,buf,sizeof buf);
    assert(buf[(CHART_ROWS/2) * 3] == '*' && buf[(CHART_ROWS/2) * 3 + 1] == '*'); }
static void test_chart_bad_args(void) { long h[] = {1,2}; char buf[8];
    buf[0] = 'x';
    assert(render_chart(h,2,CHART_ROWS,buf,sizeof buf) == 0);
    assert(render_chart(h,0,CHART_ROWS,buf,sizeof buf) == 0); }
static void test_rank(void) { Market m; market_init(&m); Portfolio a,b,c;
    portfolio_init(&a,"A",100); portfolio_init(&b,"B",300); portfolio_init(&c,"C",200);
    const Portfolio *ps[] = {&a,&b,&c}; int o[3]; rank_by_value(ps,3,&m,o);
    assert(o[0]==1 && o[1]==2 && o[2]==0); }
static void test_rank_ties_stable(void) { Market m; market_init(&m); Portfolio a,b,c;
    portfolio_init(&a,"A",200); portfolio_init(&b,"B",200); portfolio_init(&c,"C",200);
    const Portfolio *ps[] = {&a,&b,&c}; int o[3]; rank_by_value(ps,3,&m,o);
    assert(o[0]==0 && o[1]==1 && o[2]==2); }
static void test_print_smoke(void) { Market m; Portfolio a,b; market_init(&m);
    portfolio_init(&a,"You",START_CASH); portfolio_init(&b,"Bot",500000);
    portfolio_buy(&a,&m,0,5);
    const Portfolio *ps[] = {&a,&b};
    print_market(&m); print_chart(&m.stocks[0]); print_portfolio(&a,&m); print_leaderboard(ps,2,&m); }

int main(void) {
    test_money_format(); test_money_negative_and_small_buffer();
    test_chart_counts_points(); test_chart_flat_prices(); test_chart_single_point();
    test_chart_geometry(); test_chart_flat_middle_row(); test_chart_bad_args();
    test_rank(); test_rank_ties_stable(); test_print_smoke();
    printf("test_display: ALL PASSED\n"); return 0; }
