/*
 * quotes.h - Fetch delayed market data for one symbol.
 *
 * Data comes from Yahoo Finance's public chart endpoint, which needs no
 * API key. It is unofficial and delayed, so every quote carries a status
 * and timestamp that Claude uses to decide what to verify by web search.
 */
#ifndef QUOTES_H
#define QUOTES_H

#include <time.h>

/* Quotes older than this are flagged STALE. */
#define STALE_AFTER_SECONDS (7L * 24 * 60 * 60)

typedef enum {
    QUOTE_OK,          /* fresh data */
    QUOTE_STALE,       /* data returned but last trade is old */
    QUOTE_UNAVAILABLE, /* request failed or symbol unknown */
    QUOTE_SKIPPED      /* not fetched (offline mode) */
} QuoteStatus;

typedef struct {
    char symbol[32];
    char name[128];
    char exchange[32];
    char currency[8];

    double price;            /* latest regular-market price */
    double change_pct;       /* change vs previous close, percent */
    double month_change_pct; /* change over roughly one month, percent */
    double high_52w;
    double low_52w;
    int price_hint;          /* decimal places suggested by the provider */

    int has_change;
    int has_month_change;
    int has_52w;

    time_t market_time;      /* time of the latest price */

    QuoteStatus status;
    char error[160];
} Quote;

/* Fill `quote` for `symbol`. Never fails hard: problems set the status. */
void fetch_quote(const char *symbol, Quote *quote);

/* Mark a quote as intentionally not fetched. */
void skip_quote(const char *symbol, Quote *quote);

/* Short label for a status, e.g. "OK" or "STALE". */
const char *quote_status_label(QuoteStatus status);

#endif /* QUOTES_H */
