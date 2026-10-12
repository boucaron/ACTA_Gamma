/*
 * stub_server.c — in-process stub OpenAI-compatible backend for tests.
 *
 * POSIX socket + pthread (works on MSYS2/MinGW and POSIX; on Windows the
 * winsock2 API provides the same socket names). Deliberately minimal:
 * reads a single request (header up to the blank line + POST body per
 * Content-Length), answers with a canned HTTP/1.1 response, closes the
 * connection. No keep-alive, no chunking, no TLS — the runner uses
 * plain http:// URLs.
 */

#include "stub_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define sock_close(s) closesocket(s)
/* winsock has no POSIX read(); recv() is the equivalent here. */
#define sock_read(s, buf, n) ((int)recv((s), (buf), (n), 0))
/* winsock has no SIGPIPE: a dropped connection is a plain error code. */
#define SEND_FLAGS 0
static int ws2_inited;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>   /* strncasecmp: POSIX declares it here, not in <string.h>
                         * (glibc only happened to pull it in transitively) */
#define sock_close(s) close(s)
#define sock_read(s, buf, n) read((s), (buf), (n))
/* POSIX raises SIGPIPE (default action: kill the process) when the client
 * has already closed the socket — which the runner does after reading its
 * response. MSG_NOSIGNAL turns it into an ordinary send() error instead. */
#define SEND_FLAGS MSG_NOSIGNAL
#endif

static pthread_t g_thread;
static int      g_running = 0;
static int      g_listen = -1;
static stub_config_t g_cfg;
static int      g_port = 0;
static char     g_last_auth[256]; /* Authorization value of last request */
static int      g_chat_requests = 0; /* POST /v1/chat/completions count */
static int      g_health_requests = 0; /* GET /health count */
static int      g_models_requests = 0; /* GET /v1/models count */
static int      g_catalog_requests = 0; /* GET / (llama.cpp catalog) count */

/* The most recent POST body, kept as a sliding window of the last
 * STUB_BODY_MAX bytes. The runner's request body ends with
 * "response_format", so keeping the TAIL is what makes the envelope
 * visible even if a very large prompt had to be truncated. */
#define STUB_BODY_MAX 16384
static char     g_body[STUB_BODY_MAX + 1];

static void msleep(int ms)
{
    if (ms <= 0)
        return;
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
#endif
}

/* JSON-escape `src` into `dst` (NUL-terminated). Returns chars written
 * excluding the terminator; truncates at dstsz-1. */
static size_t json_escape(char *dst, size_t dstsz, const char *src)
{
    size_t n = 0;
    for (const char *p = src; *p && n + 6 < dstsz; p++) {
        char c = *p;
        const char *rep = NULL;
        if (c == '"')       rep = "\\\"";
        else if (c == '\\') rep = "\\\\";
        else if (c == '\n') rep = "\\n";
        else if (c == '\r') rep = "\\r";
        else if (c == '\t') rep = "\\t";
        if (rep) {
            size_t rl = strlen(rep);
            if (n + rl >= dstsz)
                break;
            memcpy(dst + n, rep, rl);
            n += rl;
        } else {
            if ((unsigned char)c < 0x20) {
                if (n + 6 >= dstsz)
                    break;
                n += (size_t)sprintf(dst + n, "\\u%04x", (unsigned char)c);
            } else {
                dst[n++] = c;
            }
        }
    }
    dst[n] = '\0';
    return n;
}

/* Send one canned HTTP response and close the connection. */
static void send_response(int c, int status, const char *status_text,
                          const char *body)
{
    char head[256];
    int hl = snprintf(head, sizeof head,
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %d\r\n"
                      "Connection: close\r\n\r\n",
                      status, status_text, (int)strlen(body));
    if (hl > 0) {
        size_t sent = 0;
        while (sent < (size_t)hl) {
            int w = send(c, head + sent, (int)((size_t)hl - sent), SEND_FLAGS);
            if (w <= 0)
                break;
            sent += (size_t)w;
        }
    }
    size_t bl = strlen(body);
    if (bl > 0) {
        size_t sent = 0;
        while (sent < bl) {
            int w = send(c, body + sent, (int)(bl - sent), SEND_FLAGS);
            if (w <= 0)
                break;
            sent += (size_t)w;
        }
    }
    sock_close(c);
}

/* Case-insensitive search for an HTTP header field; returns the first
 * byte after the field name (i.e. the start of its value), or NULL.
 * (strcasestr is a POSIX extension, not in the mingw C library.) */
static const char *hdr_value(const char *hay, const char *field)
{
    size_t flen = strlen(field);
    for (const char *p = hay; *p; p++)
        if (strncasecmp(p, field, flen) == 0)
            return p + flen;
    return NULL;
}

/* Capture the Authorization header value (whitespace-trimmed) of one
 * request, if present. */
static void capture_auth(const char *req)
{
    const char *a = hdr_value(req, "Authorization:");
    if (!a) {
        g_last_auth[0] = '\0';
        return;
    }
    while (*a == ' ' || *a == '\t')
        a++;
    size_t n = 0;
    while (a[n] && a[n] != '\r' && a[n] != '\n' && n + 1 < sizeof g_last_auth) {
        g_last_auth[n] = a[n];
        n++;
    }
    g_last_auth[n] = '\0';
}

const char *stub_server_last_auth(void)
{
    return g_last_auth[0] ? g_last_auth : NULL;
}

int stub_server_chat_requests(void)
{
    return g_chat_requests;
}

int stub_server_health_requests(void)
{
    return g_health_requests;
}

int stub_server_models_requests(void)
{
    return g_models_requests;
}

int stub_server_catalog_requests(void)
{
    return g_catalog_requests;
}

/* The body of the most recent POST (a sliding window of the last
 * STUB_BODY_MAX bytes, tail-preserving), or NULL if no POST has been
 * served since stub_server_start. Lets a test assert what the runner
 * really put in the request: which envelope, and whether a parameter was
 * sent at all (e.g. top_k, which the openai dialect omits). */
const char *stub_server_last_chat_body(void)
{
    return g_body[0] ? g_body : NULL;
}

/*
 * Does this request body carry the response_format envelope `dialect`
 * expects?
 *
 *   llamacpp  {"type":"json_schema","schema":{…}}
 *   openai    {"type":"json_schema","json_schema":{"name":…,
 *              "strict":true,"schema":{…}}}
 *
 * The two are distinguishable textually: only the OpenAI envelope has a
 * `json_schema` KEY (the llama.cpp one has "json_schema" only as the
 * value of "type"). Returns 1 (accept) when no dialect is pinned — every
 * pre-existing scenario sends no dialect and must stay byte-identical —
 * and when the request carries no response_format at all (a skill without
 * output_schema, or supports_response_format:false).
 */
static int envelope_ok(const char *dialect, const char *body)
{
    if (!dialect || !dialect[0])
        return 1;
    if (!body || !body[0])
        return 1;
    if (!strstr(body, "\"response_format\""))
        return 1;                       /* no envelope sent: always fine */

    int nested = strstr(body, "\"json_schema\":{") != NULL;  /* OpenAI wrapper */
    int schema = strstr(body, "\"schema\":") != NULL;         /* a schema object */

    if (strcmp(dialect, "openai") == 0)
        return nested && schema;
    if (strcmp(dialect, "llamacpp") == 0)
        return !nested && schema;
    return 1;   /* an unknown label pins nothing */
}

/* The refusal body of the dialect that was violated. The openai wording is
 * the message Strata really returns (docs/session_strata_backend.md,
 * issue A); the llamacpp wording is the mirror-image refusal the stub
 * invents for the same mistake in the other direction. */
static const char *envelope_error(const char *dialect)
{
    if (dialect && strcmp(dialect, "openai") == 0)
        return "response_format.json_schema needs a schema object";
    return "response_format.schema is required for type json_schema";
}

/* Read the request (header + body) and serve one response. */
static void handle_connection(int c, const stub_config_t *cfg)
{
    char req[16384];
    size_t total = 0;

    /* Read until the blank line (end of header) or EOF. */
    for (;;) {
        if (total + 1 >= sizeof req)
            break;
        int n = sock_read(c, req + total, 1);
        if (n <= 0)
            break;
        total++;   /* the byte is already in req[total-1] */
        if (total >= 4 && memcmp(req + total - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    req[total] = '\0';

    capture_auth(req);

    char method[16] = "", path[256] = "";
    if (sscanf(req, "%15s %255s", method, path) < 2) {
        send_response(c, 400, "Bad Request",
                      "{\"error\":{\"code\":400,\"message\":\"bad request\","
                      "\"type\":\"invalid_request_error\"}}");
        return;
    }

    /* POST body per Content-Length — captured (bounded, tail-preserving)
     * so the chat handler can validate the response_format envelope.
     * Only POST bodies land in g_body: it keeps the LAST chat request body
     * across the GETs of the same pipeline (stub_server_last_chat_body()). */
    if (strcmp(method, "POST") == 0) {
        size_t body_got = 0;
        const char *cl = hdr_value(req, "Content-Length:");
        long body_len = cl ? atol(cl) : 0;
        if (body_len < 0)
            body_len = 0;
        long drained = 0;
        char drain[1024];
        while (drained < body_len) {
            int chunk = (int)(body_len - drained > 1024
                                                        ? 1024
                                                        : body_len - drained);
            int n = sock_read(c, drain, chunk);
            if (n <= 0)
                break;
            drained += n;
            if (body_got + (size_t)n > STUB_BODY_MAX) {
                size_t keep = STUB_BODY_MAX - (size_t)n;
                memmove(g_body, g_body + (body_got - keep), keep);
                body_got = keep;
            }
            memcpy(g_body + body_got, drain, (size_t)n);
            body_got += (size_t)n;
        }
        g_body[body_got] = '\0';
    }

    if (cfg->delay_ms > 0)
        msleep(cfg->delay_ms);

    if (strcmp(path, "/health") == 0 || strcmp(path, "/v1/health") == 0) {
        g_health_requests++;
        if (cfg->health_status == 503)
            send_response(c, 503, "Service Unavailable",
                          "{\"error\":{\"code\":503,"
                          "\"message\":\"Loading model\","
                          "\"type\":\"unavailable_error\"}}");
        else if (cfg->health_max_context > 0) {
            /* Servers without a per-model max_context in /v1/models
             * (Strata) expose the ceiling here instead. */
            char body[128];
            snprintf(body, sizeof body,
                     "{\"status\":\"ok\",\"max_context\":%ld}",
                     cfg->health_max_context);
            send_response(c, 200, "OK", body);
        } else
            send_response(c, 200, "OK", "{\"status\":\"ok\"}");
        return;
    }

    if (strcmp(path, "/v1/models") == 0) {
        g_models_requests++;
        if (cfg->models_status != 200) {
            char body[512];
            snprintf(body, sizeof body,
                     "{\"error\":{\"code\":%d,"
                     "\"message\":\"server error\","
                     "\"type\":\"server_error\"}}",
                     cfg->models_status);
            send_response(c, cfg->models_status,
                          cfg->models_status == 500 ? "Internal Server Error"
                                                    : "Error",
                          body);
            return;
        }
        char body[512];
        char id[128];
        json_escape(id, sizeof id, cfg->model_id ? cfg->model_id : "");
        /* Optional entry fields: `max_context` (llama.cpp router) and/or
         * `meta.n_ctx` (OpenAI-dialect servers). Which one the stub serves
         * decides which ceiling source backend_preflight() must pick. */
        char extra[128];
        extra[0] = '\0';
        if (cfg->max_context > 0)
            snprintf(extra + strlen(extra), sizeof extra - strlen(extra),
                     ",\"max_context\":%ld", cfg->max_context);
        if (cfg->models_meta_n_ctx > 0)
            snprintf(extra + strlen(extra), sizeof extra - strlen(extra),
                     ",\"meta\":{\"n_ctx\":%ld}", cfg->models_meta_n_ctx);
        snprintf(body, sizeof body,
                 "{\"object\":\"list\",\"data\":[{\"id\":\"%s\","
                 "\"object\":\"model\"%s}]}", id, extra);
        send_response(c, 200, "OK", body);
        return;
    }

    if (strcmp(path, "/") == 0) {
        g_catalog_requests++;
        if (cfg->catalog_status != 200) {
            send_response(c, cfg->catalog_status,
                          cfg->catalog_status == 404 ? "Not Found" : "Error",
                          "{\"error\":{\"code\":404,"
                          "\"message\":\"not found\","
                          "\"type\":\"not_found_error\"}}");
            return;
        }
        /* Canned catalog entry (the llama.cpp "models.json" shape):
         * one model matching cfg->model_id with fixed launch args and
         * meta, so tests can assert the catalog logging (R8). */
        char body[1024];
        char id[128];
        json_escape(id, sizeof id, cfg->model_id ? cfg->model_id : "");
        snprintf(body, sizeof body,
                 "{\"object\":\"list\",\"data\":[{\"id\":\"%s\","
                 "\"status\":{\"value\":\"loaded\","
                 "\"args\":[\"llama-server\",\"--ctx-size\",\"162000\","
                 "\"--temperature\",\"1.0\"]},"
                 "\"meta\":{\"n_ctx\":162048,\"n_params\":27320697856,"
                 "\"size\":10431832064,\"ftype\":\"IQ3_S - 3.4375 bpw\"}}]}",
                 id);
        send_response(c, 200, "OK", body);
        return;
    }

    if (strcmp(path, "/v1/chat/completions") == 0) {
        g_chat_requests++;
        /* Dialect-shaped server: the wrong envelope is a 400 before any
         * generation happens (this is exactly the failure that made a
         * skill with output_schema fail against Strata). */
        if (!envelope_ok(cfg->dialect, g_body)) {
            char body[512];
            snprintf(body, sizeof body,
                     "{\"error\":{\"code\":400,\"message\":\"%s\","
                     "\"type\":\"invalid_request_error\"}}",
                     envelope_error(cfg->dialect));
            send_response(c, 400, "Bad Request", body);
            return;
        }
        if (cfg->chat_status != 200) {
            char body[512];
            char emsg[256];
            json_escape(emsg, sizeof emsg,
                        cfg->chat_error ? cfg->chat_error : "server error");
            snprintf(body, sizeof body,
                     "{\"error\":{\"code\":%d,\"message\":\"%s\","
                     "\"type\":\"server_error\"}}",
                     cfg->chat_status, emsg);
            send_response(c, cfg->chat_status,
                          cfg->chat_status == 500 ? "Internal Server Error"
                                                  : "Error",
                          body);
            return;
        }
        char content[1024];
        json_escape(content, sizeof content,
                    cfg->chat_content ? cfg->chat_content : "stub-response");
        char body[2048];
        char id[128];
        json_escape(id, sizeof id, cfg->model_id ? cfg->model_id : "");
        snprintf(body, sizeof body,
                 "{\"id\":\"chatcmpl-stub\",\"object\":\"chat.completion\","
                 "\"model\":\"%s\",\"choices\":[{\"index\":0,"
                 "\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},"
                 "\"finish_reason\":\"stop\"}],"
                 "\"usage\":{\"prompt_tokens\":7,\"completion_tokens\":3,"
                 "\"total_tokens\":10},"
                 "\"timings\":{\"prompt_ms\":11,\"predicted_ms\":42,"
                 "\"predicted_per_second\":12.5}}",
                 id, content);
        send_response(c, 200, "OK", body);
        return;
    }

    send_response(c, 404, "Not Found",
                  "{\"error\":{\"code\":404,\"message\":\"not found\","
                  "\"type\":\"not_found_error\"}}");
}

static void *server_main(void *arg)
{
    (void)arg;
    while (g_running) {
        int c = accept(g_listen, NULL, NULL);
        if (c < 0)
            break;
        if (!g_running) {
            sock_close(c);
            break;
        }
        handle_connection(c, &g_cfg);
    }
    return NULL;
}

int stub_server_start(const stub_config_t *cfg)
{
    if (!cfg || cfg->port <= 0)
        return -1;

#ifdef _WIN32
    if (!ws2_inited) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return -1;
        ws2_inited = 1;
    }
#endif

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&addr, sizeof addr) != 0) {
        sock_close(s);
        return -1;
    }
    if (listen(s, 4) != 0) {
        sock_close(s);
        return -1;
    }

    g_listen = s;
    g_port = cfg->port;
    g_cfg = *cfg;
    if (g_cfg.catalog_status == 0)
        g_cfg.catalog_status = 200;
    if (g_cfg.models_status == 0)
        g_cfg.models_status = 200;
    g_last_auth[0] = '\0';
    g_body[0] = '\0';
    g_chat_requests = 0;
    g_health_requests = 0;
    g_models_requests = 0;
    g_catalog_requests = 0;
    g_running = 1;
    if (pthread_create(&g_thread, NULL, server_main, NULL) != 0) {
        g_running = 0;
        sock_close(s);
        g_listen = -1;
        return -1;
    }
    return 0;
}

int stub_server_stop(void)
{
    if (!g_running)
        return 0;

    g_running = 0;

    /* Wake the blocked accept() with a throwaway connection. */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)g_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int w = socket(AF_INET, SOCK_STREAM, 0);
    if (w >= 0) {
        (void)connect(w, (struct sockaddr *)&addr, sizeof addr);
        sock_close(w);
    }

    pthread_join(g_thread, NULL);
    if (g_listen >= 0) {
        sock_close(g_listen);
        g_listen = -1;
    }
    return 0;
}
