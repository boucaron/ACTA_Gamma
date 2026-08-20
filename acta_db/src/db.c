#include "internal.h"
#include "db.h"
#include <sqlite3.h>

/* ------------------------------------------------------------------ */
/*  Error string                                                       */
/* ------------------------------------------------------------------ */

const char *acta_db_strerror(int code)
{
    switch (code) {
    case ACTA_DB_OK:            return "success";
    case ACTA_DB_ERR_NOT_FOUND: return "not found";
    case ACTA_DB_ERR_SQL:       return "sql error";
    case ACTA_DB_ERR_ALLOC:     return "allocation failure";
    case ACTA_DB_ERR_INVALID:   return "invalid argument";
    default:                    return "unknown error";
    }
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

db_t *acta_db_open(const char *path, int *err) {
    if (!path) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3 *handle;
    if (sqlite3_open(path, &handle) != SQLITE_OK) {
        sqlite3_close(handle);
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    /* Enable WAL and foreign keys */
    sqlite3_exec(handle, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(handle, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    db_t *db = malloc(sizeof(db_t));
    if (!db) {
        sqlite3_close(handle);
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }
    db->handle          = handle;
    db->last_error      = NULL;
    db->in_transaction  = 0;
    if (err) *err = ACTA_DB_OK;
    return db;
}

void acta_db_close(db_t *db) {
    if (!db) return;

    /* Implicit rollback if the caller forgot to commit/rollback. */
    if (db->in_transaction) {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
        db->in_transaction = 0;
    }

    free(db->last_error);
    sqlite3_close(db->handle);
    free(db);
}

/* ------------------------------------------------------------------ */
/*  Exec / last error                                                  */
/* ------------------------------------------------------------------ */

int acta_db_exec(db_t *db, const char *sql) {
    if (!db || !sql) return ACTA_DB_ERR_INVALID;

    free(db->last_error);
    db->last_error = NULL;

    char *err = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        db->last_error = err;   /* take ownership of sqlite-allocated string */
        return ACTA_DB_ERR_SQL;
    }
    return ACTA_DB_OK;
}

const char *acta_db_last_error(db_t *db) {
    if (!db) return NULL;
    return db->last_error;
}

/* ------------------------------------------------------------------ */
/*  Callback-style transaction                                         */
/* ------------------------------------------------------------------ */

int acta_db_transaction(db_t *db, int (*fn)(db_t *, void *), void *user_data) {
    if (!db || !fn) return ACTA_DB_ERR_INVALID;
    if (db->in_transaction) return ACTA_DB_ERR_INVALID;  /* no nesting */

    db->in_transaction = 1;

    if (sqlite3_exec(db->handle, "BEGIN;", NULL, NULL, NULL) != SQLITE_OK) {
        db->in_transaction = 0;
        return ACTA_DB_ERR_SQL;
    }

    int result = fn(db, user_data);

    if (result == ACTA_DB_OK) {
        sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
    } else {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
    }

    db->in_transaction = 0;
    return result;
}

/* ------------------------------------------------------------------ */
/*  Application-controlled (long-running) transaction                  */
/* ------------------------------------------------------------------ */

int acta_db_begin(db_t *db) {
    if (!db) return ACTA_DB_ERR_INVALID;
    if (db->in_transaction) return ACTA_DB_ERR_INVALID;  /* already in txn */

    if (sqlite3_exec(db->handle, "BEGIN;", NULL, NULL, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    db->in_transaction = 1;
    return ACTA_DB_OK;
}

int acta_db_commit(db_t *db) {
    if (!db) return ACTA_DB_ERR_INVALID;
    if (!db->in_transaction) return ACTA_DB_ERR_INVALID;  /* nothing to commit */

    db->in_transaction = 0;

    if (sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    return ACTA_DB_OK;
}

int acta_db_rollback(db_t *db) {
    if (!db) return ACTA_DB_ERR_INVALID;
    if (!db->in_transaction) return ACTA_DB_ERR_INVALID;  /* nothing to roll back */

    db->in_transaction = 0;

    if (sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    return ACTA_DB_OK;
}
