#ifndef ACTA_DB_EXECUTION_H
#define ACTA_DB_EXECUTION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACTA_EXEC_STATUS_PENDING   "pending"
#define ACTA_EXEC_STATUS_RUNNING   "running"
#define ACTA_EXEC_STATUS_COMPLETED "completed"
#define ACTA_EXEC_STATUS_FAILED    "failed"
#define ACTA_EXEC_STATUS_CANCELLED "cancelled"

typedef struct {
    int     id;
    int     context_id;
    int     skill_revision_id;
    int     model_revision_id;
    char   *prompt;
    char   *raw_response;
    char   *result;
    char   *status;
    char   *error;
    char   *created_at;
    char   *started_at;
    char   *completed_at;
    int     parent_execution_id;
} execution_t;

/*
 * ── Execution state machine ──────────────────────────────────────────────────
 *
 *   pending ──start()──────────▶ running ──complete()──▶ completed   (terminal)
 *                     │               │
 *                     │          fail()
 *                     │               ▼
 *                     └──cancel()───▶ failed        (terminal)
 *                              ▼
 *                         cancelled                (terminal)
 *
 *  Transition rules (enforced in the C layer, not in SQL):
 *
 *    start()       requires  status == pending
 *    cancel()      requires  status ∈ {pending, running}
 *    complete()    requires  status == running
 *    fail()        requires  status == running
 *
 *  Terminal states (completed, failed, cancelled) are immutable;
 *  no function will modify a row in a terminal state.
 *
 *  set_raw_response() has NO status restriction — it updates a data
 *  field, not a state transition, and may be called from any non-terminal
 *  (or even terminal) state.
 *
 *  The caller does NOT need to pre-check status.  Each transition function
 *  validates the current state internally and returns ACTA_DB_ERR_INVALID
 *  if the transition is illegal.
 *
 *  ⚠  The SELECT-then-UPDATE pattern is not atomic.  This is safe under
 *  the project's single-threaded DB usage model.  If the handle is ever
 *  shared across threads, fold the status guard into the UPDATE itself
 *  (  WHERE id = ? AND status = ?  ) and rely on sqlite3_changes().
 * ─────────────────────────────────────────────────────────────────────────────
 */

/* --- action / mutation functions (return int status directly) --- */
int  acta_db_execution_create(db_t *db, const execution_t *e, int *out_id);
int  acta_db_execution_start(db_t *db, int id);
int  acta_db_execution_cancel(db_t*, int id);
int  acta_db_execution_complete(db_t *db, int id, const char *result);
int  acta_db_execution_fail(db_t *db, int id, const char *error);
int  acta_db_execution_set_raw_response(db_t *db, int id, const char *raw);

/* --- getters / listers (standardised err pattern) --- */
execution_t  *acta_db_execution_get(db_t *db, int id, int *err);
execution_t **acta_db_execution_list_all(db_t *db,
                                          int offset, int limit,
                                          int *out_count, int *err);
execution_t **acta_db_execution_list_by_status(db_t *db, const char *status,
                                                int offset, int limit,
                                                int *out_count, int *err);
execution_t **acta_db_execution_list_children(db_t *db, int parent_id,
                                               int offset, int limit,
                                               int *out_count, int *err);
execution_t **acta_db_execution_list_by_context(db_t *db, int context_id,
                                                int offset, int limit,
                                                int *out_count, int *err);
execution_t **acta_db_execution_list_by_skill_revision(db_t *db,
                                                        int skill_revision_id,
                                                        int offset, int limit,
                                                        int *out_count, int *err);
execution_t **acta_db_execution_list_by_model_revision(db_t *db,
                                                        int model_revision_id,
                                                        int offset, int limit,
                                                        int *out_count, int *err);

/* --- free --- */
void acta_db_execution_free(execution_t *e);
void acta_db_execution_list_free(execution_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif
