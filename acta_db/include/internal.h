#ifndef ACTA_DB_INTERNAL_H
#define ACTA_DB_INTERNAL_H

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

/* ── Unpack the opaque db_t ────────────────────────────────────────── */

struct db_t {
    sqlite3 *handle;
    /*
     * Owned by the db layer; updated on every SQL error.
     * acta_db_last_error() returns a const char * pointing here.
     * The pointer is valid only until the next SQL operation on this
     * handle overwrites the field.  Callers that need to retain the
     * message must copy it (e.g. strdup) before the next call.
     */
    char    *last_error;
    int     in_transaction; /* 0 = idle, 1 = BEGIN issued, not yet closed */
};

/* ── String helpers ────────────────────────────────────────────────── */

/*
 * Heap-copy a string.
 *
 *   s == NULL     → returns NULL, *err untouched
 *                   (the "no value" case; not an error).
 *   malloc fails  → returns NULL, *err = ACTA_DB_ERR_ALLOC.
 *   success       → returns a heap copy; caller must free.
 *
 * err may be NULL (the code is then discarded).
 */
/*
 * Record an error message in db->last_error (the string returned by
 * acta_db_last_error()).  The message is a sqlite3_malloc'd copy owned
 * by the db layer — freed by the next db_exec_capture() / close — so
 * the caller passes any NUL-terminated string and keeps no pointer.
 *
 * Used by entity mutator failure paths that decide an error in C code
 * without a failing SQL statement (e.g. the FK pre-check in
 * acta_db_execution_create), so the CLI error message can surface the
 * detail instead of "(no detail)" (KI-7).
 */
static inline void db_set_error(db_t *db, const char *msg) {
    if (!db || !msg) return;
    sqlite3_free(db->last_error);
    size_t len = strlen(msg) + 1;
    char *copy = sqlite3_malloc(len);
    if (copy) {
        memcpy(copy, msg, len);
        db->last_error = copy;
    }
}

static inline char *db_strdup(const char *s, int *err) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (!copy) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }
    memcpy(copy, s, len);
    return copy;
}

/* ── Column readers ────────────────────────────────────────────────── */

/*
 * Read a text column into a heap-allocated copy.
 *
 *   SQL NULL column  → returns NULL, *err untouched
 *   malloc failure   → returns NULL, *err = ACTA_DB_ERR_ALLOC
 *   success          → returns a heap copy
 *
 * err may be NULL.
 *
 * Caller contract (every getter / lister):
 *
 *   Read all columns into the struct, passing the same &alloc_err
 *   to each db_col_text call.  After the last column, bail if any
 *   allocation failed:
 *
 *       int alloc_err = ACTA_DB_OK;
 *       c->type      = db_col_text(stmt, 1, &alloc_err);
 *       c->content   = db_col_text(stmt, 2, &alloc_err);
 *       …
 *       if (alloc_err) {
 *           acta_db_context_free(c);   // frees the string fields set so far
 *           free(c);
 *           sqlite3_finalize(stmt);
 *           if (err) *err = alloc_err;
 *           return NULL;
 *       }
 */
static inline char *db_col_text(sqlite3_stmt *stmt, int col, int *err) {
    const unsigned char *val = sqlite3_column_text(stmt, col);
    return val ? db_strdup((const char *)val, err) : NULL;
}

/*
 * Read an integer column.
 *
 * No allocation, no failure mode beyond SQLite's own.
 * The int64 → int cast truncates on out-of-range values
 * (implementation-defined in C, but SQLite rowids and small
 * counters never reach INT_MAX in this codebase).
 */
static inline int db_col_int(sqlite3_stmt *stmt, int col) {
    return (int)sqlite3_column_int64(stmt, col);
}

/*
 * Read an integer column, mapping SQL NULL → 0.
 * Used for optional FK columns (parent_id, folder_id,
 * context_id, skill_revision_id, model_revision_id)
 * where 0 means "unset / no parent".
 */
static inline int db_col_int_or_zero(sqlite3_stmt *stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return 0;
    return db_col_int(stmt, col);
}

/* ── Free helper ───────────────────────────────────────────────────── */

/*
 * Free a string pointer and NULL it out.
 * Safe on NULL (free(NULL) is a no-op).
 * NULL-ing protects against double-free if the struct is
 * partially built and then freed via the entity's _free function.
 */
#define DB_FREE_STR(field) do { free(field); (field) = NULL; } while (0)


static inline int db_clamp_limit(int limit)
{
    if (limit <= 0 || limit > ACTA_DB_MAX_PAGE)
        return ACTA_DB_MAX_PAGE;
    return limit;
}

#endif /* ACTA_DB_INTERNAL_H */
