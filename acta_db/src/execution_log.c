#include "internal.h"
#include "execution_log.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static int acta_log_level_is_valid(const char *level) {
    if (!level) return 0;
    return strcmp(level, ACTA_LOG_LEVEL_DEBUG) == 0
        || strcmp(level, ACTA_LOG_LEVEL_INFO)  == 0
        || strcmp(level, ACTA_LOG_LEVEL_WARN)  == 0
        || strcmp(level, ACTA_LOG_LEVEL_ERROR) == 0;
}

/* NULL and "" both mean "no filter". */
static int level_active(const char *level) {
    return level && level[0] != '\0';
}

/*
 * Decode one result row into a heap-allocated execution_log_t.
 *
 * On failure the struct is freed and NULL is returned; *err receives:
 *   ACTA_DB_ERR_ALLOC – calloc or db_col_text malloc failure
 *   ACTA_DB_ERR_SQL   – level or event came back SQL NULL
 *                       (NOT NULL in the schema; data is corrupt)
 *
 * err may be NULL.
 */
static execution_log_t *row_to_execution_log(sqlite3_stmt *stmt, int *err) {
    execution_log_t *log = calloc(1, sizeof *log);
    if (!log) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    log->id           = db_col_int(stmt, 0);
    log->execution_id = db_col_int(stmt, 1);
    log->level        = db_col_text(stmt, 2, &alloc_err);
    log->event        = db_col_text(stmt, 3, &alloc_err);
    log->message      = db_col_text(stmt, 4, &alloc_err);
    log->metadata     = db_col_text(stmt, 5, &alloc_err);
    log->created_at   = db_col_text(stmt, 6, &alloc_err);

    if (alloc_err) {
        acta_db_execution_log_free(log);
        if (err) *err = alloc_err;
        return NULL;
    }

    if (!log->level || !log->event) {
        acta_db_execution_log_free(log);
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    return log;
}

/* ------------------------------------------------------------------ */
/*  SQL templates (static – no runtime formatting, no truncation risk)  */
/* ------------------------------------------------------------------ */

static const char SQL_LIST_BY_EXEC_WITH_LEVEL[] =
    "SELECT id, execution_id, level, event, message, metadata, created_at"
    " FROM execution_logs"
    " WHERE execution_id = ? AND level = ?"
    " ORDER BY created_at, id"
    " LIMIT ? OFFSET ?;";

static const char SQL_LIST_BY_EXEC[] =
    "SELECT id, execution_id, level, event, message, metadata, created_at"
    " FROM execution_logs"
    " WHERE execution_id = ?"
    " ORDER BY created_at, id"
    " LIMIT ? OFFSET ?;";

static const char SQL_COUNT_WITH_LEVEL[] =
    "SELECT COUNT(*) FROM execution_logs"
    " WHERE execution_id = ? AND level = ?;";

static const char SQL_COUNT[] =
    "SELECT COUNT(*) FROM execution_logs"
    " WHERE execution_id = ?;";

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

int acta_db_execution_log_create(db_t *db, const execution_log_t *log, int *out_id) {
    if (!db || !log)                        return ACTA_DB_ERR_INVALID;
    if (!log->level || !log->event)        return ACTA_DB_ERR_INVALID;
    if (!acta_log_level_is_valid(log->level)) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO execution_logs (execution_id, level, event, message, metadata)"
        " VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_int  (stmt, 1, log->execution_id);
    sqlite3_bind_text (stmt, 2, log->level,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, log->event,  -1, SQLITE_TRANSIENT);
    if (log->message)
        sqlite3_bind_text(stmt, 4, log->message, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 4);
    if (log->metadata)
        sqlite3_bind_text(stmt, 5, log->metadata, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 5);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;

    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

/* ------------------------------------------------------------------ */
/*  Getter                                                               */
/* ------------------------------------------------------------------ */

execution_log_t *acta_db_execution_log_get(db_t *db, int id, int *err) {
    if (!db || id <= 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, execution_id, level, event, message, metadata, created_at"
        " FROM execution_logs WHERE id = ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);

    execution_log_t *log = NULL;
    int code;

    if (rc == SQLITE_ROW) {
        int row_err = ACTA_DB_OK;
        log = row_to_execution_log(stmt, &row_err);
        code = log ? ACTA_DB_OK : row_err;
    } else if (rc == SQLITE_DONE) {
        code = ACTA_DB_OK;   /* not found */
    } else {
        code = ACTA_DB_ERR_SQL;
    }

    sqlite3_finalize(stmt);
    if (err) *err = code;
    return log;
}

/* ------------------------------------------------------------------ */
/*  Lister                                                               */
/* ------------------------------------------------------------------ */

execution_log_t **acta_db_execution_log_list_by_execution(db_t *db,
                                                          int execution_id,
                                                          const char *level,
                                                          int offset, int limit,
                                                          int *out_count,
                                                          int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (level_active(level) && !acta_log_level_is_valid(level)) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (out_count) *out_count = 0;

    int  has_level  = level_active(level);
    int  eff_limit  = db_clamp_limit(limit);   /* ≤ 0 or > MAX → MAX_PAGE */
    const char *sql = has_level ? SQL_LIST_BY_EXEC_WITH_LEVEL
                                : SQL_LIST_BY_EXEC;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    sqlite3_bind_int (stmt, param++, execution_id);
    if (has_level)
        sqlite3_bind_text(stmt, param++, level, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, param++, eff_limit);
    sqlite3_bind_int (stmt, param++, offset);

    int    count    = 0;
    size_t capacity = 0;
    execution_log_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if ((size_t)count >= capacity) {
            size_t new_cap = capacity ? capacity * 2 : 64;
            execution_log_t **tmp = realloc(items, new_cap * sizeof *tmp);
            if (!tmp) {
                acta_db_execution_log_list_free(items, (int)count);
                sqlite3_finalize(stmt);
                if (err) *err = ACTA_DB_ERR_ALLOC;
                return NULL;
            }
            items    = tmp;
            capacity = new_cap;
        }

        int row_err = ACTA_DB_OK;
        execution_log_t *item = row_to_execution_log(stmt, &row_err);
        if (!item) {
            acta_db_execution_log_list_free(items, (int)count);
            sqlite3_finalize(stmt);
            if (err) *err = row_err;
            return NULL;
        }
        items[count++] = item;
    }

    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err)       *err       = ACTA_DB_OK;

    if (count == 0) {
        free(items);
        return NULL;
    }
    return items;
}

/* ------------------------------------------------------------------ */
/*  Count                                                                */
/* ------------------------------------------------------------------ */

int acta_db_execution_log_count(db_t *db,
                                int execution_id,
                                const char *level,
                                int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }
    if (level_active(level) && !acta_log_level_is_valid(level)) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    int has_level  = level_active(level);
    const char *sql = has_level ? SQL_COUNT_WITH_LEVEL : SQL_COUNT;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int param = 1;
    sqlite3_bind_int (stmt, param++, execution_id);
    if (has_level)
        sqlite3_bind_text(stmt, param++, level, -1, SQLITE_TRANSIENT);

    int rc = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rc = (int)sqlite3_column_int64(stmt, 0);
        if (err) *err = ACTA_DB_OK;
    } else {
        if (err) *err = ACTA_DB_ERR_SQL;
    }

    sqlite3_finalize(stmt);
    return rc;
}

/* ------------------------------------------------------------------ */
/*  Free helpers                                                        */
/* ------------------------------------------------------------------ */

void acta_db_execution_log_free(execution_log_t *log) {
    if (!log) return;
    free(log->level);
    free(log->event);
    free(log->message);
    free(log->metadata);
    free(log->created_at);
    free(log);
}

void acta_db_execution_log_list_free(execution_log_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_execution_log_free(items[i]);
    free(items);
}
