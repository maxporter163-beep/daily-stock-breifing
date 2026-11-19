/*
 * http.h - Minimal HTTP GET helper built on libcurl.
 *
 * The Response buffer and write callback are carried over from the
 * original single-file version of this program (which used them for the
 * Anthropic and Twilio requests). They now back plain GET requests for
 * market data.
 */
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Growable buffer that collects an HTTP response body. */
typedef struct {
    char *data;
    size_t size;
} Response;

/*
 * Perform an HTTP GET request.
 *
 *   url        - fully formed URL (caller must URL-encode any parameters)
 *   response   - zero-initialised Response; filled with the body on return
 *   status_out - receives the HTTP status code (0 if no response)
 *   err, errlen- receives a human-readable error message on failure
 *
 * Returns 1 if the request completed with a 2xx status, otherwise 0.
 * The caller must always call response_free() afterwards.
 */
int http_get(const char *url, Response *response, long *status_out,
             char *err, size_t errlen);

/* Release the memory held by a Response and reset it. */
void response_free(Response *response);

#endif /* HTTP_H */
