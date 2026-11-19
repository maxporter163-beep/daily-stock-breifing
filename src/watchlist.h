/*
 * watchlist.h - Load symbol lists from config files.
 *
 * File format (one entry per line):
 *
 *     SYMBOL            # optional note shown in the output
 *
 * Blank lines and lines starting with '#' are ignored. Symbols use
 * Yahoo Finance notation so the exchange is unambiguous, e.g.
 * FPH.NZ (NZX), SMI.AX (ASX), RKLB (US), ^NZ50 (index), NZDUSD=X (FX).
 */
#ifndef WATCHLIST_H
#define WATCHLIST_H

#define MAX_STOCKS 100 /* kept from the original program */
#define SYMBOL_LEN 32
#define NOTE_LEN 128

typedef struct {
    char symbol[SYMBOL_LEN];
    char note[NOTE_LEN];
} WatchItem;

typedef struct {
    WatchItem items[MAX_STOCKS];
    int count;
} Watchlist;

/*
 * Read a watchlist file into `list`.
 * Returns the number of symbols loaded, or -1 if the file cannot be opened.
 * Invalid lines are reported on stderr and skipped.
 */
int read_stocks(const char *path, Watchlist *list);

#endif /* WATCHLIST_H */
