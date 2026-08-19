#ifndef ACTA_DB_DB_H
#define ACTA_DB_DB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct db_t db_t;

/* Open (or create) a database at the given path.
 * Returns NULL on failure. */
db_t *acta_db_open(const char *path);

/* Close the database and free the handle. */
void acta_db_close(db_t *db);

/* Execute a SQL statement (or script). Returns 0 on success, <0 on error.
 * Use for running DDL / migrations. */
int acta_db_exec(db_t *db, const char *sql);

/* Returns the last error message for this connection. */
const char *acta_db_last_error(db_t *db);

/* Run a transaction: begins, runs the callback, commits (or rolls back).
 * The callback receives the db handle and user_data.
 * Returns 0 on success, <0 if the callback returned <0 or a SQL error occurred. */
int acta_db_transaction(db_t *db, int (*fn)(db_t *, void *), void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_DB_H */
