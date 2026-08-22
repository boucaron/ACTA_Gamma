#ifndef ACTA_DB_EXECUTION_LOG_H
#define ACTA_DB_EXECUTION_LOG_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- String constants --- */
#define ACTA_LOG_LEVEL_DEBUG "debug"
#define ACTA_LOG_LEVEL_INFO  "info"
#define ACTA_LOG_LEVEL_WARN  "warn"
#define ACTA_LOG_LEVEL_ERROR "error"

/* Convenience macro for callers */
#define ACTA_LOG_LEVEL_SET(lvl, name) (lvl) = ACTA_LOG_LEVEL_##name

typedef struct {
    int     id;
    int     execution_id;
    char   *level;         /* "debug" | "info" | "warn" | "error" */
    char   *event;
    char   *message;
    char   *metadata;
    char   *created_at;
} execution_log_t;

/*
 * Mutator.
 *   Return value is the error code:
 *     ACTA_DB_OK (0)        – row created; *out_id set (if non-NULL).
 *     negative ACTA_DB_ERR_* – failure; *out_id left untouched.
 * out_id may be NULL if the caller does not need the new id.
 */
int acta_db_execution_log_create(db_t *db,
                                 const execution_log_t *log,
                                 int *out_id);

/*
 * Getter – fetch a single row by its primary key.
 *
 *   Return value:
 *     NULL                  – failure or row not found; *err set to a
 *                             negative ACTA_DB_ERR_* code.
 *     heap-allocated pointer – row found; *err set to ACTA_DB_OK (0).
 *
 * err may be NULL if the caller only needs the pointer (e.g. an
 * existence check via a NULL result).
 *
 * Free the result with acta_db_execution_log_free().
 */
execution_log_t *acta_db_execution_log_get(db_t *db,
                                           int id,
                                           int *err);

/*
 * Lister – paginated rows for a given execution, ordered by id.
 *
 *   Pagination:
 *     offset – number of rows to skip (0-based).
 *     limit  – maximum number of rows to return.
 *              <= 0 means no limit (return all matching rows).
 *
 *   success / rows found → returns valid execution_log_t ** (array of
 *                          heap-allocated structs), *err = ACTA_DB_OK.
 *   no rows (not-found)  → returns NULL, *err = ACTA_DB_OK.
 *   real failure         → returns NULL, *err = negative ACTA_DB_ERR_*.
 *
 * Both out_count and err may be NULL (caller ignores them).
 */
execution_log_t **acta_db_execution_log_list_by_execution(db_t *db,
                                                          int execution_id,
                                                          int offset, int limit,
                                                          int *out_count, int *err);

/*
 * Count – total number of log rows for a given execution (ignoring pagination).
 *
 * Returns the row count (>= 0), or -1 on error (*err set).
 * err may be NULL.
 */
int acta_db_execution_log_count(db_t *db,
                                int execution_id,
                                int *err);

/* Free a single log entry (its string fields + the struct). Safe with NULL. */
void acta_db_execution_log_free(execution_log_t *log);

/* Free an array of log entries produced by the lister, plus the array itself.
 * Frees each element and the pointer array. Safe with NULL. */
void acta_db_execution_log_list_free(execution_log_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_EXECUTION_LOG_H */
