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
 * When cfg->dialect is set, the stub also plays the role of a dialect-
 * specific server and rejects a request whose response_format envelope is
 * the other dialect's shape (see stub_server.c envelope_ok()). With
 * cfg->dialect == NULL no request body is inspected at all, which is what
 * every pre-existing scenario relies on.
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
    int         models_status;  /* /v1/models response (default 200; e.g.
                                     500 simulates a catalog failure) */
    long        max_context;    /* max_context of the served /v1/models
                                     entry (0 = field omitted) */
    long        models_meta_n_ctx; /* meta.n_ctx of the served /v1/models
                                     entry (0 = field omitted); the shape
                                     OpenAI-dialect servers (Strata) use
                                     instead of a top-level max_context */
    long        health_max_context; /* max_context in the GET /health body
                                     (0 = field omitted); the last-resort
                                     ceiling source */
    const char *dialect;     /* "llamacpp" | "openai": the envelope this
                                  stub accepts on POST /v1/chat/completions.
                                  NULL (the default) = inspect no request
                                  body, i.e. every pre-existing scenario
                                  unchanged. */
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
 * (docs/runner_contract.md, decision 4: env-set-wins / file
 * fallback). */
const char *stub_server_last_auth(void);

/* Number of POST /v1/chat/completions requests received since the last
 * stub_server_start. Reset on stub_server_start. The "zero tokens"
 * assertion hook for the `check` suite: every `check` scenario must
 * leave this at 0 (docs/runner_contract.md, check action). */
int stub_server_chat_requests(void);

/* Number of GET /health requests received since the last
 * stub_server_start (reset on stub_server_start). Used by the
 * auto-preflight suite to pin "once per acta_runner invocation":
 * an N-row --pending
 * batch makes 1 + N /health calls (one pre-claim auto preflight plus
 * one per-execution pipeline preflight), not 2 * N. */
int stub_server_health_requests(void);

/* Number of GET /v1/models requests received since the last
 * stub_server_start (reset on stub_server_start); same "once per
 * invocation" assertion as stub_server_health_requests(). */
int stub_server_models_requests(void);

/* Number of GET / requests received since the last stub_server_start
 * (reset on stub_server_start) — the llama.cpp catalog audit. It pins
 * decision 4 of docs/plans/backend_dialects.md §9: the openai dialect
 * must make ZERO catalog calls (fetch_catalog = 0, "catalog": null),
 * while the llamacpp dialect makes exactly one per pipeline run. */
int stub_server_catalog_requests(void);

/* Body of the most recent POST served since stub_server_start (a
 * tail-preserving window of the last 16 KiB), or NULL when no POST has
 * been served. Lets a test assert the request the runner really built:
 * which response_format envelope, and whether a parameter such as top_k
 * was sent at all. */
const char *stub_server_last_chat_body(void);

#endif /* ACTA_RUNNER_STUB_SERVER_H */
