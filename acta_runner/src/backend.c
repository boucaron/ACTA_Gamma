/*
 * backend.c — minimal curl wrapper (runner phase 2).
 *
 * Single JSON HTTP request against the model's OpenAI-compatible
 * backend. See backend.h for the result contract. Deliberately small:
 * no connection pooling, no retries, no streaming (decision 5).
 */

#include "backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>
#include <curl/curlver.h>

/* The timeout CURLcode was renamed in curl 8.8:
 *   CURLE_OPERATION_TIMED (old)  ->  CURLE_OPERATION_TIMEDOUT (new)
 * (Enum values are not preprocessor macros, so select by version.) */
#if LIBCURL_VERSION_NUM >= 0x080800
#define ACTA_RUNNER_CURL_TIMED CURLE_OPERATION_TIMEDOUT
#else
#define ACTA_RUNNER_CURL_TIMED CURLE_OPERATION_TIMED
#endif

/* One-shot curl global init (process is single-threaded). */
static void curl_once(void)
{
    static int done = 0;
    if (!done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        done = 1;
    }
}

/* Growable NUL-terminated string buffer. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} buf_t;

static int buf_reserve(buf_t *b, size_t extra)
{
    size_t need = b->len + extra + 1;
    if (b->cap >= need)
        return 0;
    size_t ncap = b->cap ? b->cap : 256;
    while (ncap < need)
        ncap *= 2;
    char *nd = realloc(b->data, ncap);
    if (!nd)
        return -1;
    b->data = nd;
    b->cap = ncap;
    return 0;
}

static int buf_append(buf_t *b, const char *s, size_t n)
{
    if (buf_reserve(b, n) != 0)
        return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static void buf_free(buf_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static size_t on_body(char *ptr, size_t size, size_t nmemb, void *userp)
{
    buf_t *b = (buf_t *)userp;
    size_t n = size * nmemb;
    if (buf_append(b, ptr, n) != 0)
        return 0; /* abort the transfer on OOM */
    return n;
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
}

int backend_request(const char *method, const char *url,
                    const char *json_body,
                    const char *api_key,
                    int timeout_sec,
                    backend_response_t *out)
{
    if (!out)
        return BACKEND_ERR_ALLOC;
    out->http_status = 0;
    out->body = NULL;
    out->latency_ms = 0;

    if (!method || !url || timeout_sec <= 0)
        return BACKEND_ERR_ALLOC;
    if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0)
        return BACKEND_ERR_ALLOC;

    curl_once();

    CURL *h = curl_easy_init();
    if (!h)
        return BACKEND_ERR_TRANSPORT;

    struct curl_slist *hdrs = NULL;
    if (api_key && api_key[0]) {
        char auth[512];
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", api_key);
        hdrs = curl_slist_append(hdrs, auth);
    }
    if (strcmp(method, "POST") == 0)
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    if (!hdrs) {
        curl_easy_cleanup(h);
        return BACKEND_ERR_ALLOC;
    }

    buf_t body = { NULL, 0, 0 };

    long t0 = now_ms();
    CURLcode rc = CURLE_OK;
    {
        curl_easy_setopt(h, CURLOPT_URL, url);
        curl_easy_setopt(h, CURLOPT_TIMEOUT, (long)timeout_sec);
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, on_body);
        curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
        if (strcmp(method, "POST") == 0)
            curl_easy_setopt(h, CURLOPT_POSTFIELDS,
                             json_body ? json_body : "");
        rc = curl_easy_perform(h);
    }
    out->latency_ms = now_ms() - t0;

    if (rc != CURLE_OK) {
        if (rc == ACTA_RUNNER_CURL_TIMED) {
            buf_free(&body);
            return BACKEND_ERR_TIMEOUT;
        }
        /* Any other transport failure (DNS, connect refused, TLS, ...). */
        buf_free(&body);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(h);
        return BACKEND_ERR_TRANSPORT;
    }

    /* curl stores RESPONSE_CODE as long; copy through a long. */
    long code = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
    out->http_status = (int)code;
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(h);

    if (!body.data) {
        /* curl always allocates on the first write callback; if the
         * server sent an empty body, synthesize "". */
        body.data = strdup("");
        if (!body.data)
            return BACKEND_ERR_ALLOC;
    }
    out->body = body.data;
    return BACKEND_OK;
}

const char *backend_strerror(int rc)
{
    switch (rc) {
    case BACKEND_OK:            return "ok";
    case BACKEND_ERR_TRANSPORT: return "transport failure";
    case BACKEND_ERR_TIMEOUT:   return "timeout";
    case BACKEND_ERR_ALLOC:     return "allocation failure";
    default:                    return "unknown backend error";
    }
}
