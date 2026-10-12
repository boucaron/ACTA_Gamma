/*
 * dialect.h — the HTTP dialect of a model backend (runner phase 3).
 *
 * A "dialect" is NOT a backend product and NOT a model: it is the shape of
 * the HTTP conversation acta_runner has with one server. Exactly two
 * dialects exist (docs/plans/backend_dialects.md §2):
 *
 *   llamacpp — a local llama.cpp llama-server, including its router
 *              catalog at GET / and its `{"type":"json_schema","schema":…}`
 *              structured-output envelope.
 *   openai   — a strict OpenAI-compatible chat-completions server (Strata,
 *              OpenAI, vLLM, …): no llama.cpp catalog, and the OpenAI
 *              `{"type":"json_schema","json_schema":{name,strict,schema}}`
 *              envelope.
 *
 * Everything else (one chat call, no retries, no streaming, the
 * choices[0].message.content extraction, exit codes, log phases) is
 * shared and must stay shared. Differences that are NOT dialect
 * differences (this server has no schema support, that one needs a
 * longer timeout) belong in model.configuration, not here.
 *
 * Where the dialect comes from (docs/plans/backend_dialects.md §3.2):
 *   1. --dialect, where the CLI accepts it (check standalone mode; the
 *      logged `run <id>` override),
 *   2. otherwise model_revision.backend,
 *   3. otherwise the caller fails with EXIT_INVALID BEFORE any HTTP call.
 * There is no auto-detection and no silent default on the run path.
 *
 * This module is pure: no HTTP, no DB, no globals. It maps a name to a
 * small descriptor and builds the one request fragment that genuinely
 * varies.
 */
#ifndef ACTA_RUNNER_DIALECT_H
#define ACTA_RUNNER_DIALECT_H

#include <cjson/cJSON.h>

typedef enum {
    DIALECT_LLAMACPP = 0,
    DIALECT_OPENAI   = 1,
    DIALECT_UNKNOWN  = -1
} dialect_id_t;

/*
 * The descriptor of one dialect. `fetch_catalog` and `sends_top_k` are
 * plain flags; the two function pointers cover the two shapes that
 * actually vary. A new field must first appear in the dialect matrix
 * (docs/plans/backend_dialects.md §2).
 */
typedef struct {
    dialect_id_t id;
    const char  *name;              /* canonical name: "llamacpp" | "openai" */

    /* 1 = the pipeline preflight audits the server catalog with GET /.
     * 0 = no such endpoint exists on this dialect: skip the call and
     * record "catalog": null. */
    int fetch_catalog;

    /* 1 = include top_k in the chat request when model.configuration
     * sets it. 0 = top_k is not part of this dialect's request shape
     * and is omitted. */
    int sends_top_k;

    /*
     * Build this dialect's `response_format` value for a skill
     * output_schema, or NULL when `schema` is NULL. The returned tree
     * is owned by the caller (cJSON_Delete) and holds a DEEP COPY of
     * `schema`, which is never modified or freed here.
     */
    cJSON *(*build_response_format)(const cJSON *schema);

    /*
     * Context ceiling this dialect's server reports for a matched
     * /v1/models entry, with `health_body` (possibly NULL) as the
     * last-resort source. Returns 0 when the server reports none.
     * When `source` is non-NULL it receives a static label naming the
     * field the value came from ("entry.max_context" |
     * "entry.meta.n_ctx" | "health.max_context" | "none"), so the log
     * can say which one was used.
     */
    long (*max_context_of)(const cJSON *model_entry,
                           const cJSON *health_body,
                           const char **source);
} dialect_t;

/*
 * Map a backend string to a dialect id. Exact, case-sensitive match on
 * the canonical names only: "llamacpp" -> DIALECT_LLAMACPP,
 * "openai" -> DIALECT_OPENAI. Anything else — NULL, "", "llama",
 * "vllm", "OpenAI", surrounding whitespace — is DIALECT_UNKNOWN, so an
 * ambiguous value fails loudly instead of being guessed (no dialect
 * auto-detection, docs/plans/backend_dialects.md §4 option C).
 */
dialect_id_t dialect_from_string(const char *backend);

/* Descriptor for a dialect id, or NULL for DIALECT_UNKNOWN. */
const dialect_t *dialect_get(dialect_id_t id);

/* Canonical name of a dialect id, or "(unknown)". */
const char *dialect_name(dialect_id_t id);

/*
 * The wording of the accepted-vocabulary error, single-sourced here so
 * every surface (run, check, model create/update) states the same
 * vocabulary: "llamacpp or openai".
 */
const char *dialect_expected_names(void);

#endif /* ACTA_RUNNER_DIALECT_H */
