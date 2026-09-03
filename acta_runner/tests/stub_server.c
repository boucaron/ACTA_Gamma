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
#define sock_close(s) closesocket(s)
/* winsock has no POSIX read(); recv() is the equivalent here. */
#define sock_read(s, buf, n) ((int)recv((s), (buf), (n), 0))
static int ws2_inited;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define sock_close(s) close(s)
#define sock_read(s, buf, n) read((s), (buf), (n))
#endif

static pthread_t g_thread;
static int      g_running = 0;
static int      g_listen = -1;
static stub_config_t g_cfg;
static int      g_port = 0;

static void msleep(int ms)
{
    if (ms <= 0)
        return;
#ifdef _WIN32
    _sleep(ms); /* mingw time.h */
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
            int w = send(c, head + sent, (int)((size_t)hl - sent), 0);
            if (w <= 0)
                break;
            sent += (size_t)w;
        }
    }
    size_t bl = strlen(body);
    if (bl > 0) {
        size_t sent = 0;
        while (sent < bl) {
            int w = send(c, body + sent, (int)(bl - sent), 0);
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

    char method[16] = "", path[256] = "";
    if (sscanf(req, "%15s %255s", method, path) < 2) {
        send_response(c, 400, "Bad Request",
                      "{\"error\":{\"code\":400,\"message\":\"bad request\","
                      "\"type\":\"invalid_request_error\"}}");
        return;
    }

    /* POST body per Content-Length. */
    if (strcmp(method, "POST") == 0) {
        const char *cl = hdr_value(req, "Content-Length:");
        long body_len = cl ? atol(cl) : 0;
        if (body_len < 0)
            body_len = 0;
        /* The runner sends small JSON bodies; we do not inspect them,
         * so no buffer is needed — just drain it. */
        long drained = 0;
        char drain[1024];
        while (drained < body_len) {
            int chunk = (int)(body_len - drained > 1024 ? 1024
                                                        : body_len - drained);
            int n = sock_read(c, drain, chunk);
            if (n <= 0)
                break;
            drained += n;
        }
    }

    if (cfg->delay_ms > 0)
        msleep(cfg->delay_ms);

    if (strcmp(path, "/health") == 0 || strcmp(path, "/v1/health") == 0) {
        if (cfg->health_status == 503)
            send_response(c, 503, "Service Unavailable",
                          "{\"error\":{\"code\":503,"
                          "\"message\":\"Loading model\","
                          "\"type\":\"unavailable_error\"}}");
        else
            send_response(c, 200, "OK", "{\"status\":\"ok\"}");
        return;
    }

    if (strcmp(path, "/v1/models") == 0) {
        char body[512];
        char id[128];
        json_escape(id, sizeof id, cfg->model_id ? cfg->model_id : "");
        snprintf(body, sizeof body,
                 "{\"object\":\"list\",\"data\":[{\"id\":\"%s\","
                 "\"object\":\"model\"}]}", id);
        send_response(c, 200, "OK", body);
        return;
    }

    if (strcmp(path, "/v1/chat/completions") == 0) {
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
