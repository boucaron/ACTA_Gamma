/*
 * deathmark.c — death marker for orphaned `running` rows
 * (docs/plans/runner-ops-hardening.md, item 3).
 *
 * On a clean process exit (SIGINT / SIGTERM / atexit) while an
 * execution is claimed in `running`, the marker fails the row with the
 * diagnostic "runner process exited during execution" and appends the
 * matching execution_failed phase row, so the row is not left orphaned
 * in `running`.  Hard kill / power loss still orphans the row; sweep
 * remains the recovery path for that case.
 *
 * The marker itself (deathmark_mark) is plain, testable C; the signal
 * wiring (deathmark_install) is thin.
 */

#include "deathmark.h"
#include "runner.h"   /* VLOG */
#include "acta_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* The claimed execution: at most one at a time (the runner claims
 * per-execution and `run --pending` loops one row at a time). */
static db_t *dm_db = NULL;
static int dm_exec_id = 0;

void deathmark_claim(db_t *db, int exec_id)
{
    dm_db = db;
    dm_exec_id = exec_id;
}

void deathmark_release(void)
{
    dm_db = NULL;
    dm_exec_id = 0;
}

int deathmark_mark(db_t *db, int exec_id)
{
    if (db == NULL || exec_id <= 0)
        return 0;

    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, exec_id, &err);
    if (err != ACTA_DB_OK || !e) {
        acta_db_execution_free(e);
        return 0;   /* unknown id: nothing to mark */
    }
    int still_running = e->status &&
        strcmp(e->status, ACTA_EXEC_STATUS_RUNNING) == 0;
    acta_db_execution_free(e);
    if (!still_running)
        return 0;   /* normal completion / failure / cancel already
                       transitioned the row: nothing to mark */

    static const char *diag = "runner process exited during execution";
    execution_log_t log;
    memset(&log, 0, sizeof log);
    log.execution_id = exec_id;
    log.level    = (char *)ACTA_LOG_LEVEL_ERROR;
    log.event    = (char *)"execution_failed";
    log.message  = (char *)diag;
    log.metadata = NULL;
    int rc = acta_db_execution_log_create(db, &log, NULL);
    if (rc != ACTA_DB_OK)
        VLOG(1, "deathmark: execution_log create failed (%s); continuing",
             acta_db_strerror(rc));
    rc = acta_db_execution_fail(db, exec_id, diag);
    if (rc != ACTA_DB_OK)
        VLOG(1, "deathmark: execution_fail returned %s; row may stay running",
             acta_db_strerror(rc));
    return 1;
}

void deathmark_exit_path(void)
{
    deathmark_mark(dm_db, dm_exec_id);
}

static void deathmark_signal(int sig)
{
    /* Terminate via exit() so the atexit hook (deathmark_exit_path)
     * runs the marker before the process dies. */
    exit(128 + sig);
}

void deathmark_install(void)
{
    if (atexit(deathmark_exit_path) != 0)
        return;
    signal(SIGINT, deathmark_signal);
    signal(SIGTERM, deathmark_signal);
}
