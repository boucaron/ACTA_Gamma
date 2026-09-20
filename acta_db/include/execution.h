#ifndef ACTA_DB_EXECUTION_H
#define ACTA_DB_EXECUTION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Status constants ─────────────────────────────────────────────── */

#define ACTA_EXEC_STATUS_PENDING   "pending"
#define ACTA_EXEC_STATUS_RUNNING   "running"
#define ACTA_EXEC_STATUS_COMPLETED "completed"
#define ACTA_EXEC_STATUS_FAILED    "failed"
#define ACTA_EXEC_STATUS_CANCELLED "cancelled"

/* ── Row ──────────────────────────────────────────────────────────── */

typedef struct {
    int     id;
    int     context_id;
    int     skill_revision_id;
    int     model_revision_id;
    char   *raw_response;
    char   *result;
    char   *status;
    char   *error;
    char   *created_at;
    char   *started_at;
    char   *completed_at;
    int     parent_execution_id;   /* 0 = root execution */
    char   *deleted_at;            /* NULL if live */
} execution_t;

/* ── Query filter ─────────────────────────────────────────────────── */

/* All fields are optional.  A zero / NULL field means "do not filter
 * on that dimension".  Multiple non-default fields are AND-ed:
 * the row must match every field you set.  Every combination is
 * valid, including all-fields-unset (matches everything).
 *
 *   status             – exact match, e.g. "running"
 *   parent_execution_id– 0 = any (no parent filter);
 *                        > 0 → WHERE parent_execution_id = ?
 *   context_id         – 0 = any;        > 0 → WHERE context_id = ?
 *   skill_revision_id  – 0 = any;        > 0 → WHERE skill_revision_id = ?
 *   model_revision_id  – 0 = any;        > 0 → WHERE model_revision_id = ?
 *   include_deleted    – 0 (default) = live rows only
 *                        (deleted_at IS NULL);
 *                        non-zero = live + soft-deleted rows; callers
 *                        distinguish them via the deleted_at field.
 *
 * The struct is stack-allocated and read-only; no allocation or
 * ownership is involved. */
typedef struct {
    const char *status;           /* NULL = any            */
    int         parent_execution_id; /* 0 = any           */
    int         context_id;      /* 0 = any               */
    int         skill_revision_id;   /* 0 = any           */
    int         model_revision_id;   /* 0 = any           */
    int         include_deleted;     /* 0 = live only     */
} execution_query_t;

/* Convenience: a query that matches every LIVE row. */
#define ACTA_EXEC_QUERY_ANY \
    (execution_query_t){ .status = NULL, .parent_execution_id = 0, \
                          .context_id = 0, .skill_revision_id = 0, \
                          .model_revision_id = 0, .include_deleted = 0 }

/* ── State machine ────────────────────────────────────────────────── */

/*
 * ── Execution state machine ──────────────────────────────────────────
 *
 *   pending ──start()──▶ running ──complete()──▶ completed  (terminal)
 *                            │
 *                            └──fail()──────────▶ failed  ──reset()──▶ pending
 *
 *   pending, running ──cancel()──▶ cancelled  (terminal)
 *
 *  Transition rules (enforced in the C layer, not in SQL):
 *
 *    start()       requires  status == pending
 *    cancel()      requires  status ∈ {pending, running}
 *    complete()    requires  status == running
 *    fail()        requires  status == running
 *    reset()       requires  status == failed
 *
 *  completed and cancelled are immutable; no function will modify a
 *  row in those states. failed is re-entrant: reset() returns it to
 *  pending (retry) and clears the failed attempt's data (error,
 *  raw_response, started_at, completed_at); the execution_log audit
 *  trail is preserved across retries.
 *
 *  set_raw_response() has NO status restriction — it updates a data
 *  field, not a state transition, and may be called from any state.
 *
 *  The caller does NOT need to pre-check status.  Each transition
 *  function validates the current state internally and returns
 *  ACTA_DB_ERR_INVALID if the transition is illegal.
 *
 *  Concurrency: each transition UPDATE already carries its own status
 *  guard (  WHERE id = ? AND status = ?  ) and checks
 *  sqlite3_changes(), so the state change itself is atomic even if
 *  the handle is shared across threads.  The pre-SELECT exists only
 *  to return the friendlier ACTA_DB_ERR_INVALID for an illegal
 *  transition; in a race the losing caller simply sees 0 rows changed.
 *
 *  Soft-delete lifecycle: execution rows carry a deleted_at flag.
 *  acta_db_execution_delete flags a live row (forbidden from the
 *  running state and on already-deleted rows); acta_db_execution_restore
 *  unflags it (strict: a live or missing row is NOT_FOUND).
 *  A deleted execution is inert: reset() refuses it (restore first,
 *  then reset to retry). Query/count default to live-only
 *  (include_deleted = 0); include_deleted = 1 includes deleted rows.
 * ─────────────────────────────────────────────────────────────────────
 */

/* ── Mutators (return int status directly) ────────────────────────── */

/* Insert a new execution row.
 *
 * Required fields (validated up front, all must hold):
 *   context_id > 0, skill_revision_id > 0, model_revision_id > 0.
 * The executions table has no prompt column: the user message comes
 * from context.content; instruction text comes from the skill
 * revision's prompt_template.
 *
 * e->status is IGNORED: a new execution is always created "pending";
 * any other state is only reachable via the transition functions
 * (start / complete / fail / cancel).
 *
 * Returns ACTA_DB_OK on success; *out_id receives the new row id.
 * Returns ACTA_DB_ERR_INVALID if db or e is NULL or a required field
 * is missing; ACTA_DB_ERR_FK if context_id / skill_revision_id /
 * model_revision_id / parent_execution_id does not reference an
 * existing row; ACTA_DB_ERR_SQL on prepare/step failure. */
int  acta_db_execution_create(db_t *db, const execution_t *e, int *out_id);

/* Transition pending → running. Sets started_at. */
int  acta_db_execution_start(db_t *db, int id);

/* Transition pending|running → cancelled. */
int  acta_db_execution_cancel(db_t *db, int id);

/* Transition running → completed. Stores the result string. */
int  acta_db_execution_complete(db_t *db, int id, const char *result);

/* Transition running → failed. Stores the error string. */
int  acta_db_execution_fail(db_t *db, int id, const char *error);

/* Transition failed → pending (retry). Clears error, raw_response,
 * started_at and completed_at so the next run starts clean. The
 * execution_log history of the previous attempt is preserved. */
int  acta_db_execution_reset(db_t *db, int id);

/* Data update, NOT a state transition. May be called from any state. */
int  acta_db_execution_set_raw_response(db_t *db, int id, const char *raw);

/* ── Soft-delete lifecycle ────────────────────────────────────────── */

/* Soft-delete a live execution (deleted_at = datetime('now')).
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if the row is running.
 * ACTA_DB_ERR_NOT_FOUND if no row matches id or the row is already
 * deleted (same contract as acta_db_context_delete).
 * ACTA_DB_ERR_SQL on failure. */
int acta_db_execution_delete(db_t *db, int id);

/* Restore a soft-deleted execution (deleted_at = NULL); status is
 * untouched (a deleted failed row restores to failed and can then be
 * reset).  Strict: ACTA_DB_ERR_NOT_FOUND unless a DELETED row matches
 * id (a live row is an error, like context_restore).
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_SQL on failure. */
int acta_db_execution_restore(db_t *db, int id);

/* ── Getter ───────────────────────────────────────────────────────── */

/* Fetch a single execution by primary key.
 *
 * Returns a heap-allocated execution_t (free with
 * acta_db_execution_free), or NULL.
 *
 *   NULL + *err == ACTA_DB_OK       → row not found
 *   NULL + *err == ACTA_DB_ERR_*    → real failure
 *   non-NULL + *err == ACTA_DB_OK   → row found
 *
 * err may be NULL. */
execution_t *acta_db_execution_get(db_t *db, int id, int *err);

/* ── Unified lister ───────────────────────────────────────────────── */

/*
 * Return a page of executions matching `q`, ordered by id ASC.
 *
 * q – filter criteria; NULL is equivalent to &ACTA_EXEC_QUERY_ANY.
 *
 * Pagination:
 *   offset – number of rows to skip (0-based; 0 = first row).
 *            Must be >= 0; negative → ACTA_DB_ERR_INVALID.
 *   limit  – maximum number of rows to return.
 *            <= 0 or > ACTA_DB_MAX_PAGE → clamped to ACTA_DB_MAX_PAGE
 *            (see the common pagination contract in db.h).
 *
 * Returns:
 *   heap-allocated array of execution_t* (free with
 *   acta_db_execution_list_free), or NULL.
 *
 *   non-NULL + *out_count > 0  → rows returned
 *   NULL     + *err == OK      → no rows (normal, not an error)
 *   NULL     + *err <  0       → real failure
 *
 * Both out_count and err may be NULL.
 *
 * ── Examples ──────────────────────────────────────────────────────
 *
 *   All rows:
 *       query(db, &ACTA_EXEC_QUERY_ANY, off, lim, …)
 *
 *   Single filter:
 *       query(db, &(execution_query_t){ .status = "running" }, …)
 *       query(db, &(execution_query_t){ .parent_execution_id = 42 }, …)
 *       query(db, &(execution_query_t){ .context_id = 7 }, …)
 *       query(db, &(execution_query_t){ .skill_revision_id = 12 }, …)
 *       query(db, &(execution_query_t){ .model_revision_id = 5 }, …)
 *
 *   Combined (AND):
 *       // running executions belonging to context 7
 *       query(db, &(execution_query_t){
 *           .status = "running",
 *           .context_id = 7
 *       }, 0, 50, &n, &err);
 *
 * NOTE: this lister materializes the blob columns (raw_response,
 * result, error) for every row; a full page is clamped
 * to ACTA_DB_MAX_PAGE rows of full-blob content.  List views that
 * only need ids/status/timestamps should prefer
 * acta_db_execution_query_light.
 * ────────────────────────────────────────────────────────────────────
 */
execution_t **acta_db_execution_query(db_t *db,
                                      const execution_query_t *q,
                                      int offset, int limit,
                                      int *out_count, int *err);


/* Return a page of executions matching `q`, ordered by id ASC,
 * using the LIGHT projection: the blob columns (raw_response, result,
 * error) are NOT fetched, so those fields are NULL in every
 * returned row.  All other fields (ids, status, timestamps, parent,
 * deleted_at) are populated as usual.
 *
 * Same signature and same contract as acta_db_execution_query (q,
 * offset, limit, pagination, out_count, err, ordering, include_deleted
 * semantics).  Use acta_db_execution_get when you need the blobs of a
 * specific row.  Free with acta_db_execution_list_free. */
execution_t **acta_db_execution_query_light(db_t *db,
                                            const execution_query_t *q,
                                            int offset, int limit,
                                            int *out_count, int *err);


/* ── Count ────────────────────────────────────────────────────────── */

/* Return the total number of execution rows matching `q`
 * (ignoring pagination).  Useful for computing total_pages,
 * rendering "Page X of Y", or deciding whether a lister is exhausted
 * without fetching the next page.
 *
 * q – filter criteria; NULL is equivalent to &ACTA_EXEC_QUERY_ANY.
 *
 * Returns:
 *   >= 0  on success (the row count; 0 is valid)
 *   -1    on failure (*err set to a negative ACTA_DB_ERR_* code)
 *
 * err may be NULL. */
int acta_db_execution_count(db_t *db,
                            const execution_query_t *q,
                            int *err);


/* ── Free ─────────────────────────────────────────────────────────── */

void acta_db_execution_free(execution_t *e);
void acta_db_execution_list_free(execution_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_EXECUTION_H */
