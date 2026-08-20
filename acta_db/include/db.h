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
 * Listers:   T **foo_list_*(db, …, int *out_count, int *err);
 *            both out-params nullable; NULL return + *err==OK means empty.
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

/* Close the database and free the handle. */
void acta_db_close(db_t *db);

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

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_DB_H */
