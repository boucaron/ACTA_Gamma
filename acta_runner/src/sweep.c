/*
 * acta_runner — "sweep" action (R4: stale-`running` cleanup)
 *
 * Per R4 and decision 6 in docs/runner_contract.md:
 * when a runner process dies, its execution row stays stuck in
 * `running` forever. `sweep` finds those rows and fails them.
 *
 *   acta_runner sweep --stale-seconds N
 *
 * Algorithm:
 *   1. Query executions with status = running.
 *   2. Per row, last runner activity = the MAX of the latest
 *      execution_log.created_at (the lister orders by created_at, id)
 *      and the execution's started_at (set atomically by start()),
 *      falling back to created_at if neither exists. A live runner can
 *      never have both stale (execution_started is logged immediately
 *      after start()), so the max is the conservative choice: a row
 *      claimed within the last N seconds is never swept.
 *   3. Timestamps are SQLite CURRENT_TIMESTAMP strings
 *      ("YYYY-MM-DD HH:MM:SS", UTC), so the lexicographic comparison
 *      against the threshold (now − N, formatted identically) IS the
 *      chronological one: last < threshold → stale.
 *   4. Stale row → acta_db_execution_fail("stale running: no runner
 *      activity for N s") + execution_failed log row (logged only when
 *      the fail succeeds — see 4a). If the row left `running` between
 *      the query and the fail() because a live runner finished, fail()
 *      returns ACTA_DB_ERR_INVALID and the sweep skips it rather than
 *      overwriting the outcome.
 *
 * Semantics: --stale-seconds must be a POSITIVE integer; 0 is rejected
 * (a zero-second sweep would fail every running row; `run` is the tool
 * for failing a specific execution).
 *
 * Exit codes: 0 when nothing is running or every row was swept; the
 * initial query failure exits per the standard DB error contract.
 */

#include "runner.h"
#include "runner_util.h"
#include "acta_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void sweep_usage(FILE *out)
{
    fputs(
        "Usage: acta_runner sweep --stale-seconds <n>\n"
        "\n"
        "Fails executions stuck in `running` whose last execution_log\n"
        "activity is older than <n> seconds (dead-runner cleanup).\n"
        "--stale-seconds must be a positive integer.\n",
        out);
}

/* Format a time_t as the UTC string SQLite CURRENT_TIMESTAMP produces
 * ("YYYY-MM-DD HH:MM:SS") so lexicographic comparison matches
 * chronological order. */
static void utc_stamp(time_t t, char *out, size_t outsz)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(out, outsz, "%Y-%m-%d %H:%M:%S", &tm);
}

/*
 * Latest runner-activity timestamp for the row: the MAX of
 * newest execution_log.created_at and started_at (both are
 * CURRENT_TIMESTAMP strings, so lexicographic max is the
 * chronological max), falling back to created_at if neither exists.
 * Returns 1 and fills `out` on success, 0 if no timestamp is usable.
 */
static int last_activity(db_t *db, const execution_t *e,
                         char *out, size_t outsz)
{
    int err = ACTA_DB_OK, n = 0;
    execution_log_t **rows = acta_db_execution_log_list_by_execution(
        db, e->id, NULL, 0, ACTA_DB_MAX_PAGE, &n, &err);
    const char *last_log = NULL;
    if (err == ACTA_DB_OK && rows) {
        /* lister order is (created_at, id) ASC → the newest is last */
        if (n > 0)
            last_log = rows[n - 1]->created_at;
        acta_db_execution_log_list_free(rows, n);
    } else if (rows) {
        acta_db_execution_log_list_free(rows, n);
    }
    const char *started = (e->started_at && e->started_at[0])
        ? e->started_at : NULL;
    const char *last;
    if (last_log && started)
        last = (strcmp(last_log, started) >= 0) ? last_log : started;
    else
        last = last_log ? last_log : started;
    if (!last && e->created_at && e->created_at[0])
        last = e->created_at;
    if (!last || !last[0])
        return 0;
    snprintf(out, outsz, "%s", last);
    return 1;
}

/* One execution_log row. Log failure is not fatal (audit aid, not
 * state) — warn and continue, mirroring log_phase in run.c. */
static void log_row(db_t *db, int exec_id, const char *level,
                    const char *event, const char *message)
{
    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = exec_id;
    log.level    = (char *)level;
    log.event    = (char *)event;
    log.message  = (char *)message;
    int rc = acta_db_execution_log_create(db, &log, NULL);
    if (rc != ACTA_DB_OK)
        VLOG(1, "sweep: execution_log create failed (%s); continuing",
             acta_db_strerror(rc));
}

int cmd_sweep(cmd_args_t *ga, const global_opts_t *gopts, db_t *db)
{
    (void)gopts;

    const char *v = cmd_args_flag(ga, "stale-seconds", 1);
    int stale_seconds = 0;
    if (!v || !parse_positive_id(v, &stale_seconds)) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "--stale-seconds requires a positive integer (got '%s')",
                 v ? v : "(absent)");
        VLOG(1, "cmd_sweep: %s", msg);
        emit_error(msg);
        sweep_usage(stderr);
        return EXIT_INVALID;
    }

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_RUNNING;
    int err = ACTA_DB_OK, n = 0;
    execution_t **rows =
        acta_db_execution_query(db, &q, 0, ACTA_DB_MAX_PAGE, &n, &err);
    /* rows is NULL for BOTH a genuine failure and an empty result
     * (n == 0 with err == ACTA_DB_OK); only the former is an error. */
    if (rows == NULL && (err != ACTA_DB_OK || n != 0))
        return finish_op_error(db, err, "execution_query");

    if (n == 0) {
        VLOG(1, "cmd_sweep: no running executions");
        return EXIT_OK;
    }

    char threshold[32];
    utc_stamp(time(NULL) - (time_t)stale_seconds, threshold, sizeof threshold);

    int swept = 0, fresh = 0, skipped = 0;
    for (int i = 0; i < n; i++) {
        execution_t *e = rows[i];

        char last[32];
        int have_ts = last_activity(db, e, last, sizeof last);
        /* no timestamp at all → definitely orphaned → stale */
        if (have_ts && strcmp(last, threshold) >= 0) {
            fresh++;
            VLOG(1, "cmd_sweep: execution %d fresh (last activity %s); kept",
                 e->id, last);
            continue;
        }

        char msg[192];
        snprintf(msg, sizeof msg,
                 "stale running: no runner activity for %d s",
                 stale_seconds);
        int rc = acta_db_execution_fail(db, e->id, msg);
        if (rc != ACTA_DB_OK) {
            /* The row left `running` between the query and the fail()
             * (a live runner completed/failed in the meantime). Do not
             * overwrite its outcome. */
            skipped++;
            VLOG(1, "cmd_sweep: execution %d left running in the meantime "
                    "(%s); skipped", e->id, acta_db_strerror(rc));
            continue;
        }
        log_row(db, e->id, ACTA_LOG_LEVEL_ERROR, "execution_failed", msg);
        swept++;
        VLOG(1, "cmd_sweep: execution %d swept to failed", e->id);
    }
    acta_db_execution_list_free(rows, n);

    VLOG(1, "cmd_sweep: swept %d, fresh %d, skipped %d", swept, fresh,
         skipped);
    return EXIT_OK;
}
