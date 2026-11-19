/*
 * quotes.c - Fetch delayed market data for one symbol.
 *
 * The JSON handling follows the same cJSON pattern the original program
 * used to read Claude's API response.
 */
#include "quotes.h"
#include "http.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>

#define CHART_URL \
    "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=1mo&interval=1d"

/* Copy a JSON string field into a fixed buffer if present. */
static void copy_string(const cJSON *object, const char *key,
                        char *dest, size_t destlen) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsString(item) && item->valuestring)
        snprintf(dest, destlen, "%s", item->valuestring);
}

/* Read a JSON number field; returns 1 if present. */
static int get_number(const cJSON *object, const char *key, double *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item)) return 0;
    *out = item->valuedouble;
    return 1;
}

/* Percent-encode a symbol for use in a URL path (e.g. ^NZ50 -> %5ENZ50). */
static void url_encode(const char *in, char *out, size_t outlen) {
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;

    for (; *in && used + 4 < outlen; in++) {
        unsigned char c = (unsigned char)*in;
        if (isalnum(c) || c == '.' || c == '-' || c == '_') {
            out[used++] = (char)c;
        } else {
            out[used++] = '%';
            out[used++] = hex[c >> 4];
            out[used++] = hex[c & 0x0F];
        }
    }
    out[used] = '\0';
}

/* Percentage change from `from` to `to`; returns 0 if `from` is unusable. */
static int percent_change(double from, double to, double *out) {
    if (from <= 0.0) return 0;
    *out = (to - from) / from * 100.0;
    return 1;
}

/*
 * Derive day and one-month changes from the daily closing prices.
 * The last bar may be today's (equal to the live price) or the previous
 * session's, so pick the reference close accordingly.
 */
static void derive_changes(const cJSON *closes, Quote *quote) {
    double values[64];
    int count = 0;
    const cJSON *item;

    cJSON_ArrayForEach(item, closes) {
        if (cJSON_IsNumber(item) && count < 64)
            values[count++] = item->valuedouble;
    }

    if (count == 0) return;

    if (!quote->has_change) {
        double last = values[count - 1];
        double reference = last;
        /* If the last close is the live price, compare with the one before. */
        if (count >= 2 && last == quote->price) reference = values[count - 2];
        if (reference != quote->price)
            quote->has_change =
                percent_change(reference, quote->price, &quote->change_pct);
    }

    quote->has_month_change =
        percent_change(values[0], quote->price, &quote->month_change_pct);
}

/* Fill the quote from a parsed chart response. Returns 1 on success. */
static int parse_chart(const cJSON *root, Quote *quote) {
    const cJSON *chart = cJSON_GetObjectItemCaseSensitive(root, "chart");
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(chart, "error");
    const cJSON *results = cJSON_GetObjectItemCaseSensitive(chart, "result");

    if (cJSON_IsObject(error)) {
        const cJSON *desc =
            cJSON_GetObjectItemCaseSensitive(error, "description");
        snprintf(quote->error, sizeof(quote->error), "%s",
                 cJSON_IsString(desc) ? desc->valuestring : "provider error");
        return 0;
    }

    const cJSON *result = cJSON_GetArrayItem(results, 0);
    const cJSON *meta = cJSON_GetObjectItemCaseSensitive(result, "meta");

    if (!cJSON_IsObject(meta) ||
        !get_number(meta, "regularMarketPrice", &quote->price)) {
        snprintf(quote->error, sizeof(quote->error), "no price in response");
        return 0;
    }

    copy_string(meta, "shortName", quote->name, sizeof(quote->name));
    copy_string(meta, "longName", quote->name, sizeof(quote->name));
    copy_string(meta, "fullExchangeName", quote->exchange,
                sizeof(quote->exchange));
    copy_string(meta, "currency", quote->currency, sizeof(quote->currency));

    double value;
    if (get_number(meta, "priceHint", &value) && value >= 0 && value <= 6)
        quote->price_hint = (int)value;
    if (get_number(meta, "regularMarketTime", &value))
        quote->market_time = (time_t)value;
    if (get_number(meta, "regularMarketChangePercent", &value)) {
        quote->change_pct = value;
        quote->has_change = 1;
    }
    quote->has_52w = get_number(meta, "fiftyTwoWeekHigh", &quote->high_52w) &&
                     get_number(meta, "fiftyTwoWeekLow", &quote->low_52w);

    const cJSON *indicators =
        cJSON_GetObjectItemCaseSensitive(result, "indicators");
    const cJSON *bars = cJSON_GetArrayItem(
        cJSON_GetObjectItemCaseSensitive(indicators, "quote"), 0);
    const cJSON *closes = cJSON_GetObjectItemCaseSensitive(bars, "close");
    if (cJSON_IsArray(closes)) derive_changes(closes, quote);

    return 1;
}

static void init_quote(const char *symbol, Quote *quote) {
    memset(quote, 0, sizeof(*quote));
    snprintf(quote->symbol, sizeof(quote->symbol), "%s", symbol);
    quote->price_hint = 2;
}

void skip_quote(const char *symbol, Quote *quote) {
    init_quote(symbol, quote);
    quote->status = QUOTE_SKIPPED;
    snprintf(quote->error, sizeof(quote->error), "offline mode");
}

void fetch_quote(const char *symbol, Quote *quote) {
    init_quote(symbol, quote);
    quote->status = QUOTE_UNAVAILABLE;

    char encoded[96];
    char url[256];
    url_encode(symbol, encoded, sizeof(encoded));
    snprintf(url, sizeof(url), CHART_URL, encoded);

    Response response = {0};
    long status = 0;
    char err[128];
    int ok = http_get(url, &response, &status, err, sizeof(err));

    /* Yahoo returns a JSON error body with HTTP 404 for unknown symbols. */
    cJSON *root = response.data ? cJSON_Parse(response.data) : NULL;

    if (root && parse_chart(root, quote)) {
        if (quote->price <= 0.0) {
            quote->status = QUOTE_STALE;
            snprintf(quote->error, sizeof(quote->error),
                     "provider returned a zero price");
        } else if (quote->market_time > 0 &&
                   time(NULL) - quote->market_time > STALE_AFTER_SECONDS) {
            quote->status = QUOTE_STALE;
            snprintf(quote->error, sizeof(quote->error),
                     "last price is more than 7 days old");
        } else {
            quote->status = QUOTE_OK;
        }
    } else if (quote->error[0] == '\0') {
        snprintf(quote->error, sizeof(quote->error), "%s",
                 ok ? "could not parse response" : err);
    }

    cJSON_Delete(root);
    response_free(&response);
}

const char *quote_status_label(QuoteStatus status) {
    switch (status) {
    case QUOTE_OK:          return "OK";
    case QUOTE_STALE:       return "STALE";
    case QUOTE_UNAVAILABLE: return "UNAVAILABLE";
    case QUOTE_SKIPPED:     return "SKIPPED";
    }
    return "UNKNOWN";
}
