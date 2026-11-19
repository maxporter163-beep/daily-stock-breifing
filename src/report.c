/*
 * report.c - Write the "research input pack" that Claude Code reads.
 */
#include "report.h"

#include <stdlib.h>
#include <string.h>

#define PROGRAM_VERSION "2.0"
#define BRIEFING_TZ "Pacific/Auckland"

/* Format a timestamp in the briefing time zone (NZ) or UTC. */
static void format_time(time_t when, int utc, const char *fmt,
                        char *buf, size_t len) {
    struct tm parts;
    if (when <= 0) {
        snprintf(buf, len, "n/a");
        return;
    }
    if (utc) gmtime_r(&when, &parts);
    else localtime_r(&when, &parts);
    strftime(buf, len, fmt, &parts);
}

/* Print a number with a fixed number of decimals, or "n/a". */
static void print_number(FILE *out, int present, double value, int decimals) {
    if (present) fprintf(out, " %.*f |", decimals, value);
    else fprintf(out, " n/a |");
}

/* Print a signed percentage, or "n/a". */
static void print_percent(FILE *out, int present, double value) {
    if (present) fprintf(out, " %+.2f%% |", value);
    else fprintf(out, " n/a |");
}

/* Table cells must not contain the pipe character. */
static void print_cell(FILE *out, const char *text) {
    fputc(' ', out);
    if (*text == '\0') fputs("-", out);
    for (; *text; text++) fputc(*text == '|' ? '/' : *text, out);
    fputs(" |", out);
}

/* A usable price: fetched (fresh or stale) and not a placeholder zero. */
static int has_price(const Quote *q) {
    return (q->status == QUOTE_OK || q->status == QUOTE_STALE) &&
           q->price > 0.0;
}

static int count_status(const Quote *quotes, int count, QuoteStatus status) {
    int total = 0;
    for (int i = 0; i < count; i++)
        if (quotes[i].status == status) total++;
    return total;
}

static void print_header(FILE *out, const RunInfo *info,
                         const Watchlist *stocks, const Quote *stock_quotes,
                         const Watchlist *market, const Quote *market_quotes) {
    char local[64], utc[64];
    format_time(info->generated_at, 0, "%A %d %B %Y, %H:%M %Z",
                local, sizeof(local));
    format_time(info->generated_at, 1, "%Y-%m-%d %H:%M UTC",
                utc, sizeof(utc));

    fprintf(out, "# Daily Stock Briefing - Research Input Pack\n\n");
    fprintf(out, "- Generated: %s (%s)\n", local, utc);
    fprintf(out, "- Briefing time zone: %s\n", BRIEFING_TZ);
    fprintf(out, "- Program: stock-briefing v%s\n", PROGRAM_VERSION);
    fprintf(out, "- Watchlist file: %s (%d symbols)\n",
            info->stocks_path, stocks->count);
    fprintf(out, "- Market file: %s (%d symbols)\n",
            info->market_path, market->count);
    fprintf(out, "- Price source: %s\n",
            info->offline ? "none (offline mode, no prices fetched)"
                          : "Yahoo Finance chart API (free, unofficial, "
                            "delayed; verify anything important)");
    fprintf(out, "- Quote coverage: watchlist %d OK / %d stale / %d "
                 "unavailable; market %d OK / %d stale / %d unavailable\n\n",
            count_status(stock_quotes, stocks->count, QUOTE_OK),
            count_status(stock_quotes, stocks->count, QUOTE_STALE),
            count_status(stock_quotes, stocks->count, QUOTE_UNAVAILABLE) +
                count_status(stock_quotes, stocks->count, QUOTE_SKIPPED),
            count_status(market_quotes, market->count, QUOTE_OK),
            count_status(market_quotes, market->count, QUOTE_STALE),
            count_status(market_quotes, market->count, QUOTE_UNAVAILABLE) +
                count_status(market_quotes, market->count, QUOTE_SKIPPED));
}

static void print_market_table(FILE *out, const Watchlist *market,
                               const Quote *quotes) {
    fprintf(out, "## 1. Market snapshot\n\n");
    fprintf(out, "| Symbol | Description | Name | Last | Day %% | 1-month %% "
                 "| Ccy | Price time (NZ) | Status |\n");
    fprintf(out, "|---|---|---|---|---|---|---|---|---|\n");

    for (int i = 0; i < market->count; i++) {
        const Quote *q = &quotes[i];
        char when[32];
        format_time(q->market_time, 0, "%Y-%m-%d %H:%M", when, sizeof(when));

        fprintf(out, "|");
        print_cell(out, q->symbol);
        print_cell(out, market->items[i].note);
        print_cell(out, q->name);
        print_number(out, has_price(q), q->price, q->price_hint);
        print_percent(out, has_price(q) && q->has_change, q->change_pct);
        print_percent(out, has_price(q) && q->has_month_change,
                      q->month_change_pct);
        print_cell(out, q->currency);
        print_cell(out, has_price(q) ? when : "n/a");
        print_cell(out, quote_status_label(q->status));
        fputc('\n', out);
    }
    fputc('\n', out);
}

static void print_watchlist_table(FILE *out, const Watchlist *stocks,
                                  const Quote *quotes) {
    fprintf(out, "## 2. Watchlist snapshot\n\n");
    fprintf(out, "| Symbol | Note | Name | Exchange | Last | Day %% "
                 "| 1-month %% | 52w low | 52w high | vs 52w high "
                 "| Ccy | Price time (NZ) | Status |\n");
    fprintf(out, "|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");

    for (int i = 0; i < stocks->count; i++) {
        const Quote *q = &quotes[i];
        int priced = has_price(q);
        int range = priced && q->has_52w && q->high_52w > 0.0;
        char when[32];
        format_time(q->market_time, 0, "%Y-%m-%d %H:%M", when, sizeof(when));

        fprintf(out, "|");
        print_cell(out, q->symbol);
        print_cell(out, stocks->items[i].note);
        print_cell(out, q->name);
        print_cell(out, q->exchange);
        print_number(out, priced, q->price, q->price_hint);
        print_percent(out, priced && q->has_change, q->change_pct);
        print_percent(out, priced && q->has_month_change,
                      q->month_change_pct);
        print_number(out, range, q->low_52w, q->price_hint);
        print_number(out, range, q->high_52w, q->price_hint);
        print_percent(out, range,
                      range ? (q->price - q->high_52w) / q->high_52w * 100.0
                            : 0.0);
        print_cell(out, q->currency);
        print_cell(out, priced ? when : "n/a");
        print_cell(out, quote_status_label(q->status));
        fputc('\n', out);
    }
    fputc('\n', out);
}

static void print_data_notes(FILE *out, const Watchlist *stocks,
                             const Quote *stock_quotes,
                             const Watchlist *market,
                             const Quote *market_quotes) {
    int problems = 0;

    fprintf(out, "## 3. Data quality notes\n\n");

    for (int pass = 0; pass < 2; pass++) {
        const Watchlist *list = pass == 0 ? stocks : market;
        const Quote *quotes = pass == 0 ? stock_quotes : market_quotes;

        for (int i = 0; i < list->count; i++) {
            const Quote *q = &quotes[i];
            if (q->status == QUOTE_OK) continue;

            char when[32];
            format_time(q->market_time, 0, "%Y-%m-%d", when, sizeof(when));
            fprintf(out, "- %s: %s - %s", q->symbol,
                    quote_status_label(q->status), q->error);
            if (q->status == QUOTE_STALE)
                fprintf(out, " (provider timestamp %s)", when);
            fprintf(out, ". Get the current price by web search.\n");
            problems++;
        }
    }

    if (problems == 0)
        fprintf(out, "- All quotes returned fresh data.\n");

    fprintf(out, "- Prices are delayed snapshots. Use them as the reference "
                 "price for upside/downside unless marked STALE, UNAVAILABLE "
                 "or SKIPPED.\n\n");
}

/*
 * The research brief. This text is the prompt that the original program
 * sent to the Anthropic API, extended with a market overview, the
 * upside/downside calculation and the stock discovery section.
 */
static void print_research_brief(FILE *out) {
    fputs(
        "## 4. Research brief\n\n"

        "You are my daily equity research analyst. Use the snapshots "
        "above as your starting point, then research the current news "
        "for each stock.\n\n"

        "OVERALL MARKET:\n"
        "- Summarise the main moves in the NZX 50, ASX 200 and US markets, "
        "the NZD and commodities, using the market snapshot.\n"
        "- Explain the main reasons behind those moves: macro data, central "
        "banks such as the RBNZ and the Fed, and sector news.\n\n"

        "FOR EACH STOCK IN THE WATCHLIST INCLUDE:\n"

        "1. Analyst consensus:\n"
        "   - Buy, Hold and Sell ratings.\n"
        "   - Number of analysts in each category.\n"
        "   - Consensus rating.\n\n"

        "2. Price target changes:\n"
        "   - Recent analyst upgrades or downgrades.\n"
        "   - Previous and revised price targets.\n"
        "   - Analyst firm and date of change.\n"
        "   - Consensus price target and implied upside/downside, calculated "
        "as (consensus target - last price) / last price, using the Last "
        "price from the snapshot. State the price you used.\n\n"

        "3. Key catalysts:\n"
        "   - Upcoming earnings announcements.\n"
        "   - Product launches and company announcements.\n"
        "   - Industry developments.\n"
        "   - Regulatory developments.\n"
        "   - Important risks and negative developments.\n\n"

        "4. Overall outlook:\n"
        "   - Most important development affecting the stock.\n"
        "   - Bullish and bearish considerations: risks and opportunities.\n"
        "   - What investors should watch next.\n\n"

        "For ETFs and funds, replace analyst ratings with the drivers of the "
        "underlying assets, such as the index, property sector or metal "
        "price, and say that analyst coverage does not apply.\n\n"

        "ADDITIONAL STOCK DISCOVERY:\n"
        "- Finish with the top five additional stocks worth watching.\n"
        "- None of them may be on the watchlist above.\n"
        "- Prioritise new opportunities, notable positive developments, "
        "meaningful momentum or analyst interest, and NZX-listed companies "
        "where possible.\n"
        "- For each, explain why it is worth watching, the developments "
        "that make it interesting and its key risks.\n"
        "- Label them as research candidates, not buy recommendations.\n\n"

        "RESEARCH RULES:\n"
        "- Search the web for current information.\n"
        "- Prioritise the latest available analyst data.\n"
        "- Clearly distinguish new developments from older news.\n"
        "- Never invent analyst ratings or price targets.\n"
        "- State when data is unavailable or unverified.\n"
        "- Include dates and credible source links.\n"
        "- Distinguish factual information from analyst opinion.\n"
        "- Do not give personalised buy/sell instructions.\n\n"

        "FORMAT:\n"
        "Start with today's date in New Zealand.\n"
        "Then give the three most important developments across the entire "
        "watchlist, then the market overview.\n"
        "Use clear headings for each ticker.\n"
        "Finish with the stock discovery section and a one-line disclaimer "
        "that this is research, not personal financial advice.\n"
        "Keep it readable on a phone, ideally under 1,500 words. "
        "Prioritise useful information.\n",
        out);
}

void print_research_pack(FILE *out, const RunInfo *info,
                         const Watchlist *stocks, const Quote *stock_quotes,
                         const Watchlist *market, const Quote *market_quotes) {
    print_header(out, info, stocks, stock_quotes, market, market_quotes);
    print_market_table(out, market, market_quotes);
    print_watchlist_table(out, stocks, stock_quotes);
    print_data_notes(out, stocks, stock_quotes, market, market_quotes);
    print_research_brief(out);
}
