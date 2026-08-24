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
/*  Internal: run a one-shot SQL statement, capturing the error string.
 *
 *  Returns SQLITE_OK or a SQLite error code.
 *  On failure, *last_error is set to a sqlite3_malloc'd string that
 *  the caller owns (free with sqlite3_free).
 * ------------------------------------------------------------------ */
static int db_exec_capture(db_t *db, const char *sql) {
    /* Free any previous error string before running a new statement. */
    sqlite3_free(db->last_error);
    db->last_error = NULL;

    int rc = sqlite3_exec(db->handle, sql, NULL, NULL,
                          (char **)&db->last_error);
    return rc;
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
        if (handle) sqlite3_close(handle);
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    /*
     * PRAGMAs are best-effort.  WAL is not supported on :memory: or
     * some network filesystems; the DB still opens without it.
     * We do not treat a PRAGMA failure as fatal here; the caller
     * can query PRAGMA journal_mode if it needs to verify.
     */
    sqlite3_exec(handle, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(handle, "PRAGMA foreign_keys=ON;",  NULL, NULL, NULL);

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

int acta_db_close(db_t *db)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Best-effort rollback if the caller forgot to commit/rollback. */
    if (db->in_transaction) {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
        db->in_transaction = 0;
    }

    sqlite3_free(db->last_error);
    db->last_error = NULL;

    /*
     * sqlite3_close (NOT close_v2):
     *   • SQLITE_BUSY  – a prepared statement is still open; the handle
     *     is *not* released, so we must NOT free db.  The caller finalizes
     *     the statement(s) and retries.
     *   • SQLITE_OK    – clean close; safe to free.
     *   • other        – I/O error, etc.; handle *is* released by SQLite,
     *     so we can still free our C-level struct.
     */
    int rc = sqlite3_close(db->handle);

    if (rc == SQLITE_BUSY) {
        /* Outstanding prepared statement(s) – handle still valid.
         * Do NOT free db; caller can finalize stmts then retry. */
        return ACTA_DB_ERR_SQL;
    }

    free(db);   /* SQLITE_OK or hard error: struct is no longer needed */
    return (rc == SQLITE_OK) ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}



/* ------------------------------------------------------------------ */
/*  Exec / last error                                                  */
/* ------------------------------------------------------------------ */

int acta_db_exec(db_t *db, const char *sql) {
    if (!db || !sql) return ACTA_DB_ERR_INVALID;

    int rc = db_exec_capture(db, sql);
    return (rc == SQLITE_OK) ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
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
    if (db->in_transaction) return ACTA_DB_ERR_INVALID;

    if (db_exec_capture(db, "BEGIN") != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    db->in_transaction = 1;

    int result = fn(db, user_data);

    /*
     * The callback (or code it calls) may have already committed or
     * rolled back via acta_db_commit / acta_db_rollback.  In that
     * case in_transaction is 0 and no further action is needed.
     */
    if (db->in_transaction) {
        db->in_transaction = 0;   /* clear before SQL so retry is possible */

        const char *sql = (result == ACTA_DB_OK) ? "COMMIT" : "ROLLBACK";
        int rc = db_exec_capture(db, sql);
        if (rc != SQLITE_OK)
            result = ACTA_DB_ERR_SQL;
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  Application-controlled (long-running) transaction                  */
/* ------------------------------------------------------------------ */

int acta_db_begin(db_t *db) {
    if (!db) return ACTA_DB_ERR_INVALID;
    if (db->in_transaction) return ACTA_DB_ERR_INVALID;

    if (db_exec_capture(db, "BEGIN") != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    db->in_transaction = 1;
    return ACTA_DB_OK;
}

int acta_db_commit(db_t *db) {
    if (!db) return ACTA_DB_ERR_INVALID;
    if (!db->in_transaction) return ACTA_DB_ERR_INVALID;

    int rc = db_exec_capture(db, "COMMIT");
    if (rc != SQLITE_OK)
        return ACTA_DB_ERR_SQL;   /* flag stays 1 → caller can retry/rollback */

    db->in_transaction = 0;
    return ACTA_DB_OK;
}

int acta_db_rollback(db_t *db) {
    if (!db) return ACTA_DB_ERR_INVALID;
    if (!db->in_transaction) return ACTA_DB_ERR_INVALID;

    int rc = db_exec_capture(db, "ROLLBACK");
    if (rc != SQLITE_OK)
        return ACTA_DB_ERR_SQL;   /* flag stays 1 → caller can retry */

    db->in_transaction = 0;
    return ACTA_DB_OK;
}
