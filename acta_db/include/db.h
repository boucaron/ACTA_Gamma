#ifndef ACTA_DB_DB_H
#define ACTA_DB_DB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif




/* --- Error codes (db.h) --- */
#define ACTA_DB_OK             0
/* ACTA_DB_ERR_NOT_FOUND: returned only by mutators when the target
 * row does not exist (e.g. move_to_folder, rename, restore).
 * Getters and listers do NOT return this code; they use
 * ACTA_DB_OK + NULL to signal "not found / empty". */
#define ACTA_DB_ERR_NOT_FOUND (-1)
#define ACTA_DB_ERR_SQL       (-2)
#define ACTA_DB_ERR_ALLOC     (-3)
#define ACTA_DB_ERR_INVALID   (-4)
#define ACTA_DB_ERR_INVALID_DB  (-5) // Not an SQLite DB
#define ACTA_DB_ERR_DUPLICATE  (-6)
#define ACTA_DB_ERR_FK  (-7)

/* --- Open modes (acta_db_open third argument) ---
 * ACTA_DB_OPEN_EXISTING: verify the file is a non-empty SQLite DB;
 *                        fails with ACTA_DB_ERR_INVALID_DB otherwise.
 * ACTA_DB_OPEN_CREATE:   open the file, creating it if it does not
 *                        exist (the caller applies the schema). */
#define ACTA_DB_OPEN_EXISTING 0
#define ACTA_DB_OPEN_CREATE   1


/**
 * Hard upper bound on rows a single lister call may return.
 *
 * If the caller passes limit <= 0 ("no cap") the implementation
 * clamps to ACTA_DB_MAX_PAGE.  Passing a value above this ceiling
 * is also clamped.  To retrieve the remaining rows the caller
 * pages forward with successive offset values.
 *
 * Chosen so that even on a table with wide rows (e.g. execution_log
 * with a multi-KB message column) one page stays well under a few MB.
 */
#define ACTA_DB_MAX_PAGE  10000

/*
 * ACTA DB – uniform calling conventions
 *
 * Getters:   T *foo_get(db, key, int *err);
 *            err nullable; NULL return + *err==OK means not-found.
 *
 * Listers:   T **foo_list_*(db, …, int offset, int limit, int *out_count, int *err);
 *            offset 0-based row offset;
 *            limit  max rows for this call.
 *                   ≤ 0 or > ACTA_DB_MAX_PAGE → clamped to ACTA_DB_MAX_PAGE.
 *                   To fetch all rows, page with successive offset values.
 *
 * Common pagination contract (all listers):
 *   offset - zero-based row offset (skip this many rows).
 *            Must be >= 0; a negative value yields ACTA_DB_ERR_INVALID.
 *   limit  - maximum number of rows to return.
 *            <= 0 or > ACTA_DB_MAX_PAGE -> clamped to ACTA_DB_MAX_PAGE (10000).
 *            A lister NEVER returns more than ACTA_DB_MAX_PAGE rows in one
 *            call, regardless of what the caller passes; to fetch a result
 *            set larger than that, page with successive offset values.
 *   Return value:
 *     heap-allocated array of T*  ->  success (a valid empty set may
 *     still be a NULL array - check *out_count / *err)
 *     NULL                        ->  real failure (*err < 0)
 *   If out_count is non-NULL it receives the number of items returned
 *   (0 for an empty page, which is success, not an error).
 *   If err is non-NULL it is set to ACTA_DB_OK on success or a
 *   negative ACTA_DB_ERR_* code on failure.
 *   Either out_count or err (or both) may be NULL.
 *
 * Counters:  int foo_count(db, ..., int *err);
 *            returns >= 0 on success, -1 on failure.
 *            err nullable; same codes as mutators.
 *
 * Mutators:  int foo_mutate(db, …);   returns ACTA_DB_OK or negative code.
 *
 * Free:      void foo_free(T *);                  (single, NULL-safe)
 *            void foo_list_free(T **items, int count);  (array, NULL-safe)
 */


/* db.h — public, near the other #defines */

typedef struct db_t db_t;

/* Return a short human-readable string for an error code.
 * Returns "unknown error" for values outside the defined range. */
const char *acta_db_strerror(int code);

/* Open a database at the given path.
 * creationMode must be one of:
 *   ACTA_DB_OPEN_EXISTING – check that the file is an existing, non-empty
 *                           SQLite DB (empty/non-SQLite file →
 *                           ACTA_DB_ERR_INVALID_DB)
 *   ACTA_DB_OPEN_CREATE   – open the file, creating it if it does not
 *                           exist (the caller applies the schema)
 * Returns NULL on failure; if err is non-NULL it receives the error code. */
db_t *acta_db_open(const char *path, int *err, int creationMode);

/* Close the database and free the handle.
 *
 * Behaviour:
 *   - Best-effort ROLLBACK if a transaction is still open.
 *   - Attempts sqlite3_close (strict close).
 *
 * Return values:
 *   ACTA_DB_OK      – clean close; db was freed.  Pointer is now invalid.
 *   ACTA_DB_ERR_SQL – one of:
 *       • SQLITE_BUSY: outstanding prepared statement(s) still reference
 *         the handle.  db is NOT freed; db->handle is still valid.
 *         The caller must finalise every sqlite3_stmt* it created,
 *         then call acta_db_close again.
 *       • Other SQLite error (I/O, etc.): the handle *is* released by
 *         SQLite internally, so db is freed.  Pointer is now invalid.
 *
 * In the SQLITE_BUSY retry loop the caller is responsible for locating
 * and finalising its own statements.  If it cannot do so (e.g. the
 * stmt pointers were lost), use acta_db_force_close instead.
 *
 * NULL-safe: a NULL db is treated as ACTA_DB_ERR_INVALID. */
int acta_db_close(db_t *db);

/* Force-close the database: always releases resources, never returns
 * an error other than ACTA_DB_ERR_INVALID for a NULL argument.
 *
 * Uses sqlite3_close_v2, which auto-finalises any open prepared
 * statements before releasing the handle.  db is unconditionally
 * freed (except when db == NULL).
 *
 * Prefer acta_db_close in normal code.  Reach for force_close only
 * when:
 *   - the caller has lost track of its sqlite3_stmt* handles and
 *     cannot finalise them individually, or
 *   - you are tearing down / shutting down and need a guaranteed
 *     resource release (atexit, signal handler, error unwind).
 *
 * Note: auto-finalised statements may leave partially-written rows
 * in an incomplete state if a transaction was in progress.  The
 * implicit ROLLBACK is attempted, but no guarantee is made if the
 * underlying I/O is already failing.
 *
 * Return value: ACTA_DB_OK on success (including the NULL case,
 * which returns ACTA_DB_ERR_INVALID). */
int acta_db_force_close(db_t *db);



/* Execute a SQL statement (or script). Returns ACTA_DB_OK on success,
 * a negative error code on failure. Use for running DDL / migrations. */
int acta_db_exec(db_t *db, const char *sql);

/* Returns the last error message for this connection. */
const char *acta_db_last_error(db_t *db);

/* Run a transaction: begins, runs the callback, commits (or rolls back).
 * The callback receives the db handle and user_data.
 * Returns ACTA_DB_OK on success, a negative error code if the callback
 * returned a non-OK code or a SQL error occurred. */
int acta_db_transaction(db_t *db, int (*fn)(db_t *, void *), void *user_data);

/*
 * Application-controlled (long-running) transactions.
 *
 * Use begin / commit / rollback when the transaction spans multiple
 * independent operations that are driven by application logic rather
 * than a single callback (e.g. a multi-file import where each file
 * triggers its own set of INSERTs).
 *
 * Rules:
 *   - begin must be called before any commit/rollback.
 *   - commit and rollback are mutually exclusive for a given begin.
 *   - If neither is called before acta_db_close, the transaction is
 *     implicitly rolled back.
 *   - Nested begin is not supported; returns ACTA_DB_ERR_INVALID.
 */

/* Begin a transaction on this connection.
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if a transaction
 * is already in progress, or another negative code on failure. */
int acta_db_begin(db_t *db);

/* Commit the current transaction.
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if no transaction
 * is in progress, or another negative code on failure. */
int acta_db_commit(db_t *db);

/* Roll back the current transaction.
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if no transaction
 * is in progress, or another negative code on failure. */
int acta_db_rollback(db_t *db);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_DB_H */
