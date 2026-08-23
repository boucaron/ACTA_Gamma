#include "internal.h"
#include "execution.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

static execution_t *row_to_execution(sqlite3_stmt *stmt) {
    execution_t *e = calloc(1, sizeof(execution_t));
    if (!e) return NULL;
    e->id                  = db_col_int(stmt, 0);
    e->context_id          = db_col_int(stmt, 1);
    e->skill_revision_id   = db_col_int(stmt, 2);
    e->model_revision_id   = db_col_int(stmt, 3);
    e->prompt              = db_col_text(stmt, 4);
    e->raw_response        = db_col_text(stmt, 5);
    e->result              = db_col_text(stmt, 6);
    e->status              = db_col_text(stmt, 7);
    e->error               = db_col_text(stmt, 8);
    e->created_at          = db_col_text(stmt, 9);
    e->started_at          = db_col_text(stmt, 10);
    e->completed_at        = db_col_text(stmt, 11);
    e->parent_execution_id = db_col_int_or_zero(stmt, 12);
    return e;
}

#define EXEC_SELECT \
    "SELECT id, context_id, skill_revision_id, model_revision_id, prompt, " \
    "raw_response, result, status, error, created_at, started_at, " \
    "completed_at, parent_execution_id FROM executions"

/* ------------------------------------------------------------------ */
/*  Dynamic WHERE-clause builder (shared by query + count)            */
/* ------------------------------------------------------------------ */

/*
 * Builds "WHERE <conditions>" from the query struct.
 * Returns the number of bound parameters (0..5), or -1 on overflow.
 *
 * `sql` must have room for at least 384 bytes of WHERE clause
 * (5 filters × ~64 chars worst-case + padding).
 * `bind_index` is set to 1 (first param slot after the SELECT).
 */
static int exec_build_where(char *sql, size_t sql_sz,
                            const execution_query_t *q,
                            int *bind_index)
{
    if (!sql || !sql_sz) return -1;

    int pos = 0;
    int n   = 0;

    pos += snprintf(sql + pos, sql_sz - (size_t)pos, " WHERE");

    /* status */
    if (q->status) {
        pos += snprintf(sql + pos, sql_sz - (size_t)pos,
                        "%s status = ?", n++ ? " AND" : "");
    }

    /* parent_execution_id */
    if (q->parent_execution_id) {
        pos += snprintf(sql + pos, sql_sz - (size_t)pos,
                        "%s parent_execution_id = ?", n++ ? " AND" : "");
    }

    /* context_id */
    if (q->context_id) {
        pos += snprintf(sql + pos, sql_sz - (size_t)pos,
                        "%s context_id = ?", n++ ? " AND" : "");
    }

    /* skill_revision_id */
    if (q->skill_revision_id) {
        pos += snprintf(sql + pos, sql_sz - (size_t)pos,
                        "%s skill_revision_id = ?", n++ ? " AND" : "");
    }

    /* model_revision_id */
    if (q->model_revision_id) {
        pos += snprintf(sql + pos, sql_sz - (size_t)pos,
                        "%s model_revision_id = ?", n++ ? " AND" : "");
    }

    /* No filters → match all (no WHERE clause) */
    if (n == 0) {
        sql[0] = '\0';
    }

    if (bind_index) *bind_index = 1;
    return n;
}

/* Bind the filter parameters (in the same order the builder emitted them).
 * Returns the index of the next free bind slot. */
static int exec_bind_where(sqlite3_stmt *stmt,
                           const execution_query_t *q,
                           int start_idx)
{
    int i = start_idx;
    if (q->status)              sqlite3_bind_text(stmt, i++, q->status, -1, SQLITE_TRANSIENT);
    if (q->parent_execution_id) sqlite3_bind_int (stmt, i++, q->parent_execution_id);
    if (q->context_id)          sqlite3_bind_int (stmt, i++, q->context_id);
    if (q->skill_revision_id)   sqlite3_bind_int (stmt, i++, q->skill_revision_id);
    if (q->model_revision_id)   sqlite3_bind_int (stmt, i++, q->model_revision_id);
    return i;
}

/* ------------------------------------------------------------------ */
/*  Unified query (paginated lister)                                  */
/* ------------------------------------------------------------------ */

execution_t **acta_db_execution_query(db_t *db,
                                      const execution_query_t *q,
                                      int offset, int limit,
                                      int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (limit <= 0) limit = -1;  /* SQLite: LIMIT -1 = no upper bound */
    if (out_count) *out_count = 0;

    /* ── build SQL ─────────────────────────────────────────────── */
    char where_clause[384];

    if (q) {
        exec_build_where(where_clause, sizeof(where_clause), q, NULL);
    } else {
        where_clause[0] = '\0';
    }

    char sql[600];
    snprintf(sql, sizeof(sql),
             EXEC_SELECT "%s ORDER BY id ASC LIMIT ? OFFSET ?;",
             where_clause);

    /* ── prepare ───────────────────────────────────────────────── */
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    /* ── bind params ───────────────────────────────────────────── */
    int idx = 1;
    if (q) idx = exec_bind_where(stmt, q, idx);

    sqlite3_bind_int(stmt, idx++, limit);
    sqlite3_bind_int(stmt, idx++, offset);

    /* ── iterate rows ──────────────────────────────────────────── */
    int    count    = 0;
    size_t capacity = 0;
    execution_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if ((size_t)count >= capacity) {
            size_t new_cap = capacity ? capacity * 2 : 8;
            execution_t **tmp = realloc(items, new_cap * sizeof *tmp);
            if (!tmp) {
                acta_db_execution_list_free(items, count);
                sqlite3_finalize(stmt);
                if (err) *err = ACTA_DB_ERR_ALLOC;
                return NULL;
            }
            items    = tmp;
            capacity = new_cap;
        }

        execution_t *item = row_to_execution(stmt);
        if (!item) {
            acta_db_execution_list_free(items, count);
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
/*  Count                                                             */
/* ------------------------------------------------------------------ */

int acta_db_execution_count(db_t *db,
                            const execution_query_t *q,
                            int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    char where_clause[384];
    if (q) {
        exec_build_where(where_clause, sizeof(where_clause), q, NULL);
    } else {
        where_clause[0] = '\0';
    }

    char sql[500];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM executions%s;", where_clause);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int idx = 1;
    if (q) idx = exec_bind_where(stmt, q, idx);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = (int)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (err) *err = (result >= 0) ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
    return result;
}

/* ------------------------------------------------------------------ */
/*  Create                                                            */
/* ------------------------------------------------------------------ */

int acta_db_execution_create(db_t *db, const execution_t *e, int *out_id) {
    if (!db || !e || !e->status) return ACTA_DB_ERR_INVALID;
    const char *sql =
        "INSERT INTO executions (context_id, skill_revision_id, model_revision_id, prompt, status, parent_execution_id) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, e->context_id);
    sqlite3_bind_int(stmt, 2, e->skill_revision_id);
    sqlite3_bind_int(stmt, 3, e->model_revision_id);
    if (e->prompt) sqlite3_bind_text(stmt, 4, e->prompt, -1, SQLITE_TRANSIENT);
    else           sqlite3_bind_null(stmt, 4);
    sqlite3_bind_text(stmt, 5, e->status, -1, SQLITE_TRANSIENT);
    if (e->parent_execution_id == 0) sqlite3_bind_null(stmt, 6);
    else                              sqlite3_bind_int(stmt, 6, e->parent_execution_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_CONSTRAINT) return ACTA_DB_ERR_INVALID;
    if (rc != SQLITE_DONE)       return ACTA_DB_ERR_SQL;
    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

/* ------------------------------------------------------------------ */
/*  Get                                                               */
/* ------------------------------------------------------------------ */

execution_t *acta_db_execution_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql = EXEC_SELECT " WHERE id = ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    execution_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_execution(stmt);
        if (!result) {
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
    }
    sqlite3_finalize(stmt);

    if (err) *err = ACTA_DB_OK;
    return result;
}

/* ------------------------------------------------------------------ */
/*  Start / Cancel / Complete / Fail / Set raw                        */
/* ------------------------------------------------------------------ */

int acta_db_execution_start(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    execution_t *existing = acta_db_execution_get(db, id, NULL);
    if (!existing) return ACTA_DB_ERR_NOT_FOUND;

    if (strcmp(existing->status, ACTA_EXEC_STATUS_PENDING) != 0) {
        acta_db_execution_free(existing);
        return ACTA_DB_ERR_INVALID;
    }
    acta_db_execution_free(existing);

    const char *sql =
        "UPDATE executions SET status = 'running', started_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return ACTA_DB_ERR_SQL; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_cancel(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    execution_t *existing = acta_db_execution_get(db, id, NULL);
    if (!existing) return ACTA_DB_ERR_NOT_FOUND;

    if (strcmp(existing->status, ACTA_EXEC_STATUS_PENDING) != 0 &&
        strcmp(existing->status, ACTA_EXEC_STATUS_RUNNING) != 0) {
        acta_db_execution_free(existing);
        return ACTA_DB_ERR_INVALID;
    }
    acta_db_execution_free(existing);

    const char *sql =
        "UPDATE executions SET status = 'cancelled', completed_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return ACTA_DB_ERR_SQL; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_complete(db_t *db, int id, const char *result) {
    if (!db) return ACTA_DB_ERR_INVALID;

    execution_t *existing = acta_db_execution_get(db, id, NULL);
    if (!existing) return ACTA_DB_ERR_NOT_FOUND;

    if (strcmp(existing->status, ACTA_EXEC_STATUS_RUNNING) != 0) {
        acta_db_execution_free(existing);
        return ACTA_DB_ERR_INVALID;
    }
    acta_db_execution_free(existing);

    const char *sql =
        "UPDATE executions SET status = 'completed', result = ?, "
        "completed_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    if (result) sqlite3_bind_text(stmt, 1, result, -1, SQLITE_TRANSIENT);
    else        sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return ACTA_DB_ERR_SQL; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_fail(db_t *db, int id, const char *error) {
    if (!db) return ACTA_DB_ERR_INVALID;

    execution_t *existing = acta_db_execution_get(db, id, NULL);
    if (!existing) return ACTA_DB_ERR_NOT_FOUND;

    if (strcmp(existing->status, ACTA_EXEC_STATUS_RUNNING) != 0) {
        acta_db_execution_free(existing);
        return ACTA_DB_ERR_INVALID;
    }
    acta_db_execution_free(existing);

    const char *sql =
        "UPDATE executions SET status = 'failed', error = ?, "
        "completed_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    if (error) sqlite3_bind_text(stmt, 1, error, -1, SQLITE_TRANSIENT);
    else       sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return ACTA_DB_ERR_SQL; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_set_raw_response(db_t *db, int id, const char *raw) {
    if (!db) return ACTA_DB_ERR_INVALID;

    execution_t *existing = acta_db_execution_get(db, id, NULL);
    if (!existing) return ACTA_DB_ERR_NOT_FOUND;
    acta_db_execution_free(existing);

    const char *sql = "UPDATE executions SET raw_response = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    if (raw) sqlite3_bind_text(stmt, 1, raw, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return ACTA_DB_ERR_SQL; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/*  Free                                                              */
/* ------------------------------------------------------------------ */

void acta_db_execution_free(execution_t *e) {
    if (!e) return;
    free(e->prompt);       free(e->raw_response); free(e->result);
    free(e->status);       free(e->error);
    free(e->created_at);   free(e->started_at);   free(e->completed_at);
    free(e);
}

void acta_db_execution_list_free(execution_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_execution_free(items[i]);
    free(items);
}
