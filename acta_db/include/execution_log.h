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
 *
 * Validation: db and log must be non-NULL; log->level must be one of
 * "debug" | "info" | "warn" | "error" (ACTA_LOG_LEVEL_* constants,
 * case-sensitive); log->event must be non-NULL.  Any violation
 * → ACTA_DB_ERR_INVALID.
 *
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
 *   Return value / *err:
 *     non-NULL pointer + *err == ACTA_DB_OK  – row found (heap-allocated).
 *     NULL         + *err == ACTA_DB_OK       – row NOT found (no error).
 *     NULL         + *err == ACTA_DB_ERR_*    – genuine failure
 *                                               (bad handle, SQL, alloc, …).
 *
 *   The caller MUST inspect *err to distinguish "not found" from an
 *   actual error.  A NULL return alone is ambiguous.
 *
 * err may be NULL if the caller only needs the pointer (e.g. an
 * existence check via a NULL result), but then the not-found / error
 * distinction is lost.
 *
 * Free the result with acta_db_execution_log_free().
 */
execution_log_t *acta_db_execution_log_get(db_t *db,
                                           int id,
                                           int *err);


/*
 * Lister – paginated rows for a given execution, ordered by id.
 *
 *   level:
 *     Server-side filter on the log level column.
 *       - NULL or "" (empty string) → no filter (return all levels).
 *       - "debug" | "info" | "warn" | "error" → only that level.
 *       - any other non-empty value → *err = ACTA_DB_ERR_INVALID,
 *         returns NULL (server validates the level, it is not free-form).
 *
 *   Pagination:
 *     offset – number of rows to skip (0-based).
 *     limit  – maximum number of rows to return.
 *              <= 0 or > ACTA_DB_MAX_PAGE → clamped to ACTA_DB_MAX_PAGE
 *              (see the common pagination contract in db.h).
 *
 *   rows found   → returns a valid array of heap-allocated structs,
 *                  *out_count = number of rows, *err = ACTA_DB_OK.
 *   empty page   → returns NULL, *out_count = 0, *err = ACTA_DB_OK
 *                  (success — not an error).
 *   real failure → returns NULL, *err = negative ACTA_DB_ERR_*.
 *
 * Both out_count and err may be NULL (caller ignores them).
 */
execution_log_t **acta_db_execution_log_list_by_execution(db_t *db,
                                                          int execution_id,
                                                          const char *level,
                                                          int offset, int limit,
                                                          int *out_count, int *err);

/*
 * Count – total number of log rows for a given execution (ignoring pagination).
 *
 *   level:
 *     Same semantics as list_by_execution: NULL or "" → count all
 *     levels; unrecognized non-empty level → returns -1,
 *     *err = ACTA_DB_ERR_INVALID.
 *
 * Returns the row count (>= 0), or -1 on error (*err set).
 * err may be NULL.
 */
int acta_db_execution_log_count(db_t *db,
                                int execution_id,
                                const char *level,
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
