/*
 * backend.h — minimal curl wrapper over the model's OpenAI-compatible
 * backend (runner phase 2).
 *
 * One function: backend_request() performs a single JSON HTTP request
 * (GET with no body, or POST with a JSON body) against an arbitrary
 * URL, with a hard timeout and an optional
 * `Authorization: Bearer <key>` header. No retries, no streaming —
 * failures are first-class artifacts (docs/runner_contract.md,
 * decisions 4/5).
 *
 * Result contract:
 *   BACKEND_OK            — HTTP exchange completed (any status code).
 *                           out->http_status holds the status,
 *                           out->body holds the (possibly empty) body.
 *   BACKEND_ERR_TRANSPORT — DNS/connect/TLS/curl failure before a
 *                           complete response (http_status = 0).
 *   BACKEND_ERR_TIMEOUT   — the request hit the CURLOPT_TIMEOUT.
 *                           (http_status = 0.)
 *   BACKEND_ERR_CANCELED  — the request was aborted by the cooperative
 *                           cancel flag (UI cancel button).
 *                           (http_status = 0.)
 *   BACKEND_ERR_ALLOC     — local allocation failure.
 *
 * out->body is always NUL-terminated and heap-allocated; free() it.
 */
#ifndef ACTA_RUNNER_BACKEND_H
#define ACTA_RUNNER_BACKEND_H

/* Result codes. */
#define BACKEND_OK            0
#define BACKEND_ERR_TRANSPORT (-1)
#define BACKEND_ERR_TIMEOUT   (-2)
#define BACKEND_ERR_ALLOC     (-3)
#define BACKEND_ERR_CANCELED  (-4)

/*
 * Cooperative cancellation (UI "Cancel" button).
 *
 * A process-global flag: the app runs at most one runner pipeline at a
 * time, and the CLI never requests a cancel, so a plain global is
 * sufficient (and lets the GUI thread set it while the worker thread
 * reads it in the curl xferinfo callback and between phases).
 */
void backend_cancel_request(void);
void backend_cancel_reset(void);
int  backend_cancel_requested(void);

/* Result of one backend_request() call. */
typedef struct {
    int   http_status;  /* 0 when the transport failed */
    char *body;         /* heap-allocated, NUL-terminated ("" if empty) */
    long  latency_ms;   /* wall-clock duration of the request */
} backend_response_t;

/*
 * Perform one HTTP request.
 *
 *   method     — "GET" or "POST".
 *   url        — full URL, e.g. "http://127.0.0.1:8080/v1/chat/completions"
 *   json_body  — request body for POST (NULL for GET; ignored on GET).
 *   api_key    — if non-NULL and non-empty, sends
 *                "Authorization: Bearer <api_key>".
 *   timeout_sec — hard wall-clock timeout for the whole exchange.
 *   out        — receives the response (body must be free()'d by caller).
 *
 * Returns a BACKEND_* code (see header comment).
 */
int backend_request(const char *method, const char *url,
                    const char *json_body,
                    const char *api_key,
                    int timeout_sec,
                    backend_response_t *out);

/* Human-readable string for a BACKEND_* code. */
const char *backend_strerror(int rc);

/*
 * Token-free backend preflight (shared by the `run` pipeline and the
 * standalone `check` action).
 *
 * Performs the two cheap GETs — GET /health, then GET /v1/models (only
 * after /health succeeds: a 503 server is "loading", not "unknown
 * model") — and classifies the outcome, so both callers share one
 * request/response path (docs/plans/runner-health-check.md). No POST,
 * no chat/completions: this is the zero-token surface.
 *
 * Result codes:
 *   PREFLIGHT_OK               — /health is 200 and model_id is present
 *                                in the catalog; out->max_context holds
 *                                the entry's max_context (0 when absent).
 *   PREFLIGHT_CANCELED         — a request was aborted by the cooperative
 *                                cancel flag (GUI only; the CLI never
 *                                triggers it).
 *   PREFLIGHT_HEALTH_TIMEOUT   — the /health call hit the timeout.
 *   PREFLIGHT_HEALTH_TRANSPORT — /health transport failure
 *                                (DNS/connect/TLS/...); out->brc is the
 *                                raw BACKEND_* code.
 *   PREFLIGHT_HEALTH_NOT_200   — /health returned a non-200 status;
 *                                out->http_status holds it (503 = "model
 *                                still loading").
 *   PREFLIGHT_MODELS_TIMEOUT   — the /v1/models call hit the timeout.
 *   PREFLIGHT_MODELS_TRANSPORT — /v1/models transport failure.
 *   PREFLIGHT_MODELS_NOT_200   — /v1/models returned a non-200 status;
 *                                out->http_status holds it.
 *   PREFLIGHT_MODELS_UNPARSEABLE — the /v1/models body is not JSON.
 *   PREFLIGHT_MODEL_NOT_SERVED — catalog is OK but model_id is not one
 *                                of the served ids; out->available_ids
 *                                holds the served ids (comma list).
 *
 * Caller contract: non-empty base_url and model_id, timeout_sec > 0,
 * out non-NULL. `api_key` is forwarded to backend_request() (the run
 * pipeline sends its key on every request, including these GETs —
 * docs/runner_contract.md decision 4; the `check` action passes NULL,
 * it is keyless by design).
 */
typedef enum {
    PREFLIGHT_OK = 0,
    PREFLIGHT_CANCELED,
    PREFLIGHT_HEALTH_TIMEOUT,
    PREFLIGHT_HEALTH_TRANSPORT,
    PREFLIGHT_HEALTH_NOT_200,
    PREFLIGHT_MODELS_TIMEOUT,
    PREFLIGHT_MODELS_TRANSPORT,
    PREFLIGHT_MODELS_NOT_200,
    PREFLIGHT_MODELS_UNPARSEABLE,
    PREFLIGHT_MODEL_NOT_SERVED
} preflight_result_t;

/* Outcome of backend_preflight(). */
typedef struct {
    int   brc;              /* raw BACKEND_* code of the failing call (0 if OK) */
    int   http_status;      /* status of the failing call (0 if transport) */
    long  max_context;      /* catalog max_context (0 when unknown) */
    char  available_ids[512]; /* served ids, comma list (NOT_SERVED only) */
} backend_preflight_t;

/*
 * Run the token-free preflight: GET /health, then GET /v1/models.
 * Returns a preflight_result_t code; `out` is filled accordingly.
 */
int backend_preflight(const char *base_url, const char *model_id,
                      const char *api_key,
                      int timeout_sec, backend_preflight_t *out);

#endif /* ACTA_RUNNER_BACKEND_H */
