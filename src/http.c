/*
 * http.c - Minimal HTTP GET helper built on libcurl.
 */
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* Identify ourselves politely; some data providers reject empty agents. */
#define USER_AGENT "Mozilla/5.0 (compatible; daily-stock-briefing/2.0)"
#define TIMEOUT_SECONDS 20L

/* Store HTTP response (unchanged from the original program). */
static size_t write_callback(void *contents,
                             size_t size,
                             size_t nmemb,
                             void *userp) {
    size_t total = size * nmemb;
    Response *response = (Response *)userp;

    char *new_data = realloc(response->data,
                             response->size + total + 1);

    if (!new_data) return 0;

    response->data = new_data;

    memcpy(response->data + response->size,
           contents, total);

    response->size += total;
    response->data[response->size] = '\0';

    return total;
}

int http_get(const char *url, Response *response, long *status_out,
             char *err, size_t errlen) {
    *status_out = 0;
    if (errlen > 0) err[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(err, errlen, "curl_easy_init failed");
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status_out);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        snprintf(err, errlen, "network error: %s",
                 curl_easy_strerror(result));
        return 0;
    }

    if (*status_out < 200 || *status_out >= 300) {
        snprintf(err, errlen, "HTTP %ld", *status_out);
        return 0;
    }

    return 1;
}

void response_free(Response *response) {
    free(response->data);
    response->data = NULL;
    response->size = 0;
}
