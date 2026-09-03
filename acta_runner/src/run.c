/*
 * acta_runner — "run" action
 *
 * Phase-1 scaffold: argument handling and the execution-claim path are
 * real; the LLM call path (fetch skill/model revisions, prompt
 * resolution, HTTP call, set_raw_response, output_schema validation,
 * complete/fail, execution_log phase rows) is phase 2.
 */

#include "runner.h"
#include "runner_util.h"
#include "acta_db.h"

#include <stdio.h>
#include <string.h>

static void run_usage(FILE *out)
{
    fputs(
        "Usage: acta_runner run <execution-id> [flags]\n"
        "       acta_runner run --pending [flags]\n"
        "\n"
        "Flags:\n"
        "  --pending       Run pending executions instead of one id\n"
        "  --max <n>       Max executions to run with --pending (0 = no limit)\n"
        "  --timeout <sec> Backend timeout in seconds (default 300)\n"
        "  --api-key <key> API key override (default: $OPENAI_API_KEY)\n",
        out);
}

int cmd_run(cmd_args_t *ga, const global_opts_t *gopts, db_t *db)
{
    int pending = cmd_args_has_flag(ga, "pending");
    int max = 0;
    int timeout = 300;
    const char *api_key = NULL;
    char msg[192];

    const char *v = cmd_args_flag(ga, "max", 1);
    if (v) {
        if (!parse_nonneg_int(v, &max)) {
            VLOG(1, "cmd_run: ERROR --max must be a non-negative integer, got '%s'", v);
            emit_error("--max must be a non-negative integer");
            run_usage(stderr);
            return EXIT_INVALID;
        }
    }

    v = cmd_args_flag(ga, "timeout", 1);
    if (v) {
        if (!parse_nonneg_int(v, &timeout) || timeout == 0) {
            VLOG(1, "cmd_run: ERROR --timeout must be a positive integer, got '%s'", v);
            emit_error("--timeout must be a positive integer");
            run_usage(stderr);
            return EXIT_INVALID;
        }
    }

    api_key = cmd_args_flag(ga, "api_key", 1);
    if (!api_key)
        api_key = getenv("OPENAI_API_KEY");

    const char *id_str = cmd_args_next_positional(ga);

    if (pending && id_str) {
        VLOG(1, "cmd_run: ERROR --pending conflicts with an execution-id positional");
        emit_error("--pending conflicts with an execution-id positional");
        run_usage(stderr);
        return EXIT_INVALID;
    }
    if (!pending && !id_str) {
        VLOG(1, "cmd_run: ERROR missing <execution-id> (or use --pending)");
        emit_error("missing <execution-id>: pass an id or use --pending");
        run_usage(stderr);
        return EXIT_INVALID;
    }

    if (pending) {
        /* Run a batch of pending executions (up to --max; 0 = no limit).
         * limit 0 is clamped to ACTA_DB_MAX_PAGE by the lister. */
        execution_query_t q = ACTA_EXEC_QUERY_ANY;
        q.status = ACTA_EXEC_STATUS_PENDING;

        int n = 0;
        int err = ACTA_DB_OK;
        execution_t **rows =
            acta_db_execution_query(db, &q, 0, max, &n, &err);
        if (!rows)
            return finish_op_error(db, err, "execution_query");

        if (n == 0) {
            VLOG(1, "cmd_run: no pending executions");
            return EXIT_OK;
        }

        VLOG(1, "cmd_run: running %d pending execution(s)", n);
        int worst = EXIT_OK;
        for (int i = 0; i < n; i++) {
            int rc = run_execution(db, rows[i]->id, timeout, api_key);
            if (rc != EXIT_OK && rc > worst)
                worst = rc;
        }
        acta_db_execution_list_free(rows, n);
        return worst;
    }

    int id = 0;
    if (!parse_positive_id(id_str, &id)) {
        snprintf(msg, sizeof msg,
                 "invalid <execution-id>: '%s' must be a positive integer",
                 id_str);
        VLOG(1, "cmd_run: %s", msg);
        emit_error(msg);
        run_usage(stderr);
        return EXIT_INVALID;
    }

    return run_execution(db, id, timeout, api_key);
}

/*
 * Single-execution pipeline.
 *
 * Phase 1 (this scaffold): fetch the execution, reject non-pending
 * rows, and stop WITHOUT touching state — the execution stays
 * pending. Phase 2 fills in the real pipeline:
 *
 *   start() → log execution_started
 *   → context/skill revision/model revision fetches
 *     → log context_loaded / prompt_resolved
 *   → build request (system = prompt_template,
 *                   user = context content + prompt,
 *                   model = model_identifier, base_url, configuration)
 *     → log llm_request
 *   → POST {base_url}/chat/completions (timeout, api key)
 *     → set_raw_response(raw) → log llm_response
 *   → output_schema validation → log validation_*
 *   → complete(result) / fail(error)
 */
int run_execution(db_t *db, int exec_id, int timeout_sec,
                  const char *api_key)
{
    (void)timeout_sec;
    (void)api_key;

    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, exec_id, &err);
    if (err != ACTA_DB_OK) {
        acta_db_execution_free(e);
        return finish_op_error(db, err, "execution_get");
    }
    if (!e)
        return emit_not_found("execution");

    if (e->status && strcmp(e->status, ACTA_EXEC_STATUS_PENDING) != 0) {
        VLOG(1, "run_execution: execution %d is not pending (status: %s)",
             exec_id, e->status ? e->status : "(null)");
        acta_db_execution_free(e);
        emit_error("execution is not pending; only pending executions "
                   "can be run");
        return EXIT_INVALID;
    }

    VLOG(1, "run_execution: execution %d pending; backend path not "
             "implemented yet", exec_id);
    acta_db_execution_free(e);

    emit_error("LLM call path not implemented yet (phase-1 scaffold); "
               "execution left pending");
    return EXIT_INVALID;
}
