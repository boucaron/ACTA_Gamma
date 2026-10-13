/*
 * acta_runner — "check" action (token-free backend health check)
 *
 * Per docs/runner_contract.md (check action): verifies the backend and a
 * specific model WITHOUT any chat completion — zero tokens, zero
 * inference, zero execution rows, no DB writes of any kind. It is the
 * token-free pre-flight for a `run --pending` batch and the first
 * triage step after a `failed` backend call.
 *
 *   acta_runner check <model-record-id>                  # DB mode
 *   acta_runner check --base-url <url> --model-identifier <id>   # standalone
 *                               [--dialect llamacpp|openai]
 *
 * The dialect (src/dialect.h) comes from the model row in DB mode — an
 * unsupported row backend exits 4 before any HTTP call — and from
 * --dialect in standalone mode, default llamacpp. --dialect with a
 * <model-record-id> is rejected: the row decides.
 *
 * Check sequence (all token-free, via backend_preflight):
 *   1. GET /health       — 200 ok; 503 "model still loading";
 *                          connection refused / DNS / timeout
 *                          "server unreachable"; any other status
 *                          "server unreachable".
 *   2. GET /v1/models    — only after /health succeeds (a 503 server
 *                          is "loading", not "unknown model"):
 *                          model id present -> ok (max_context
 *                          reported); absent -> "model not served";
 *                          endpoint error / unparseable body ->
 *                          "catalog unreachable".
 *
 * Result contract (one JSON line on stdout, scriptable — emitted via
 * emit_check_verdict in runner_util.h, shared with the run pre-claim
 * auto preflight):
 *   success: {"ok":true,"model":"<id>","max_context":<n>,"base_url":"<url>"}
 *   failure: {"ok":false,"model":"<id>","base_url":"<url>",
 *             "verdict":"model still loading" | "server unreachable"
 *                       | "model not served" | "catalog unreachable"}
 *
 * Exit codes (the runner's existing code space, no new numbers):
 *   0  — all checks passed
 *   1  — unknown <model-record-id> (DB mode only)
 *   4  — invalid arguments (flag conflicts, bad --timeout, malformed
 *        config file — the file is always read, for the api-key fallback)
 *   12 — EXIT_HTTP: /health non-200, model not in catalog, HTTP-level
 *        failure
 *   13 — EXIT_TIMEOUT: a check call hit --timeout
 *
 * No POST /v1/chat/completions — ever. No execution_log rows, no
 * execution state change, no DB writes of any kind. No streaming, no
 * retry loop (consistent with the product decisions).
 *
 * The two GETs carry an Authorization header ONLY when a key resolves
 * from $OPENAI_API_KEY (if set, even empty) -> the config file
 * "api_key" (docs/runner_contract.md decision 4). That is what makes
 * `check` work on a key-protected server (llama-server started with
 * --api-key: its /v1/models answers 401 without the key; /health stays
 * open). With no key anywhere the calls are keyless, exactly as before:
 * keyless servers are unchanged, and a missing key is NOT a hard error
 * here (unlike `run`).
 */

#include "runner.h"
#include "runner_util.h"
#include "argparse.h"
#include "acta_db.h"
#include "backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_usage(FILE *out)
{
    fputs(
        "Usage: acta_runner check <model-record-id> [flags]\n"
        "       acta_runner check --base-url <url> --model-identifier <id> [flags]\n"
        "\n"
        "Verifies the backend and a model WITHOUT any chat completion\n"
        "(zero tokens, zero execution rows, no DB writes):\n"
        "  1. GET /health      — 200 ok, 503 = model still loading,\n"
        "                        unreachable = server down\n"
        "  2. GET /v1/models   — the model id must be in the catalog\n"
        "\n"
        "  <model-record-id>       models row id (DB mode; the record's\n"
        "                          base_url + model_identifier are used)\n"
        "  --base-url <url>        standalone mode: probe this server\n"
        "  --model-identifier <id> standalone mode (with --base-url);\n"
        "                          mutually exclusive with the positional\n"
        "                          <model-record-id>\n"
        "  --timeout <sec>         Backend timeout in seconds (default:\n"
        "                          config file \"timeout\", else 600)\n"
        "  --dialect <llamacpp|openai>\n"
        "                          STANDALONE MODE ONLY (with --base-url and\n"
        "                          --model-identifier): which dialect the\n"
        "                          server speaks, since a standalone probe\n"
        "                          has no model row. Default llamacpp. Not\n"
        "                          accepted with a <model-record-id>: there\n"
        "                          the dialect comes from the model row.\n"
        "  Key: $OPENAI_API_KEY (if set), else the config file \"api_key\";\n"
        "       when none is available the calls are keyless (keyless\n"
        "       servers are unchanged; a missing key is not an error)\n"
        "\n"
        "stdout: {\"ok\":true,...,\"max_context\":N} on success, or\n"
        "{\"ok\":false,...,\"verdict\":\"<reason>\"} on failure.\n"
        "Exit: 0 ok | 1 model not found | 4 invalid | 12 http |\n"
        "13 timeout\n",
        out);
}

int cmd_check(cmd_args_t *ga, const global_opts_t *gopts, db_t *db)
{
    (void)gopts;
    char msg[192];

    int has_base = cmd_args_has_flag(ga, "base-url");
    int has_mid  = cmd_args_has_flag(ga, "model-identifier");
    int standalone = has_base && has_mid;

    const char *v = cmd_args_flag(ga, "timeout", 1);
    int timeout = 0; /* parsed --timeout; 0 = flag not given */
    if (v) {
        if (!parse_nonneg_int(v, &timeout) || timeout == 0) {
            VLOG(1, "cmd_check: ERROR --timeout must be a positive integer, got '%s'", v);
            emit_error("--timeout must be a positive integer");
            check_usage(stderr);
            return EXIT_INVALID;
        }
    }

    /* Per-machine settings resolution
     * (docs/runner_contract.md, decisions 4 and 5). The config file is
     * read in EVERY case now, not only when --timeout is absent: it is
     * the fallback source of the api key below. The --timeout flag still
     * wins outright for the timeout, but a readable-but-malformed file
     * is a hard error (fail-closed, like everywhere else) even when the
     * flag is present.
     *   timeout: --timeout flag -> config file "timeout"
     *            -> built-in default (600 s);
     *   api key: $OPENAI_API_KEY (if set, even empty) -> config file
     *            "api_key" -> keyless. A set env var (even empty)
     *            always shadows the file key (decision 4). Unlike
     *            `run`, a missing key is NOT a hard error: the check
     *            simply stays keyless, so keyless servers are unchanged. */
    acta_conf_t conf;
    int conf_missing = 0;
    char *conf_err = NULL;
    if (acta_conf_read(acta_conf_default_path(), &conf, &conf_missing,
                       &conf_err) != 0) {
        const char *what = conf_err ? conf_err : "config file is invalid";
        VLOG(1, "cmd_check: ERROR %s", what);
        int rc = emit_runner_error(EXIT_INVALID, what);
        free(conf_err);
        return rc;
    }
    int timeout_sec = (timeout > 0)
                        ? timeout
                        : acta_conf_resolve_timeout(&conf, 0);
    char *api_key = NULL;
    {
        const char *env_key = getenv("OPENAI_API_KEY");
        const char *file_key = conf.api_key; /* NULL: missing/absent key */
        const char *src = (env_key != NULL) ? env_key : file_key;
        if (src && src[0] != '\0')
            api_key = strdup(src);
    }
    acta_conf_free(&conf);
    VLOG(2, "cmd_check: api_key=%s",
         api_key ? "present (Authorization header sent)"
                 : "absent (keyless, no Authorization header)");

    const char *pos = cmd_args_next_positional(ga);
    if (standalone && pos) {
        VLOG(1, "cmd_check: ERROR --base-url/--model-identifier conflict with a <model-record-id> positional");
        emit_error("--base-url/--model-identifier conflict with a "
                   "<model-record-id> positional");
        check_usage(stderr);
        return EXIT_INVALID;
    }
    if (!standalone) {
        if (has_base || has_mid) {
            VLOG(1, "cmd_check: ERROR standalone flags must be used together");
            emit_error("--base-url and --model-identifier must be used "
                       "together (or pass a <model-record-id>)");
            check_usage(stderr);
            return EXIT_INVALID;
        }
        if (!pos) {
            VLOG(1, "cmd_check: ERROR missing <model-record-id> (or use --base-url + --model-identifier)");
            emit_error("missing <model-record-id>: pass an id or use "
                       "--base-url + --model-identifier");
            check_usage(stderr);
            return EXIT_INVALID;
        }
        if (!db) {
            VLOG(1, "cmd_check: ERROR no database handle (internal)");
            emit_error("internal error: check (DB mode) needs the "
                       "database handle");
            return EXIT_INVALID;
        }
    }

    /* --dialect is the standalone-mode escape hatch: a standalone probe
     * has no model row, so the dialect has nowhere else to come from
     * (docs/plans/backend_dialects.md §3.2). With a <model-record-id> it
     * is rejected outright — the row decides, and a flag that silently
     * changed the probe shape for a stored row is exactly what the
     * revision design forbids. Validated before the model-row lookup:
     * `check` never writes to the DB in any mode. */
    dialect_id_t cli_dialect = DIALECT_UNKNOWN; /* UNKNOWN = not given */
    if (cmd_args_has_flag(ga, "dialect")) {
        const char *dval = cmd_args_flag(ga, "dialect", 1);
        if (!dval || !dval[0]) {
            VLOG(1, "cmd_check: ERROR --dialect requires a value (%s)",
                 dialect_expected_names());
            emit_error("--dialect requires a value");
            check_usage(stderr);
            return EXIT_INVALID;
        }
        cli_dialect = dialect_from_string(dval);
        if (cli_dialect == DIALECT_UNKNOWN) {
            snprintf(msg, sizeof msg, "invalid --dialect '%s' (expected %s)",
                     dval, dialect_expected_names());
            VLOG(1, "cmd_check: ERROR %s", msg);
            emit_error(msg);
            check_usage(stderr);
            return EXIT_INVALID;
        }
        if (!standalone) {
            VLOG(1, "cmd_check: ERROR --dialect is standalone-mode only");
            emit_error("--dialect is only valid with --base-url and "
                       "--model-identifier: check <model-record-id> takes "
                       "the dialect from the model row");
            check_usage(stderr);
            return EXIT_INVALID;
        }
    }

    char *base = NULL, *mid = NULL; /* heap copies, freed before every return */
    char *backend = NULL;           /* DB mode: the row's backend label */
    {
        const char *base_src, *mid_src, *backend_src;
        model_t *m = NULL;
        if (standalone) {
            base_src = cmd_args_flag(ga, "base-url", 1);
            mid_src  = cmd_args_flag(ga, "model-identifier", 1);
            backend_src = NULL;   /* no row: the dialect comes from
                                     --dialect (default llamacpp) */
        } else {
            int id = 0;
            if (!parse_positive_id(pos, &id)) {
                snprintf(msg, sizeof msg,
                         "invalid <model-record-id>: '%s' must be a positive integer",
                         pos);
                VLOG(1, "cmd_check: %s", msg);
                emit_error(msg);
                check_usage(stderr);
                return EXIT_INVALID;
            }
            /* Read-only model-row lookup; no execution is created,
             * claimed, or logged. Soft-deleted rows are not found
             * (get_live). */
            int err = ACTA_DB_OK;
            m = acta_db_model_get_live(db, id, &err);
            if (err != ACTA_DB_OK)
                return finish_op_error(db, err, "model_get_live");
            if (!m) {
                char what[128];
                snprintf(what, sizeof what, "model %d not found", id);
                return finish_db_error(ACTA_DB_ERR_NOT_FOUND, what);
            }
            if (!m->base_url || m->base_url[0] == '\0' ||
                !m->model_identifier || m->model_identifier[0] == '\0') {
                acta_db_model_free(m);
                m = NULL;
                snprintf(msg, sizeof msg,
                         "model %d has no base_url or model_identifier", id);
                VLOG(1, "cmd_check: %s", msg);
                return emit_runner_error(EXIT_INVALID, msg);
            }
            base_src = m->base_url;
            mid_src  = m->model_identifier;
            backend_src = m->backend;
        }
        /* Copy while the row is still alive: base_src/mid_src point into m,
         * so the model must not be released before these strdup()s. */
        base = strdup(base_src);
        mid  = strdup(mid_src);
        backend = backend_src ? strdup(backend_src) : NULL;
        if (m) acta_db_model_free(m);
        if (!base || !mid || (!standalone && !backend)) {
            free(base);
            free(mid);
            free(backend);
            free(api_key);
            return EXIT_ALLOC;
        }
    }

    /* The dialect decides how the context ceiling is read out of
     * /v1/models and /health (backend_preflight). In DB mode it is the
     * model revision/row property, validated BEFORE any HTTP call: an
     * unsupported backend exits 4 with an explicit message instead of
     * probing a server whose dialect is unknown. In standalone mode there
     * is no row, so --dialect decides — default llamacpp, i.e. the
     * historical behavior (docs/plans/backend_dialects.md §3.2). */
    const dialect_t *dialect;
    if (standalone) {
        dialect = dialect_get(cli_dialect != DIALECT_UNKNOWN
                              ? cli_dialect : DIALECT_LLAMACPP);
        VLOG(2, "cmd_check: dialect=%s (%s)", dialect->name,
             cli_dialect != DIALECT_UNKNOWN ? "--dialect" : "default");
    } else {
        dialect = dialect_get(dialect_from_string(backend));
        if (!dialect) {
            char bmsg[256];
            snprintf(bmsg, sizeof bmsg,
                     "model %s has unsupported backend '%s' (expected %s)",
                     pos ? pos : "?", backend, dialect_expected_names());
            VLOG(1, "cmd_check: %s", bmsg);
            free(base);
            free(mid);
            free(backend);
            free(api_key);
            return emit_runner_error(EXIT_INVALID, bmsg);
        }
        VLOG(2, "cmd_check: dialect=%s (model backend '%s')",
             dialect->name, backend);
    }

    /* The two token-free calls. When a key resolved above, the
     * Authorization header is sent so a key-protected server (whose
     * /v1/models answers 401 without the key) is checkable; with no key
     * anywhere this is keyless, exactly as before. No chat call is ever
     * made, and nothing is written to the DB in either case. */
    backend_preflight_t pf;
    int prc = backend_preflight(base, mid, dialect, api_key, timeout_sec, &pf);

    /* The prc -> (verdict, exit code) mapping is the shared
     * preflight_verdict() in backend.c — the same classification the run
     * pre-claim auto preflight uses, so the two surfaces cannot drift. */
    int exit_code = EXIT_OK;
    const char *verdict = preflight_verdict(prc, &pf, &exit_code);

    VLOG(1, "cmd_check: %s (base_url=%s, model=%s)",
         prc == PREFLIGHT_OK ? "ok" : verdict, base, mid);
    if (prc == PREFLIGHT_OK)
        VLOG(1, "cmd_check: max_context=%ld (source: %s)",
             pf.max_context,
             pf.max_context_source[0] ? pf.max_context_source : "none");
    emit_check_verdict(prc == PREFLIGHT_OK, mid, base, pf.max_context,
                       verdict);

    free(base);
    free(mid);
    free(backend);
    free(api_key);
    return exit_code;
}
