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

#endif /* ACTA_RUNNER_BACKEND_H */
