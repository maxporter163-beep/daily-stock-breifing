/*
 * report.h - Write the "research input pack" that Claude Code reads.
 *
 * The pack is Markdown: run metadata, market and watchlist snapshots,
 * data-quality notes, and the research brief (the analyst instructions
 * that used to be sent to the Anthropic API from this program).
 */
#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>
#include <time.h>

#include "quotes.h"
#include "watchlist.h"

typedef struct {
    time_t generated_at;
    int offline;
    const char *stocks_path;
    const char *market_path;
} RunInfo;

void print_research_pack(FILE *out, const RunInfo *info,
                         const Watchlist *stocks, const Quote *stock_quotes,
                         const Watchlist *market, const Quote *market_quotes);

#endif /* REPORT_H */
