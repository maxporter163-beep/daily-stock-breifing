/*
 * main.c - Daily stock briefing: data-gathering stage.
 *
 * This program no longer calls any AI API or SMS service. It:
 *   1. reads the watchlist (config/stocks.txt) and market list
 *      (config/market.txt),
 *   2. fetches a delayed price snapshot for every symbol,
 *   3. prints a Markdown "research input pack" for Claude Code, which
 *      does the research and writes the final briefing.
 *
 * Progress messages go to stderr so stdout contains only the pack.
 *
 * Exit codes: 0 = pack written (even if some quotes failed),
 *             1 = fatal error (no watchlist, bad arguments, write failure).
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>

#include "quotes.h"
#include "report.h"
#include "watchlist.h"

#define DEFAULT_STOCKS "config/stocks.txt"
#define DEFAULT_MARKET "config/market.txt"

/* Pause between requests so we do not hammer the data provider. */
#define REQUEST_GAP_MS 250

typedef struct {
    const char *stocks_path;
    const char *market_path;
    const char *output_path; /* NULL = stdout only */
    int offline;
} Options;

static void usage(FILE *out) {
    fprintf(out,
        "Usage: stock-briefing [options]\n\n"
        "Gathers market data for the watchlist and prints a research input\n"
        "pack (Markdown) for Claude Code to turn into the daily briefing.\n\n"
        "Options:\n"
        "  --stocks FILE   watchlist file (default %s)\n"
        "  --market FILE   market indices/FX file (default %s)\n"
        "  --output FILE   also write the pack to FILE\n"
        "  --offline       do not fetch prices (for testing)\n"
        "  --help          show this help\n",
        DEFAULT_STOCKS, DEFAULT_MARKET);
}

/* Parse command-line options. Returns 1 on success, 0 to exit. */
static int parse_args(int argc, char **argv, Options *opts, int *exit_code) {
    opts->stocks_path = DEFAULT_STOCKS;
    opts->market_path = DEFAULT_MARKET;
    opts->output_path = NULL;
    opts->offline = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        int has_value = i + 1 < argc;

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage(stdout);
            *exit_code = 0;
            return 0;
        } else if (strcmp(arg, "--offline") == 0) {
            opts->offline = 1;
        } else if (strcmp(arg, "--stocks") == 0 && has_value) {
            opts->stocks_path = argv[++i];
        } else if (strcmp(arg, "--market") == 0 && has_value) {
            opts->market_path = argv[++i];
        } else if (strcmp(arg, "--output") == 0 && has_value) {
            opts->output_path = argv[++i];
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n\n", arg);
            usage(stderr);
            *exit_code = 1;
            return 0;
        }
    }
    return 1;
}

static void pause_ms(long ms) {
    struct timespec gap = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&gap, NULL);
}

/* Fetch (or skip) a quote for every symbol in a list. */
static void collect_quotes(const Watchlist *list, Quote *quotes,
                           int offline, const char *label) {
    for (int i = 0; i < list->count; i++) {
        const char *symbol = list->items[i].symbol;

        if (offline) {
            skip_quote(symbol, &quotes[i]);
            continue;
        }

        if (i > 0) pause_ms(REQUEST_GAP_MS);
        fetch_quote(symbol, &quotes[i]);

        fprintf(stderr, "  [%s %d/%d] %-12s %s%s%s\n", label, i + 1,
                list->count, symbol, quote_status_label(quotes[i].status),
                quotes[i].status == QUOTE_OK ? "" : " - ",
                quotes[i].status == QUOTE_OK ? "" : quotes[i].error);
    }
}

/* Main program */
int main(int argc, char **argv) {
    Options opts;
    int exit_code = 0;

    if (!parse_args(argc, argv, &opts, &exit_code))
        return exit_code;

    /* All human-facing times are shown in New Zealand time. */
    setenv("TZ", "Pacific/Auckland", 1);
    tzset();

    fprintf(stderr, "Starting daily stock briefing data collection...\n");

    /* Large arrays live on the heap, not the stack. */
    Watchlist *stocks = calloc(1, sizeof(*stocks));
    Watchlist *market = calloc(1, sizeof(*market));
    Quote *stock_quotes = calloc(MAX_STOCKS, sizeof(*stock_quotes));
    Quote *market_quotes = calloc(MAX_STOCKS, sizeof(*market_quotes));

    if (!stocks || !market || !stock_quotes || !market_quotes) {
        fprintf(stderr, "Out of memory.\n");
        exit_code = 1;
        goto cleanup;
    }

    if (read_stocks(opts.stocks_path, stocks) <= 0) {
        fprintf(stderr, "Stock watchlist is empty.\n");
        exit_code = 1;
        goto cleanup;
    }

    /* The market list is optional: the briefing still works without it. */
    if (read_stocks(opts.market_path, market) < 0)
        fprintf(stderr, "Continuing without market indices.\n");

    curl_global_init(CURL_GLOBAL_DEFAULT);

    fprintf(stderr, "Fetching market snapshot (%d symbols)...\n",
            market->count);
    collect_quotes(market, market_quotes, opts.offline, "market");

    fprintf(stderr, "Fetching watchlist snapshot (%d symbols)...\n",
            stocks->count);
    collect_quotes(stocks, stock_quotes, opts.offline, "stock");

    curl_global_cleanup();

    RunInfo info = {
        .generated_at = time(NULL),
        .offline = opts.offline,
        .stocks_path = opts.stocks_path,
        .market_path = opts.market_path,
    };

    print_research_pack(stdout, &info, stocks, stock_quotes,
                        market, market_quotes);

    if (opts.output_path) {
        FILE *file = fopen(opts.output_path, "w");
        if (!file) {
            fprintf(stderr, "Cannot write %s\n", opts.output_path);
            exit_code = 1;
            goto cleanup;
        }
        print_research_pack(file, &info, stocks, stock_quotes,
                            market, market_quotes);
        if (fclose(file) != 0) {
            fprintf(stderr, "Error writing %s\n", opts.output_path);
            exit_code = 1;
            goto cleanup;
        }
        fprintf(stderr, "Research input pack written to %s\n",
                opts.output_path);
    }

    fprintf(stderr, "Done.\n");

cleanup:
    free(stocks);
    free(market);
    free(stock_quotes);
    free(market_quotes);
    return exit_code;
}
