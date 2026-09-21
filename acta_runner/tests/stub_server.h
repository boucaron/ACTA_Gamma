/*
 * stub_server.h — in-process stub OpenAI-compatible backend for tests.
 *
 * Serves the four endpoints the runner calls, on 127.0.0.1:
 *   GET  /health                 -> cfg->health_status (200 or 503)
 *   GET  /v1/models              -> one model with id cfg->model_id
 *   GET  /                       -> the llama.cpp model catalog (canned
 *                                    entry for cfg->model_id with fixed
 *                                    status.args + meta);
 *                                    cfg->catalog_status (default 200,
 *                                    404 simulates a non-llama backend)
 *   POST /v1/chat/completions    -> cfg->chat_status; on 200 a canned
 *                                    response whose choices[0].message.
 *                                    content is cfg->chat_content
 *
 * Single-threaded (one connection at a time) — the runner makes its
 * requests sequentially, so that is sufficient for tests.
 */
#ifndef ACTA_RUNNER_STUB_SERVER_H
#define ACTA_RUNNER_STUB_SERVER_H

typedef struct {
    int         port;          /* bind to 127.0.0.1:<port> */
    int         health_status; /* 200 (ready) or 503 (still loading) */
    const char *model_id;      /* id served by /v1/models */
    int         chat_status;   /* 200 by default, e.g. 500 for errors */
    const char *chat_error;    /* error message when chat_status != 200 */
    const char *chat_content;  /* message content on a 200 response */
    int         catalog_status; /* GET / response (default 200; 404 = no
                                     catalog, i.e. non-llama backend) */
    int         delay_ms;      /* sleep before replying (timeout tests) */
} stub_config_t;

/* Start the stub in a background thread. Returns 0 on success, -1 on
 * failure (bind/listen/thread create). */
int stub_server_start(const stub_config_t *cfg);

/* Stop the stub: wake the accept loop, join the thread, close the
 * listening socket. Safe to call after a failed start (no-op). */
int stub_server_stop(void);

/* The Authorization header value captured from the most recent request
 * (e.g. "Bearer <key>"), or NULL when no request sent one (e.g. keyless
 * run) or no request has arrived yet.  Reset on stub_server_start.
 * Lets tests assert which API key the runner actually used
 * (docs/plans/acta-config-file.md, work item 7: env-set-wins / file
 * fallback). */
const char *stub_server_last_auth(void);

#endif /* ACTA_RUNNER_STUB_SERVER_H */
