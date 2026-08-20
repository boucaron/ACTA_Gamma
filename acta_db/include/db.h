#ifndef ACTA_DB_DB_H
#define ACTA_DB_DB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* --- Error codes (db.h) --- */
#define ACTA_DB_OK             0
#define ACTA_DB_ERR_NOT_FOUND (-1)
#define ACTA_DB_ERR_SQL       (-2)
#define ACTA_DB_ERR_ALLOC     (-3)
#define ACTA_DB_ERR_INVALID   (-4)

/*
 * ACTA DB – uniform calling conventions
 *
 * Getters:   T *foo_get(db, key, int *err);
 *            err nullable; NULL return + *err==OK means not-found.
 *
 * Listers:   T **foo_list_*(db, …, int offset, int limit, int *out_count, int *err);
 *            offset 0-based row offset; limit ≤ 0 means no cap (return all).
 *            Both out-params nullable; NULL return + *err==OK means empty.
 *
 * Mutators:  int foo_mutate(db, …);   returns ACTA_DB_OK or negative code.
 *
 * Free:      void foo_free(T *);                  (single, NULL-safe)
 *            void foo_list_free(T **items, int count);  (array, NULL-safe)
 */



/* Return a short human-readable string for an error code.
 * Returns "unknown error" for values outside the defined range. */
const char *acta_db_strerror(int code);


typedef struct db_t db_t;

/* Open (or create) a database at the given path.
 * Returns NULL on failure; if err is non-NULL it receives the error code. */
db_t *acta_db_open(const char *path, int *err);

/* Close the database and free the handle.
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_SQL if the close failed
 * (e.g. outstanding prepared statements still hold the handle). */
int acta_db_close(db_t *db);


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
