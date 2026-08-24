#include "internal.h"
#include "execution.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Column order shared by EXEC_SELECT and row_to_execution           */
/* ------------------------------------------------------------------ */

enum {
    COL_ID = 0, COL_CONTEXT_ID, COL_SKILL_REV_ID, COL_MODEL_REV_ID,
    COL_PROMPT, COL_RAW_RESPONSE, COL_RESULT, COL_STATUS, COL_ERROR,
    COL_CREATED_AT, COL_STARTED_AT, COL_COMPLETED_AT, COL_PARENT_ID,
    COL_COUNT
};

#define EXEC_SELECT \
    "SELECT id, context_id, skill_revision_id, model_revision_id, prompt, " \
    "raw_response, result, status, error, created_at, started_at, " \
    "completed_at, parent_execution_id FROM executions"

/* ------------------------------------------------------------------ */
/*  Row decoding (allocation-error aware)                             */
/* ------------------------------------------------------------------ */

static execution_t *row_to_execution(sqlite3_stmt *stmt, int *err)
{
    execution_t *e = calloc(1, sizeof(execution_t));
    if (!e) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    e->id                  = db_col_int(stmt, COL_ID);
    e->context_id          = db_col_int(stmt, COL_CONTEXT_ID);
    e->skill_revision_id   = db_col_int(stmt, COL_SKILL_REV_ID);
    e->model_revision_id   = db_col_int(stmt, COL_MODEL_REV_ID);
    e->prompt              = db_col_text(stmt, COL_PROMPT, &alloc_err);
    e->raw_response        = db_col_text(stmt, COL_RAW_RESPONSE, &alloc_err);
    e->result              = db_col_text(stmt, COL_RESULT, &alloc_err);
    e->status              = db_col_text(stmt, COL_STATUS, &alloc_err);
    e->error               = db_col_text(stmt, COL_ERROR, &alloc_err);
    e->created_at          = db_col_text(stmt, COL_CREATED_AT, &alloc_err);
    e->started_at          = db_col_text(stmt, COL_STARTED_AT, &alloc_err);
    e->completed_at        = db_col_text(stmt, COL_COMPLETED_AT, &alloc_err);
    e->parent_execution_id = db_col_int_or_zero(stmt, COL_PARENT_ID);

    if (alloc_err) {
        acta_db_execution_free(e);
        if (err) *err = alloc_err;
        return NULL;
    }
    return e;
}

/* ------------------------------------------------------------------ */
/*  Dynamic WHERE-clause builder (shared by query + count)            */
/* ------------------------------------------------------------------ */

/*
 * Append one clause to the running WHERE string.
 * Returns new position, or -1 on buffer overflow.
 */
static int where_append(char *buf, size_t sz, int pos, int *n,
                        const char *clause)
{
    int rc = snprintf(buf + pos, sz - (size_t)pos,
                     "%s%s", *n ? " AND " : "", clause);
    if (rc < 0 || (size_t)rc >= sz - (size_t)pos)
        return -1;
    pos += rc;
    (*n)++;
    return pos;
}

/*
 * Build " WHERE <predicates>" into sql.
 * Returns number of predicates (0 → sql[0] = '\0'), or -1 on overflow.
 */
static int exec_build_where(char *sql, size_t sql_sz,
                            const execution_query_t *q)
{
    if (!sql || !sql_sz) return -1;

    int rc = snprintf(sql, sql_sz, " WHERE ");
    if (rc < 0 || (size_t)rc >= sql_sz) return -1;
    int pos = rc;
    int n   = 0;

    if (q->status)              { pos = where_append(sql, sql_sz, pos, &n, "status = ?");              if (pos < 0) return -1; }
    if (q->parent_execution_id) { pos = where_append(sql, sql_sz, pos, &n, "parent_execution_id = ?"); if (pos < 0) return -1; }
    if (q->context_id)          { pos = where_append(sql, sql_sz, pos, &n, "context_id = ?");          if (pos < 0) return -1; }
    if (q->skill_revision_id)   { pos = where_append(sql, sql_sz, pos, &n, "skill_revision_id = ?");   if (pos < 0) return -1; }
    if (q->model_revision_id)   { pos = where_append(sql, sql_sz, pos, &n, "model_revision_id = ?");   if (pos < 0) return -1; }

    if (n == 0)
        sql[0] = '\0';
    return n;
}

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
/*  Lightweight status check (replaces full-row read in transitions)  */
/* ------------------------------------------------------------------ */

static int exec_verify_status(db_t *db, int id, const char *expected)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle,
            "SELECT status FROM executions WHERE id = ?;",
            -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_int(stmt, 1, id);

    int result = ACTA_DB_ERR_NOT_FOUND;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *s = sqlite3_column_text(stmt, 0);
        result = (s && strcmp((const char *)s, expected) == 0)
                   ? ACTA_DB_OK
                   : ACTA_DB_ERR_INVALID;
    }
    sqlite3_finalize(stmt);
    return result;
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
    limit = db_clamp_limit(limit);
    if (out_count) *out_count = 0;

    /* ── build SQL ─────────────────────────────────────────────── */
    char where_clause[384];

    if (q) {
        if (exec_build_where(where_clause, sizeof(where_clause), q) < 0) {
            if (err) *err = ACTA_DB_ERR_INVALID;
            return NULL;
        }
    } else {
        where_clause[0] = '\0';
    }

    char sql[600];
    int slen = snprintf(sql, sizeof(sql),
             EXEC_SELECT "%s ORDER BY id ASC LIMIT ? OFFSET ?;",
             where_clause);
    if (slen < 0 || (size_t)slen >= sizeof(sql)) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

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
    int          count    = 0;
    size_t      capacity  = 0;
    execution_t **items   = NULL;

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

        int row_err = ACTA_DB_OK;
        execution_t *item = row_to_execution(stmt, &row_err);
        if (!item) {
            acta_db_execution_list_free(items, count);
            sqlite3_finalize(stmt);
            if (err) *err = row_err;
            return NULL;
        }
        items[count++] = item;
    }

    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err)       *err       = ACTA_DB_OK;

    return items;   /* NULL when count == 0 — same as before, just explicit */
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
        if (exec_build_where(where_clause, sizeof(where_clause), q) < 0) {
            if (err) *err = ACTA_DB_ERR_INVALID;
            return -1;
        }
    } else {
        where_clause[0] = '\0';
    }

    char sql[500];
    int slen = snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM executions%s;", where_clause);
    if (slen < 0 || (size_t)slen >= sizeof(sql)) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int idx = 1;
    if (q) idx = exec_bind_where(stmt, q, idx);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = (int)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    if (err) *err = (result >= 0) ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
    return result;
}

/* ------------------------------------------------------------------ */
/*  Create                                                            */
/* ------------------------------------------------------------------ */

int acta_db_execution_create(db_t *db, const execution_t *e, int *out_id)
{
    if (!db || !e || !e->prompt)
        return ACTA_DB_ERR_INVALID;

    const char *status = e->status ? e->status : ACTA_EXEC_STATUS_PENDING;

    const char *sql =
        "INSERT INTO executions "
        " (context_id, skill_revision_id, model_revision_id, "
        "  prompt, status, parent_execution_id) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_int  (stmt, 1, e->context_id);
    sqlite3_bind_int  (stmt, 2, e->skill_revision_id);
    sqlite3_bind_int  (stmt, 3, e->model_revision_id);
    sqlite3_bind_text (stmt, 4, e->prompt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 5, status, -1, SQLITE_TRANSIENT);
    if (e->parent_execution_id == 0)
        sqlite3_bind_null(stmt, 6);
    else
        sqlite3_bind_int (stmt, 6, e->parent_execution_id);

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

execution_t *acta_db_execution_get(db_t *db, int id, int *err)
{
    if (!db || id <= 0) {
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
        int row_err = ACTA_DB_OK;
        result = row_to_execution(stmt, &row_err);
        if (!result && err) *err = row_err;
    }
    sqlite3_finalize(stmt);

    if (err && result)
        *err = ACTA_DB_OK;
    /* If result is NULL and no alloc error was set, *err is already
     * ACTA_DB_OK (meaning "not found"), which is correct. */

    return result;
}

/* ------------------------------------------------------------------ */
/*  State transitions                                                 */
/* ------------------------------------------------------------------ */

int acta_db_execution_start(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_PENDING);
    if (rc != ACTA_DB_OK) return rc;

    const char *sql =
        "UPDATE executions "
        "SET    status = 'running', started_at = datetime('now') "
        "WHERE  id = ? AND status = 'pending';";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_cancel(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Allow cancel from pending or running. */
    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_PENDING);
    if (rc == ACTA_DB_ERR_NOT_FOUND)
        rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_RUNNING);
    if (rc != ACTA_DB_OK) return rc;

    const char *sql =
        "UPDATE executions "
        "SET    status = 'cancelled', completed_at = datetime('now') "
        "WHERE  id = ? AND status IN ('pending', 'running');";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_complete(db_t *db, int id, const char *result)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_RUNNING);
    if (rc != ACTA_DB_OK) return rc;

    const char *sql =
        "UPDATE executions "
        "SET    status = 'completed', result = ?, completed_at = datetime('now') "
        "WHERE  id = ? AND status = 'running';";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    if (result) sqlite3_bind_text(stmt, 1, result, -1, SQLITE_TRANSIENT);
    else        sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_fail(db_t *db, int id, const char *error)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_RUNNING);
    if (rc != ACTA_DB_OK) return rc;

    const char *sql =
        "UPDATE executions "
        "SET    status = 'failed', error = ?, completed_at = datetime('now') "
        "WHERE  id = ? AND status = 'running';";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    if (error) sqlite3_bind_text(stmt, 1, error, -1, SQLITE_TRANSIENT);
    else       sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID;
}

int acta_db_execution_set_raw_response(db_t *db, int id, const char *raw)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE executions SET raw_response = ? WHERE id = ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    if (raw) sqlite3_bind_text(stmt, 1, raw, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/*  Free                                                              */
/* ------------------------------------------------------------------ */

void acta_db_execution_free(execution_t *e)
{
    if (!e) return;
    free(e->prompt);
    free(e->raw_response);
    free(e->result);
    free(e->status);
    free(e->error);
    free(e->created_at);
    free(e->started_at);
    free(e->completed_at);
    free(e);
}

void acta_db_execution_list_free(execution_t **items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_execution_free(items[i]);
    free(items);
}
