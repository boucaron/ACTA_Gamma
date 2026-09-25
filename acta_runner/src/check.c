/*
 * acta_runner — "check" action (token-free backend health check)
 *
 * Per docs/plans/runner-health-check.md: verifies the backend and a
 * specific model WITHOUT any chat completion — zero tokens, zero
 * inference, zero execution rows, no DB writes of any kind. It is the
 * token-free pre-flight for a `run --pending` batch and the first
 * triage step after a `failed` backend call.
 *
 *   acta_runner check <model-record-id>                  # DB mode
 *   acta_runner check --base-url <url> --model-identifier <id>   # standalone
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
 * Result contract (one JSON line on stdout, scriptable):
 *   success: {"ok":true,"model":"<id>","max_context":<n>,"base_url":"<url>"}
 *   failure: {"ok":false,"model":"<id>","base_url":"<url>",
 *             "verdict":"model still loading" | "server unreachable"
 *                       | "model not served" | "catalog unreachable"}
 *
 * Exit codes (the runner's existing code space, no new numbers):
 *   0  — all checks passed
 *   1  — unknown <model-record-id> (DB mode only)
 *   4  — invalid arguments (flag conflicts, bad --timeout, malformed
 *        config file when it is the timeout source)
 *   12 — EXIT_HTTP: /health non-200, model not in catalog, HTTP-level
 *        failure
 *   13 — EXIT_TIMEOUT: a check call hit --timeout
 *
 * No POST /v1/chat/completions — ever. No execution_log rows, no
 * execution state change, no DB writes of any kind. No streaming, no
 * retry loop (consistent with the product decisions).
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
        "\n"
        "stdout: {\"ok\":true,...,\"max_context\":N} on success, or\n"
        "{\"ok\":false,...,\"verdict\":\"<reason>\"} on failure.\n"
        "Exit: 0 ok | 1 model not found | 4 invalid | 12 http |\n"
        "13 timeout\n",
        out);
}

/* One-line JSON verdict on stdout (the scriptable result contract). */
static void emit_verdict(int ok, const char *model, const char *base_url,
                         long max_context, const char *verdict)
{
    if (ok) {
        printf("{\"ok\":true,\"model\":");
        json_str(stdout, model ? model : "");
        printf(",\"max_context\":%ld,\"base_url\":", max_context);
        json_str(stdout, base_url ? base_url : "");
        printf("}\n");
    } else {
        printf("{\"ok\":false,\"model\":");
        json_str(stdout, model ? model : "");
        printf(",\"base_url\":");
        json_str(stdout, base_url ? base_url : "");
        printf(",\"verdict\":");
        json_str(stdout, verdict ? verdict : "");
        printf("}\n");
    }
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

    /* Timeout resolution: --timeout flag -> config file "timeout" ->
     * built-in default (600 s), the existing resolution from
     * docs/runner_contract.md decision 5. The config file is only read
     * when the flag is absent (the flag wins outright, so the file
     * cannot change the result). A readable-but-malformed file is a
     * hard error, fail-closed, like everywhere else. */
    int timeout_sec;
    if (timeout > 0) {
        timeout_sec = timeout;
    } else {
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
        timeout_sec = acta_conf_resolve_timeout(&conf, 0);
        acta_conf_free(&conf);
    }

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

    char *base = NULL, *mid = NULL; /* heap copies, freed before every return */
    {
        const char *base_src, *mid_src;
        if (standalone) {
            base_src = cmd_args_flag(ga, "base-url", 1);
            mid_src  = cmd_args_flag(ga, "model-identifier", 1);
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
            model_t *m = acta_db_model_get_live(db, id, &err);
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
                snprintf(msg, sizeof msg,
                         "model %d has no base_url or model_identifier", id);
                VLOG(1, "cmd_check: %s", msg);
                return emit_runner_error(EXIT_INVALID, msg);
            }
            base_src = m->base_url;
            mid_src  = m->model_identifier;
            acta_db_model_free(m);
        }
        base = strdup(base_src);
        mid  = strdup(mid_src);
        if (!base || !mid) {
            free(base);
            free(mid);
            return EXIT_ALLOC;
        }
    }

    /* The two token-free calls, keyless by design (the check probes the
     * server surface before any model is registered; no chat call is
     * ever made). */
    backend_preflight_t pf;
    int prc = backend_preflight(base, mid, NULL, timeout_sec, &pf);

    const char *verdict = NULL;
    int exit_code = EXIT_OK;
    switch (prc) {
    case PREFLIGHT_OK:
        break;
    case PREFLIGHT_CANCELED:
        /* Defensive dead path for the CLI: the cooperative cancel flag
         * is only ever set by the GUI. */
        verdict = "server unreachable";
        exit_code = EXIT_HTTP;
        break;
    case PREFLIGHT_HEALTH_TIMEOUT:
        verdict = "server unreachable";
        exit_code = EXIT_TIMEOUT;
        break;
    case PREFLIGHT_HEALTH_TRANSPORT:
        verdict = "server unreachable";
        exit_code = EXIT_HTTP;
        break;
    case PREFLIGHT_HEALTH_NOT_200:
        verdict = (pf.http_status == 503)
            ? "model still loading" : "server unreachable";
        exit_code = EXIT_HTTP;
        break;
    case PREFLIGHT_MODELS_TIMEOUT:
        verdict = "catalog unreachable";
        exit_code = EXIT_TIMEOUT;
        break;
    case PREFLIGHT_MODELS_TRANSPORT:
        verdict = "catalog unreachable";
        exit_code = EXIT_HTTP;
        break;
    case PREFLIGHT_MODELS_NOT_200:
    case PREFLIGHT_MODELS_UNPARSEABLE:
        verdict = "catalog unreachable";
        exit_code = EXIT_HTTP;
        break;
    case PREFLIGHT_MODEL_NOT_SERVED:
        verdict = "model not served";
        exit_code = EXIT_HTTP;
        break;
    default:
        verdict = "server unreachable";
        exit_code = EXIT_HTTP;
        break;
    }

    VLOG(1, "cmd_check: %s (base_url=%s, model=%s)",
         prc == PREFLIGHT_OK ? "ok" : verdict, base, mid);
    emit_verdict(prc == PREFLIGHT_OK, mid, base, pf.max_context, verdict);

    free(base);
    free(mid);
    return exit_code;
}
