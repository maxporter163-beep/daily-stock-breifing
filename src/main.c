
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define MAX_STOCKS 100

typedef struct {
    char *data;
    size_t size;
} Response;

/* Store HTTP response */
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

/* Read stock watchlist */
char *read_stocks(void) {
    FILE *file = fopen("config/stocks.txt", "r");

    if (!file) {
        fprintf(stderr, "Cannot open stocks.txt\n");
        return NULL;
    }

    char *stocks = calloc(1, 8192);
    char line[256];

    if (!stocks) {
        fclose(file);
        return NULL;
    }

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (strlen(stocks) + strlen(line) >= 8192)
            break;

        strcat(stocks, line);
    }

    fclose(file);
    return stocks;
}

/* Make a POST request to Claude */
char *get_claude_report(const char *stocks) {

    const char *api_key = getenv("ANTHROPIC_API_KEY");
    const char *model = getenv("CLAUDE_MODEL");

    if (!api_key || !model) {
        fprintf(stderr, "Missing Claude API configuration\n");
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    Response response = {0};

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "model", model);
    cJSON_AddNumberToObject(root, "max_tokens", 1800);

    cJSON *tools = cJSON_AddArrayToObject(root, "tools");
    cJSON *search = cJSON_CreateObject();

    cJSON_AddStringToObject(search, "type",
                            "web_search_20250305");
    cJSON_AddStringToObject(search, "name", "web_search");
    cJSON_AddNumberToObject(search, "max_uses", 8);

    cJSON_AddItemToArray(tools, search);

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    cJSON *message = cJSON_CreateObject();

    cJSON_AddStringToObject(message, "role", "user");

    char prompt[12000];

    snprintf(prompt, sizeof(prompt),

        "You are my daily Wall Street equity research analyst.\n\n"

        "Research these stocks:\n%s\n"

        "Produce a concise daily investment research briefing.\n\n"

        "FOR EACH STOCK INCLUDE:\n"

        "1. Analyst consensus:\n"
        "   - Buy, Hold and Sell ratings.\n"
        "   - Number of analysts in each category.\n"
        "   - Consensus rating.\n\n"

        "2. Price target changes:\n"
        "   - Recent analyst upgrades or downgrades.\n"
        "   - Previous and revised price targets.\n"
        "   - Analyst firm and date of change.\n"
        "   - Consensus price target and implied upside/downside "
        "if reliable current share-price data is available.\n\n"

        "3. Key catalysts:\n"
        "   - Upcoming earnings announcements.\n"
        "   - Product launches and company announcements.\n"
        "   - Industry developments.\n"
        "   - Regulatory developments.\n"
        "   - Important risks and negative developments.\n\n"

        "4. Overall outlook:\n"
        "   - Most important development affecting the stock.\n"
        "   - Bullish and bearish considerations.\n"
        "   - What investors should watch next.\n\n"

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
        "Start with today's date.\n"
        "Use clear headings for each ticker.\n"
        "Finish with the three most important developments "
        "across the entire watchlist.\n"
        "Keep the SMS briefing concise, ideally around "
        "1200 characters. Prioritise useful information.",

        stocks
    );

    cJSON_AddStringToObject(message, "content", prompt);
    cJSON_AddItemToArray(messages, message);

    char *json = cJSON_PrintUnformatted(root);

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers,
        "Content-Type: application/json");

    headers = curl_slist_append(headers,
        "anthropic-version: 2023-06-01");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "x-api-key: %s", api_key);

    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL,
        "https://api.anthropic.com/v1/messages");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180L);

    CURLcode result = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    char *report = NULL;

    if (result == CURLE_OK && status >= 200 && status < 300) {

        cJSON *parsed = cJSON_Parse(response.data);
        cJSON *content = cJSON_GetObjectItem(parsed, "content");

        if (cJSON_IsArray(content)) {
            size_t capacity = 1;
            size_t used = 0;
            report = calloc(1, capacity);

            cJSON *block;

            cJSON_ArrayForEach(block, content) {
                cJSON *type = cJSON_GetObjectItem(block, "type");
                cJSON *text = cJSON_GetObjectItem(block, "text");

                if (cJSON_IsString(type) &&
                    strcmp(type->valuestring, "text") == 0 &&
                    cJSON_IsString(text)) {

                    size_t len = strlen(text->valuestring);

                    char *next = realloc(report, used + len + 2);

                    if (!next) {
                        free(report);
                        report = NULL;
                        break;
                    }

                    report = next;

                    if (used > 0)
                        report[used++] = '\n';

                    memcpy(report + used, text->valuestring, len);
                    used += len;
                    report[used] = '\0';
                }
            }
        }

        cJSON_Delete(parsed);

    } else {
        fprintf(stderr, "Claude API error: %ld\n", status);
        if (response.data)
            fprintf(stderr, "%s\n", response.data);
    }

    free(response.data);
    free(json);
    cJSON_Delete(root);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return report;
}

/* Send report through Twilio */
int send_sms(const char *report) {

    const char *sid = getenv("TWILIO_ACCOUNT_SID");
    const char *token = getenv("TWILIO_AUTH_TOKEN");
    const char *from = getenv("TWILIO_FROM");
    const char *to = getenv("PHONE_NUMBER");

    if (!sid || !token || !from || !to) {
        fprintf(stderr, "Missing Twilio configuration\n");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char *encoded_body = curl_easy_escape(curl, report, 0);
    char *encoded_from = curl_easy_escape(curl, from, 0);
    char *encoded_to = curl_easy_escape(curl, to, 0);

    if (!encoded_body || !encoded_from || !encoded_to) {
        curl_free(encoded_body);
        curl_free(encoded_from);
        curl_free(encoded_to);
        curl_easy_cleanup(curl);
        return 0;
    }

    char *post_data = malloc(
        strlen(encoded_body) +
        strlen(encoded_from) +
        strlen(encoded_to) + 32
    );

    if (!post_data) {
        curl_free(encoded_body);
        curl_free(encoded_from);
        curl_free(encoded_to);
        curl_easy_cleanup(curl);
        return 0;
    }

    sprintf(post_data, "Body=%s&From=%s&To=%s",
            encoded_body, encoded_from, encoded_to);

    char url[256];

    snprintf(url, sizeof(url),
        "https://api.twilio.com/2010-04-01/"
        "Accounts/%s/Messages.json", sid);

    Response response = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

    curl_easy_setopt(curl, CURLOPT_USERNAME, sid);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, token);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode result = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    int success = (result == CURLE_OK &&
                   status >= 200 && status < 300);

    if (success) {
        printf("SMS successfully submitted to Twilio.\n");
    } else {
        fprintf(stderr, "SMS failed: HTTP %ld\n", status);
        if (response.data)
            fprintf(stderr, "%s\n", response.data);
    }

    free(post_data);
    free(response.data);

    curl_free(encoded_body);
    curl_free(encoded_from);
    curl_free(encoded_to);

    curl_easy_cleanup(curl);

    return success;
}

/* Main program */
int main(void) {

    curl_global_init(CURL_GLOBAL_DEFAULT);

    printf("Starting daily stock briefing...\n");

    char *stocks = read_stocks();

    if (!stocks || strlen(stocks) == 0) {
        fprintf(stderr, "Stock watchlist is empty.\n");
        free(stocks);
        curl_global_cleanup();
        return 1;
    }

    printf("Generating Claude research report...\n");

    char *report = get_claude_report(stocks);

    free(stocks);

    if (!report || strlen(report) == 0) {
        fprintf(stderr, "Failed to generate report.\n");
        free(report);
        curl_global_cleanup();
        return 1;
    }

    printf("Research report generated.\n");

    printf("Sending SMS...\n");

    int success = send_sms(report);

    free(report);
    curl_global_cleanup();

    return success ? 0 : 1;
}
