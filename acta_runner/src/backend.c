/*
 * backend.c — minimal curl wrapper (runner phase 2).
 *
 * Single JSON HTTP request against the model's OpenAI-compatible
 * backend. See backend.h for the result contract. Deliberately small:
 * no connection pooling, no retries, no streaming (decision 5).
 */

#include "backend.h"
#include "runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>
#include <curl/curlver.h>

#include <cjson/cJSON.h>

/* The timeout CURLcode (value 28) is CURLE_OPERATION_TIMEDOUT in curl >= 7.50;
 * older headers spelled the enumerator CURLE_OPERATION_TIMEOUTED. Enum values
 * are not preprocessor macros, so the choice must be made on the version. */
#if LIBCURL_VERSION_NUM >= 0x073200 /* 7.50.0 */
#define ACTA_RUNNER_CURL_TIMED CURLE_OPERATION_TIMEDOUT
#else
#define ACTA_RUNNER_CURL_TIMED CURLE_OPERATION_TIMEOUTED
#endif

/*
 * Cooperative cancel flag (UI "Cancel" button). Process-global on
 * purpose: the app runs at most one pipeline at a time and the CLI
 * never requests a cancel, so a plain volatile global lets the GUI
 * thread set it while the worker thread reads it.
 */
static volatile int backend_cancel_flag = 0;

void backend_cancel_request(void) { backend_cancel_flag = 1; }
void backend_cancel_reset(void)   { backend_cancel_flag = 0; }
int  backend_cancel_requested(void) { return backend_cancel_flag != 0; }

/*
 * Xferinfo callback: returning non-zero aborts the in-flight transfer
 * (curl reports CURLE_ABORTED_BY_CALLBACK). This is how a cancel
 * request interrupts a long-running HTTP exchange.
 */
static int on_xferinfo(void *userp, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t ultotal, curl_off_t ulnow)
{
    (void)userp; (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
    return backend_cancel_flag ? 1 : 0;
}

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
    int hdr_fail = 0;
    if (api_key && api_key[0]) {
        /* Size the header for the key: a fixed buffer would silently
         * truncate a long api_key and send a wrong credential.
         * curl_slist_append copies the string, so it is freed right away. */
        const char *prefix = "Authorization: Bearer ";
        size_t auth_len = strlen(prefix) + strlen(api_key) + 1;
        char *auth = malloc(auth_len);
        if (!auth)
            hdr_fail = 1;
        else {
            snprintf(auth, auth_len, "%s%s", prefix, api_key);
            hdrs = curl_slist_append(hdrs, auth);
            free(auth);
            if (!hdrs)
                hdr_fail = 1;
        }
    }
    if (strcmp(method, "POST") == 0) {
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
        if (!hdrs)
            hdr_fail = 1;
    }
    if (hdr_fail) {
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(h);
        return BACKEND_ERR_ALLOC;
    }
    /* hdrs may legitimately be NULL (e.g. GET without an api key). */

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
        curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, on_xferinfo);
        curl_easy_setopt(h, CURLOPT_XFERINFODATA, NULL);
        if (strcmp(method, "POST") == 0)
            curl_easy_setopt(h, CURLOPT_POSTFIELDS,
                             json_body ? json_body : "");
        rc = curl_easy_perform(h);
    }
    out->latency_ms = now_ms() - t0;

    if (rc != CURLE_OK) {
        if (rc == CURLE_ABORTED_BY_CALLBACK) {
            buf_free(&body);
            return BACKEND_ERR_CANCELED;
        }
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
    case BACKEND_ERR_CANCELED:  return "cancelled";
    default:                    return "unknown backend error";
    }
}

/* Build "<base_url><path>" with the base's trailing slash normalized
 * (run.c keeps its own copy for the GET / and chat URLs). */
static void build_url(char *out, size_t outsz, const char *base, const char *path)
{
    size_t n = base ? strlen(base) : 0;
    while (n > 0 && base[n - 1] == '/')
        n--;
    snprintf(out, outsz, "%.*s%s", (int)n, base ? base : "", path);
}

/*
 * The context ceiling is a DIALECT property (dialect_t.max_context_of):
 * llama.cpp reports it as the entry's top-level max_context, OpenAI-
 * dialect servers (Strata) as the entry's meta.n_ctx and/or in the
 * /health body. backend_preflight() supplies the two documents it has -
 * the matched /v1/models entry and the parsed /health body - and reports
 * the value plus the field it came from, so the log says WHICH source
 * produced the ceiling (docs/plans/backend_dialects.md, issue B).
 */

int backend_preflight(const char *base_url, const char *model_id,
                      const dialect_t *dialect,
                      const char *api_key,
                      int timeout_sec, backend_preflight_t *out)
{
    if (out) {
        out->brc = 0;
        out->http_status = 0;
        out->max_context = 0;
        out->max_context_source[0] = '\0';
        out->available_ids[0] = '\0';
    }
    /* Defensive: every caller resolves the dialect before calling (an
     * unknown model backend must fail BEFORE any HTTP call). A missing
     * descriptor falls back to the dialect this code implemented before
     * dialects existed, rather than guessing a server. */
    if (!dialect)
        dialect = dialect_get(DIALECT_LLAMACPP);

    char url[1024];
    backend_response_t r;
    /* The parsed /health body lives until the ceiling is resolved: it is
     * the dialect's last-resort ceiling source. */
    cJSON *jh = NULL;

    /* ---- GET /health ---- */
    build_url(url, sizeof url, base_url, "/health");
    int brc = backend_request("GET", url, NULL, api_key, timeout_sec, &r);
    if (brc != BACKEND_OK) {
        free(r.body);
        if (out)
            out->brc = brc;
        if (brc == BACKEND_ERR_CANCELED)
            return PREFLIGHT_CANCELED;
        if (brc == BACKEND_ERR_TIMEOUT)
            return PREFLIGHT_HEALTH_TIMEOUT;
        return PREFLIGHT_HEALTH_TRANSPORT;
    }
    if (out)
        out->http_status = r.http_status;

    /* Parse the /health body before it is released (200 only): it is the
     * last-resort ceiling document. A non-JSON body yields no fallback. */
    if (r.http_status == 200)
        jh = cJSON_Parse(r.body);
    free(r.body);
    if (r.http_status != 200)
        return PREFLIGHT_HEALTH_NOT_200;

    /* ---- GET /v1/models (only after /health succeeded) ---- */
    build_url(url, sizeof url, base_url, "/v1/models");
    brc = backend_request("GET", url, NULL, api_key, timeout_sec, &r);
    if (brc != BACKEND_OK) {
        free(r.body);
        cJSON_Delete(jh);
        if (out)
            out->brc = brc;
        if (brc == BACKEND_ERR_CANCELED)
            return PREFLIGHT_CANCELED;
        if (brc == BACKEND_ERR_TIMEOUT)
            return PREFLIGHT_MODELS_TIMEOUT;
        return PREFLIGHT_MODELS_TRANSPORT;
    }
    if (out)
        out->http_status = r.http_status;
    if (r.http_status != 200) {
        free(r.body);
        cJSON_Delete(jh);
        return PREFLIGHT_MODELS_NOT_200;
    }
    cJSON *jm = cJSON_Parse(r.body);
    free(r.body);
    if (!jm) {
        cJSON_Delete(jh);
        return PREFLIGHT_MODELS_UNPARSEABLE;
    }

    /* The server may serve several models; scan ALL entries for the
     * model_id (not just data[0]). */
    cJSON *data = cJSON_GetObjectItem(jm, "data");
    cJSON *match = NULL;
    if (data && cJSON_IsArray(data)) {
        int n = cJSON_GetArraySize(data);
        for (int i = 0; i < n; i++) {
            cJSON *it = cJSON_GetArrayItem(data, i);
            cJSON *sid = it ? cJSON_GetObjectItem(it, "id") : NULL;
            if (cJSON_IsString(sid) && sid->valuestring &&
                strcmp(sid->valuestring, model_id) == 0) {
                match = it;
                break;
            }
        }
    }
    if (!match) {
        /* Build a list of the ids the server actually has, for the
         * caller's error/verdict message. */
        char avail[512];
        avail[0] = 0;
        if (data && cJSON_IsArray(data)) {
            int n = cJSON_GetArraySize(data);
            for (int i = 0; i < n; i++) {
                cJSON *it = cJSON_GetArrayItem(data, i);
                cJSON *sid = it ? cJSON_GetObjectItem(it, "id") : NULL;
                if (!cJSON_IsString(sid) || !sid->valuestring)
                    continue;
                size_t left = sizeof avail - strlen(avail) - 1;
                if (left > 0)
                    snprintf(avail + strlen(avail), left, "%s%s",
                             avail[0] ? ", " : "",
                             sid->valuestring);
            }
        }
        if (out)
            snprintf(out->available_ids, sizeof out->available_ids,
                     "%s", avail);
        cJSON_Delete(jm);
        cJSON_Delete(jh);
        return PREFLIGHT_MODEL_NOT_SERVED;
    }
    const char *source = NULL;
    long ceiling = dialect->max_context_of(match, jh, &source);
    if (out) {
        out->max_context = ceiling;
        snprintf(out->max_context_source, sizeof out->max_context_source,
                 "%s", source ? source : "none");
    }
    cJSON_Delete(jm);
    cJSON_Delete(jh);
    return PREFLIGHT_OK;
}

/* The `check` verdict mapping, shared by the `check` action and the run
 * pre-claim auto preflight:
 * one classification, one set of exit codes. PREFLIGHT_OK -> NULL verdict
 * and exit 0; every failure -> a verdict string + 12 (HTTP) or 13
 * (timeout), exactly the `check` contract.
 */
const char *preflight_verdict(int prc, const backend_preflight_t *pf,
                             int *exit_code)
{
    const char *verdict;
    int code;
    switch (prc) {
    case PREFLIGHT_OK:
        verdict = NULL;
        code = EXIT_OK;
        break;
    case PREFLIGHT_CANCELED:
        /* Defensive dead path for the CLI: the cooperative cancel flag
         * is only ever set by the GUI. */
        /* deliberate fallthrough to "server unreachable" */
    case PREFLIGHT_HEALTH_TRANSPORT:
        verdict = "server unreachable";
        code = EXIT_HTTP;
        break;
    case PREFLIGHT_HEALTH_TIMEOUT:
        verdict = "server unreachable";
        code = EXIT_TIMEOUT;
        break;
    case PREFLIGHT_HEALTH_NOT_200:
        verdict = (pf && pf->http_status == 503)
            ? "model still loading" : "server unreachable";
        code = EXIT_HTTP;
        break;
    case PREFLIGHT_MODELS_TIMEOUT:
        verdict = "catalog unreachable";
        code = EXIT_TIMEOUT;
        break;
    case PREFLIGHT_MODELS_TRANSPORT:
    case PREFLIGHT_MODELS_NOT_200:
    case PREFLIGHT_MODELS_UNPARSEABLE:
        verdict = "catalog unreachable";
        code = EXIT_HTTP;
        break;
    case PREFLIGHT_MODEL_NOT_SERVED:
        verdict = "model not served";
        code = EXIT_HTTP;
        break;
    default:
        verdict = "server unreachable";
        code = EXIT_HTTP;
        break;
    }
    if (exit_code)
        *exit_code = code;
    return verdict;
}
