#include "internal.h"
#include "db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    case ACTA_DB_ERR_INVALID_DB: return "invalid sqlitedb file";
    case ACTA_DB_ERR_DUPLICATE: return "duplicate name";
    case ACTA_DB_ERR_FK:        return "foreign key violation";
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

/* ── tiny callback: count rows (returns 0 to keep iterating) ────── */
static int vlog_count_rows(void *ctx, int ncol, char **vals, char **names)
{
    (void)ncol; (void)vals; (void)names;
    *(int *)ctx += 1;
    return 0;
}

/* ── tiny callback: capture the first column of the first row ───── */
/* *ctx receives a sqlite3_mprintf'd copy (free with sqlite3_free);   */
/* NULL if there is no value. Used to read the effective journal_mode. */
static int pragma_capture_first(void *ctx, int ncol, char **vals, char **names)
{
    (void)names;
    if (ncol > 0 && vals && vals[0])
        *(char **)ctx = sqlite3_mprintf("%s", vals[0]);
    return 0;
}

/* ── Open a database ───────────────────────────────────────────────
 *
 *  ACTA_DB_OPEN_EXISTING: verify the file is a non-empty SQLite DB
 *  (at least one user table); an empty or non-SQLite file yields
 *  ACTA_DB_ERR_INVALID_DB.  This guards against sqlite3_open()
 *  silently creating an empty 0-byte database.
 *  ACTA_DB_OPEN_CREATE: open the file, creating it if it does not
 *  exist (schema is applied by the caller).
 */
db_t *acta_db_open(const char *path, int *err, int creationMode)
{
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
     * PRAGMAs are best-effort: check-and-report, never fail the open.
     * WAL is not supported on :memory: databases or some filesystems;
     * the set-pragma then returns SQLITE_OK while the effective mode is
     * not "wal", so the effective mode is queried and, when degraded,
     * an informational note is stored in last_error (see below).
     * PRAGMA foreign_keys=ON cannot fail at this point (no active
     * transaction); its rc is captured anyway as future-proofing.
     * A non-NULL last_error after a successful open is NOT an error:
     * the return code is ACTA_DB_OK and the connection is fully usable;
     * any later acta_db_exec* call clears it like any other error string.
     */
    int rc_wal = sqlite3_exec(handle, "PRAGMA journal_mode=WAL;",
                              NULL, NULL, NULL);
    int rc_fk  = sqlite3_exec(handle, "PRAGMA foreign_keys=ON;",
                              NULL, NULL, NULL);

    /* The mode query is the authoritative check: the set-pragma can
     * return SQLITE_OK while the mode silently stays "memory"/"delete".
     * (rc_wal is only secondary evidence; a non-OK rc implies
     * !wal_active via the mode query either way.) */
    char *mode = NULL;
    int mode_rc = sqlite3_exec(handle, "PRAGMA journal_mode;",
                               pragma_capture_first, &mode, NULL);
    int wal_active = (mode_rc == SQLITE_OK && mode &&
                      strcmp(mode, "wal") == 0);
    /* mode_str aliases mode (or the static "(unknown)"); mode is freed
     * later, after the informational note has been built. */
    const char *mode_str = (mode_rc == SQLITE_OK && mode) ? mode : "(unknown)";
    (void)rc_wal;

    /*
     * Enable extended result codes so that constraint violations
     * report SQLITE_CONSTRAINT_UNIQUE / SQLITE_CONSTRAINT_FOREIGNKEY
     * directly from sqlite3_step() instead of the generic
     * SQLITE_CONSTRAINT.  This mirrors PRAGMA foreign_keys=ON above:
     * a connection-level setting, applied once at open.
     */
    sqlite3_extended_result_codes(handle, 1);

    /*
     * Guard against sqlite3_open() auto-creating an empty file.
     * A real database must contain at least one user table; a
     * freshly-created 0-byte file has none (only sqlite_% internals).
     */
    if ( creationMode == 0) {
        int ntables = 0;
        char *errmsg = NULL;
        int rc = sqlite3_exec(handle,
            "SELECT name FROM sqlite_master "
            " WHERE type='table' AND name NOT LIKE 'sqlite_%';",
            vlog_count_rows, &ntables, &errmsg);

        if (rc != SQLITE_OK) {
            sqlite3_free(errmsg);
            sqlite3_close(handle);
            if (err) *err = ACTA_DB_ERR_SQL;
            return NULL;
        }

        if (ntables == 0) {
            sqlite3_close(handle);
            if (err) *err = ACTA_DB_ERR_INVALID_DB;
            return NULL;
        }
    }

    db_t *db = malloc(sizeof(db_t));
    if (!db) {
        sqlite3_close(handle);
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }
    db->handle          = handle;
    db->last_error      = NULL;
    db->in_transaction  = 0;

    /* Keep the opened path for acta_db_main_path() (the db backup
     * target validation needs it).  db_strdup only fails on OOM. */
    db->path = db_strdup(path, NULL);
    if (!db->path) {
        free(db);
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    /* Informational pragma note (not an error). If both degradation
     * notes ever fired, the foreign-key note wins: it is the rarer and
     * more severe condition (FK enforcement off ⇒ ACTA_DB_ERR_FK
     * mapping unreliable), so it is set last. */
    char *note = NULL;
    if (!wal_active) {
        note = sqlite3_mprintf(
            "PRAGMA journal_mode=WAL did not take effect (effective "
            "journal_mode: %s; WAL is unsupported on :memory: databases "
            "and some filesystems). Informational note, not an error: "
            "the connection is fully usable.", mode_str);
        sqlite3_free(mode);   /* mode_str is no longer needed */
    }
    if (rc_fk != SQLITE_OK) {
        sqlite3_free(note);
        if (wal_active)
            sqlite3_free(mode);   /* not yet freed (wal-branch skips it) */
        note = sqlite3_mprintf(
            "PRAGMA foreign_keys=ON failed at open (rc=%d); FK enforcement "
            "may be off on this connection, making ACTA_DB_ERR_FK mapping "
            "unreliable. Informational note, not an error.", rc_fk);
    }
    db->last_error = note;   /* NULL when both pragmas took effect */

    if (err) *err = ACTA_DB_OK;
    return db;
}


int acta_db_close(db_t *db)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    if (db->in_transaction) {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
        db->in_transaction = 0;
    }

    int rc = sqlite3_close(db->handle);

    if (rc == SQLITE_BUSY) {
        /* Handle still valid – caller must finalise stmts and retry.
         * Preserve error string for diagnostics. */
        return ACTA_DB_ERR_SQL;
    }

    /* Clean or hard-error: handle is gone, free our struct. */
    sqlite3_free(db->last_error);
    sqlite3_free(db->path);
    free(db);
    return (rc == SQLITE_OK) ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_force_close(db_t *db)   /* never fails to release */
{
    if (!db) return ACTA_DB_ERR_INVALID;
    sqlite3_close_v2(db->handle);    /* auto-finalises open stmts */
    sqlite3_free(db->last_error);
    sqlite3_free(db->path);
    free(db);
    return ACTA_DB_OK;
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

const char *acta_db_errmsg(db_t *db) {
    if (!db || !db->handle) return NULL;
    return sqlite3_errmsg(db->handle);
}

const char *acta_db_main_path(const db_t *db) {
    if (!db) return NULL;
    return db->path;
}

/* Return the names of every user table in the main database
 * (sqlite_master rows with type='table', excluding the internal
 * sqlite_ bookkeeping tables). Returns a heap-allocated, NULL-terminated
 * array of strdup'd name strings; *out_count (if non-NULL) receives the
 * number of entries. On failure returns NULL and sets *err (if non-NULL)
 * to ACTA_DB_ERR_SQL or ACTA_DB_ERR_ALLOC; on success sets ACTA_DB_OK.
 * Free the result with acta_db_user_tables_free.
 *
 * Used by acta_cli `db init` for its PRAGMA table_info-style idempotency
 * check: distinguish a fresh (no user tables) file, an already-
 * schema'd file (exactly the canonical tables), and a partially-
 * applied or foreign file. */
char **acta_db_user_tables(db_t *db, int *out_count, int *err)
{
    if (out_count) *out_count = 0;
    if (!db || !db->handle) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT name FROM sqlite_master "
        "WHERE type='table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_set_error(db, "failed to prepare the user-table query");
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int capacity = 16;
    int count = 0;
    char **names = NULL;
    int failed = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        if (!name) { failed = 1; break; }
        if (count == capacity) {
            capacity *= 2;
            char **grow = realloc(names, (size_t)capacity * sizeof *names);
            if (!grow) { failed = 1; break; }
            names = grow;
        }
        names[count] = strdup(name);
        if (!names[count]) { failed = 1; break; }
        count++;
    }
    sqlite3_finalize(stmt);

    if (failed) {
        for (int i = 0; i < count; i++) free(names[i]);
        free(names);
        if (!db->last_error)
            db_set_error(db, "memory allocation failed");
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    if (names) names[count] = NULL;   /* NULL terminator */
    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return names;
}

void acta_db_user_tables_free(char **names, int count)
{
    (void)count;
    if (!names) return;
    for (int i = 0; names[i]; i++)
        free(names[i]);
    free(names);
}

int acta_db_backup(db_t *db, const char *target, long long *bytes_out,
                   int *err)
{
    if (bytes_out) *bytes_out = 0;

    if (!db || db->handle == NULL || !target || *target == '\0') {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return ACTA_DB_ERR_INVALID;
    }

    /* 1. The snapshot itself, through the SQLite backup C API — the
     *    target is passed to the API, never interpolated into SQL
     *    text.  sqlite3_backup copies the logical contents of the
     *    open connection (WAL state included), so the result is a
     *    consistent single-file snapshot even while the DB is in WAL
     *    mode with other consumers open — the same guarantee as
     *    VACUUM INTO, without closing anything first.  The target is
     *    opened with CREATE; existence was already rejected by the
     *    caller (no silent overwrite).
     *    On any failure the partial file is removed: nothing is
     *    left behind. */
    sqlite3 *dest = NULL;
    int rc = sqlite3_open_v2(target, &dest,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);
    if (rc != SQLITE_OK) {
        db_set_error(db, "cannot open backup target for writing");
        if (err) *err = ACTA_DB_ERR_SQL;
        return ACTA_DB_ERR_SQL;
    }

    int ok = 0;
    sqlite3_backup *bk =
        sqlite3_backup_init(dest, "main", db->handle, "main");
    if (bk != NULL) {
        /* step(-1) runs the copy to completion. */
        rc = sqlite3_backup_step(bk, -1);
        ok = (rc == SQLITE_DONE);
        if (!ok)
            db_set_error(db, "backup step failed");
        sqlite3_backup_finish(bk);
    } else {
        db_set_error(db, "backup init failed");
    }
    sqlite3_close_v2(dest);

    if (!ok) {
        remove(target);
        if (err) *err = ACTA_DB_ERR_SQL;
        return ACTA_DB_ERR_SQL;
    }

    /* 2. The size of the written file (for the caller's payload). */
    struct stat st;
    /* A valid backup is at least one page; a zero-size file is a
     * failed copy.  `== 0` is used (not `< 0`) because st_size is
     * unsigned on some platforms. */
    if (stat(target, &st) != 0 || st.st_size == 0) {
        remove(target);
        db_set_error(db, "cannot stat the backup file");
        if (err) *err = ACTA_DB_ERR_SQL;
        return ACTA_DB_ERR_SQL;
    }
    *bytes_out = (long long)st.st_size;

    /* 3. Built-in verification: reopen the backup on its own
     *    connection and run PRAGMA quick_check.  A backup that does
     *    not check is deleted and reported as failed — not kept. */
    sqlite3 *chk = NULL;
    rc = sqlite3_open(target, &chk);
    if (rc != SQLITE_OK) {
        remove(target);
        db_set_error(db, "cannot reopen the backup for quick_check");
        if (err) *err = ACTA_DB_ERR_SQL;
        return ACTA_DB_ERR_SQL;
    }
    char *chk_err = NULL;
    rc = sqlite3_exec(chk, "PRAGMA quick_check;", NULL, NULL, &chk_err);
    sqlite3_free(chk_err);
    sqlite3_close(chk);
    if (rc != SQLITE_OK) {
        remove(target);
        db_set_error(db, "PRAGMA quick_check failed on the backup");
        if (err) *err = ACTA_DB_ERR_SQL;
        return ACTA_DB_ERR_SQL;
    }

    if (err) *err = ACTA_DB_OK;
    return ACTA_DB_OK;
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
