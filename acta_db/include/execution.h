#ifndef ACTA_DB_EXECUTION_H
#define ACTA_DB_EXECUTION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif


/* --- String constants (execution.h) --- */
#define ACTA_EXEC_STATUS_PENDING   "pending"
#define ACTA_EXEC_STATUS_RUNNING   "running"
#define ACTA_EXEC_STATUS_COMPLETED "completed"
#define ACTA_EXEC_STATUS_FAILED    "failed"


typedef struct {
    int     id;
    int     context_id;
    int     skill_revision_id;
    int     model_revision_id;
    char   *prompt;
    char   *raw_response;
    char   *result;
    char   *status;         /* "pending" | "running" | "completed" | "failed" */
    char   *error;
    char   *created_at;
    char   *started_at;
    char   *completed_at;
    int     parent_execution_id;  /* 0 = root */
} execution_t;

/*
 * Create a new execution row.
 * Returns ACTA_DB_OK on success; the new row id is written to *out_id.
 * Returns ACTA_DB_ERR_INVALID if any required pointer is NULL.
 * Returns ACTA_DB_ERR_SQL on a SQLite failure.
 */
int         acta_db_execution_create(db_t *db, const execution_t *e, int *out_id);

/*
 * Fetch a single execution by id.
 * Returns a heap-allocated execution_t (free with acta_db_execution_free),
 * or NULL if not found or on error.
 */
execution_t *acta_db_execution_get(db_t *db, int id);

/*
 * Transition an execution from "pending" to "running".
 * Returns ACTA_DB_OK on success.
 * Returns ACTA_DB_ERR_INVALID if db is NULL.
 * Returns ACTA_DB_ERR_SQL on a SQLite failure.
 * Returns ACTA_DB_ERR_NOT_FOUND if no row matched (wrong id or not "pending").
 */
int         acta_db_execution_start(db_t *db, int id);

/*
 * Transition an execution from "running" to "completed", storing the result.
 * Returns ACTA_DB_OK on success.
 * Returns ACTA_DB_ERR_INVALID if db is NULL.
 * Returns ACTA_DB_ERR_SQL on a SQLite failure.
 * Returns ACTA_DB_ERR_NOT_FOUND if no row matched (wrong id or not "running").
 */
int         acta_db_execution_complete(db_t *db, int id, const char *result);

/*
 * Transition an execution from "running" to "failed", storing the error.
 * Returns ACTA_DB_OK on success.
 * Returns ACTA_DB_ERR_INVALID if db is NULL.
 * Returns ACTA_DB_ERR_SQL on a SQLite failure.
 * Returns ACTA_DB_ERR_NOT_FOUND if no row matched (wrong id or not "running").
 */
int         acta_db_execution_fail(db_t *db, int id, const char *error);

/*
 * Update the raw_response column for an execution.
 * Returns ACTA_DB_OK on success.
 * Returns ACTA_DB_ERR_INVALID if db is NULL.
 * Returns ACTA_DB_ERR_SQL on a SQLite failure.
 * Returns ACTA_DB_ERR_NOT_FOUND if no row matched (wrong id).
 */
int         acta_db_execution_set_raw_response(db_t *db, int id, const char *raw);

/*
 * List all executions matching the given status string.
 * Returns a heap-allocated array of execution_t (free with
 * acta_db_execution_list_free), or NULL on error.
 * *out_count receives the number of rows returned.
 */
execution_t *acta_db_execution_list_by_status(db_t *db, const char *status, int *out_count);

/*
 * List all child executions of the given parent.
 * Returns a heap-allocated array of execution_t (free with
 * acta_db_execution_list_free), or NULL on error.
 * *out_count receives the number of rows returned.
 */
execution_t *acta_db_execution_list_children(db_t *db, int parent_id, int *out_count);

/*
 * List all executions belonging to the given context, newest first.
 * Returns a heap-allocated array of execution_t (free with
 * acta_db_execution_list_free), or NULL on error.
 * *out_count receives the number of rows returned.
 */
execution_t *acta_db_execution_list_by_context(db_t *db, int context_id, int *out_count);

/* Free a single execution_t obtained from acta_db_execution_get. */
void        acta_db_execution_free(execution_t *e);

/* Free an array returned by any acta_db_execution_list_* function. */
void        acta_db_execution_list_free(execution_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_EXECUTION_H */
