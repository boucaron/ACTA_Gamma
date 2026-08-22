#include "internal.h"
#include "execution_log.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static char *col_text_dup(sqlite3_stmt *stmt, int col) {
    const unsigned char *z = sqlite3_column_text(stmt, col);
    return z ? strdup((const char *)z) : NULL;
}

static int acta_log_level_is_valid(const char *level) {
    if (!level) return 0;
    return strcmp(level, ACTA_LOG_LEVEL_DEBUG) == 0
        || strcmp(level, ACTA_LOG_LEVEL_INFO)  == 0
        || strcmp(level, ACTA_LOG_LEVEL_WARN)  == 0
        || strcmp(level, ACTA_LOG_LEVEL_ERROR) == 0;
}

static execution_log_t *row_to_execution_log(sqlite3_stmt *stmt) {
    execution_log_t *log = calloc(1, sizeof *log);
    if (!log) return NULL;

    log->id           = db_col_int(stmt, 0);
    log->execution_id = db_col_int(stmt, 1);
    log->level        = col_text_dup(stmt, 2);
    log->event        = col_text_dup(stmt, 3);
    log->message      = col_text_dup(stmt, 4);
    log->metadata     = col_text_dup(stmt, 5);
    log->created_at   = col_text_dup(stmt, 6);

    if (!log->level || !log->event) {
        acta_db_execution_log_free(log);
        return NULL;
    }
    return log;
}

/* Translate the public limit contract (<= 0 = no limit) into SQLite's
 * own sentinel (-1 = no limit, 0 = zero rows). */
static int sql_limit(int limit) {
    return limit <= 0 ? -1 : limit;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

int acta_db_execution_log_create(db_t *db, const execution_log_t *log, int *out_id) {
    if (!db || !log)                                  return ACTA_DB_ERR_INVALID;
    if (!log->level || !log->event)                   return ACTA_DB_ERR_INVALID;
    if (!acta_log_level_is_valid(log->level))         return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO execution_logs (execution_id, level, event, message, metadata)"
        " VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_int(stmt, 1, log->execution_id);
    sqlite3_bind_text(stmt, 2, log->level,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, log->event,  -1, SQLITE_TRANSIENT);

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
    if (!db) {
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
        log  = row_to_execution_log(stmt);
        code = log ? ACTA_DB_OK : ACTA_DB_ERR_ALLOC;
    } else if (rc == SQLITE_DONE) {
        code = ACTA_DB_OK;
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
                                                          int offset, int limit,
                                                          int *out_count,
                                                          int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    if (offset < 0) offset = 0;

    if (out_count) *out_count = 0;

    const char *sql =
        "SELECT id, execution_id, level, event, message, metadata, created_at"
        " FROM execution_logs WHERE execution_id = ?"
        " ORDER BY created_at, id"
        " LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, execution_id);
    sqlite3_bind_int(stmt, 2, sql_limit(limit));
    sqlite3_bind_int(stmt, 3, offset);

    int    count    = 0;
    size_t capacity = 0;
    execution_log_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if ((size_t)count >= capacity) {
            size_t new_cap = capacity ? capacity * 2 : 8;
            execution_log_t **tmp = realloc(items, new_cap * sizeof *tmp);
            if (!tmp) {
                acta_db_execution_log_list_free(items, count);
                sqlite3_finalize(stmt);
                if (err) *err = ACTA_DB_ERR_ALLOC;
                return NULL;
            }
            items    = tmp;
            capacity = new_cap;
        }

        execution_log_t *item = row_to_execution_log(stmt);
        if (!item) {
            acta_db_execution_log_list_free(items, count);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
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

int acta_db_execution_log_count(db_t *db, int execution_id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql =
        "SELECT COUNT(*) FROM execution_logs WHERE execution_id = ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    sqlite3_bind_int(stmt, 1, execution_id);
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
    for (int i = 0; i < count; i++) {
        acta_db_execution_log_free(items[i]);
    }
    free(items);
}
