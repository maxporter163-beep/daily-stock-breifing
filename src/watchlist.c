/*
 * watchlist.c - Load symbol lists from config files.
 *
 * Evolved from the original read_stocks(), which concatenated the file
 * into one string for the AI prompt. It now parses each line into a
 * structured entry so the program can fetch data per symbol.
 */
#include "watchlist.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Remove leading and trailing whitespace in place; returns the new start. */
static char *trim(char *text) {
    while (isspace((unsigned char)*text)) text++;

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = '\0';

    return text;
}

/* Symbols end up in URLs, so only allow the characters tickers use. */
static int valid_symbol(const char *symbol) {
    if (*symbol == '\0') return 0;

    for (const char *p = symbol; *p; p++) {
        if (!isalnum((unsigned char)*p) &&
            strchr(".^=-_", *p) == NULL)
            return 0;
    }
    return 1;
}

int read_stocks(const char *path, Watchlist *list) {
    FILE *file = fopen(path, "r");

    list->count = 0;

    if (!file) {
        fprintf(stderr, "Cannot open %s\n", path);
        return -1;
    }

    char line[256];
    int line_number = 0;

    while (fgets(line, sizeof(line), file)) {
        line_number++;

        /* Split off an inline "# note". */
        char *note = strchr(line, '#');
        if (note) *note++ = '\0';

        char *symbol = trim(line);
        if (*symbol == '\0')
            continue; /* blank line or whole-line comment */

        if (!valid_symbol(symbol) || strlen(symbol) >= SYMBOL_LEN) {
            fprintf(stderr, "%s:%d: skipping invalid symbol '%s'\n",
                    path, line_number, symbol);
            continue;
        }

        if (list->count >= MAX_STOCKS) {
            fprintf(stderr, "%s: more than %d symbols, ignoring the rest\n",
                    path, MAX_STOCKS);
            break;
        }

        WatchItem *item = &list->items[list->count++];
        snprintf(item->symbol, sizeof(item->symbol), "%s", symbol);
        snprintf(item->note, sizeof(item->note), "%s",
                 note ? trim(note) : "");
    }

    fclose(file);
    return list->count;
}
