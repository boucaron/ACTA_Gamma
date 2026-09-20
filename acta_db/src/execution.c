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
    COL_DELETED_AT,
    COL_COUNT
};

#define EXEC_SELECT \
    "SELECT id, context_id, skill_revision_id, model_revision_id, prompt, " \
    "raw_response, result, status, error, created_at, started_at, " \
    "completed_at, parent_execution_id, deleted_at FROM executions"

/* Light projection: the blob columns (prompt, raw_response, result,
 * error) are omitted.  Column order must match row_to_execution_light. */
#define EXEC_SELECT_LIGHT \
    "SELECT id, context_id, skill_revision_id, model_revision_id, status, " \
    "created_at, started_at, completed_at, parent_execution_id, " \
    "deleted_at FROM executions"

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
    e->deleted_at          = db_col_text(stmt, COL_DELETED_AT, &alloc_err);

    if (alloc_err) {
        acta_db_execution_free(e);
        if (err) *err = alloc_err;
        return NULL;
    }
    return e;
}

/*
 * Light-projection row decoder (EXEC_SELECT_LIGHT column order).
 * prompt / raw_response / result / error are intentionally left NULL
 * (the struct is calloc'd and acta_db_execution_free is NULL-safe).
 * Allocation-error handling is identical to row_to_execution.
 */
static execution_t *row_to_execution_light(sqlite3_stmt *stmt, int *err)
{
    execution_t *e = calloc(1, sizeof(execution_t));
    if (!e) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    e->id                  = db_col_int(stmt, 0);
    e->context_id          = db_col_int(stmt, 1);
    e->skill_revision_id   = db_col_int(stmt, 2);
    e->model_revision_id   = db_col_int(stmt, 3);
    e->status              = db_col_text(stmt, 4, &alloc_err);
    e->created_at          = db_col_text(stmt, 5, &alloc_err);
    e->started_at          = db_col_text(stmt, 6, &alloc_err);
    e->completed_at        = db_col_text(stmt, 7, &alloc_err);
    e->parent_execution_id = db_col_int_or_zero(stmt, 8);
    e->deleted_at          = db_col_text(stmt, 9, &alloc_err);

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

    /* Live-only default: constant clause, no bind, so the bind order
     * [status, parent, context, skill, model] is unchanged. */
    if (!q->include_deleted)
    { pos = where_append(sql, sql_sz, pos, &n, "deleted_at IS NULL"); if (pos < 0) return -1; }

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

/*
 * Lightweight status check (replaces full-row read in transitions).
 *
 * Refusals are recorded in last_error via db_set_error (KI-7): the
 * decision is made in C code without any SQL error ever occurring,
 * so without this the CLI would print "<op> failed: (no detail)".
 *
 *   row missing        -> ACTA_DB_ERR_NOT_FOUND,
 *                         "execution <id> does not exist"
 *   status mismatch    -> ACTA_DB_ERR_INVALID,
 *                         "execution <id> is '<actual>'; <why>"
 *
 * `why` is the requirement sentence (e.g. "start requires status
 * 'pending'").  On ACTA_DB_OK no message is stored.
 */
static int exec_verify_status(db_t *db, int id, const char *expected,
                             const char *why)
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
        if (s && strcmp((const char *)s, expected) == 0)
            result = ACTA_DB_OK;
        else {
            char msg[192];
            snprintf(msg, sizeof msg, "execution %d is '%s'; %s",
                     id, s ? (const char *)s : "(null)", why);
            db_set_error(db, msg);
            result = ACTA_DB_ERR_INVALID;
        }
    }
    else {
        char msg[80];
        snprintf(msg, sizeof msg, "execution %d does not exist", id);
        db_set_error(db, msg);
    }
    sqlite3_finalize(stmt);
    return result;
}

/* ------------------------------------------------------------------ */
/*  Unified query (paginated lister)                                  */
/*                                                                     */
/*  acta_db_execution_query (full blobs) and acta_db_execution_query_  */
/*  light (blob columns omitted) share this implementation; only the  */
/*  SELECT column list and the row decoder differ.                    */
/* ------------------------------------------------------------------ */

static execution_t **execution_query_impl(db_t *db,
                                          const execution_query_t *q,
                                          int offset, int limit,
                                          int *out_count, int *err,
                                          int light)
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
    /* NULL query == ACTA_EXEC_QUERY_ANY == live-only.  Routing it
     * through the builder guarantees the deleted_at IS NULL clause
     * is applied; all fields zero ⇒ no WHERE binds. */
    static const execution_query_t any = ACTA_EXEC_QUERY_ANY;
    const execution_query_t *eff = q ? q : &any;

    char where_clause[384];
    if (exec_build_where(where_clause, sizeof(where_clause), eff) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[600];
    int slen = snprintf(sql, sizeof(sql),
             "%s%s ORDER BY id ASC LIMIT ? OFFSET ?;",
             light ? EXEC_SELECT_LIGHT : EXEC_SELECT, where_clause);
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
    int idx = exec_bind_where(stmt, eff, 1);
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
        execution_t *item = light
            ? row_to_execution_light(stmt, &row_err)
            : row_to_execution(stmt, &row_err);
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

execution_t **acta_db_execution_query(db_t *db,
                                      const execution_query_t *q,
                                      int offset, int limit,
                                      int *out_count, int *err)
{
    return execution_query_impl(db, q, offset, limit, out_count, err,
                                /*light*/0);
}

execution_t **acta_db_execution_query_light(db_t *db,
                                            const execution_query_t *q,
                                            int offset, int limit,
                                            int *out_count, int *err)
{
    return execution_query_impl(db, q, offset, limit, out_count, err,
                                /*light*/1);
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

    /* NULL query == ACTA_EXEC_QUERY_ANY == live-only (same convention
     * as acta_db_execution_query; no WHERE binds for the effective
     * query). */
    static const execution_query_t any = ACTA_EXEC_QUERY_ANY;
    const execution_query_t *eff = q ? q : &any;

    char where_clause[384];
    if (exec_build_where(where_clause, sizeof(where_clause), eff) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
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

    exec_bind_where(stmt, eff, 1);

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
    if (!db || !e || e->context_id <= 0 || e->skill_revision_id <= 0 ||
        e->model_revision_id <= 0)
        return ACTA_DB_ERR_INVALID;

    /* e->prompt is IGNORED: executions.prompt is a legacy nullable
     * column that is never written by current code.  SQL NULL is
     * bound unconditionally; pre-removal rows may still hold a
     * historical value, which the getters keep readable. */

    /* The referenced context must be LIVE: a soft-deleted context
     * cannot receive a new execution (per spec, NOT_FOUND), while a
     * missing context keeps the unchanged FK semantics (ERR_FK). */
    {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle,
                "SELECT deleted_at FROM contexts WHERE id = ?;",
                -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, e->context_id);

        int ctx_missing = 1;
        int ctx_deleted = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            ctx_missing = 0;
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
                ctx_deleted = 1;
        }
        sqlite3_finalize(stmt);

        /* C-level FK/not-found decisions set no SQLite error, so store
         * the detail in last_error for the CLI error message (KI-7): the
         * connection's sqlite3_errmsg is "not an error" here. */
        if (ctx_missing) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "FOREIGN KEY violation: context %d does not exist",
                     e->context_id);
            db_set_error(db, msg);
            return ACTA_DB_ERR_FK;
        }
        if (ctx_deleted) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "context %d is soft-deleted",
                     e->context_id);
            db_set_error(db, msg);
            return ACTA_DB_ERR_NOT_FOUND;
        }
    }

    /* e->status is deliberately ignored: a new execution is always
     * created "pending"; later state is only reached via the
     * transition functions (start/complete/fail/cancel). */

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
    sqlite3_bind_null(stmt, 4); /* prompt: legacy column, never written */
    sqlite3_bind_text (stmt, 5, ACTA_EXEC_STATUS_PENDING, -1, SQLITE_TRANSIENT);
    if (e->parent_execution_id == 0)
        sqlite3_bind_null(stmt, 6);
    else
        sqlite3_bind_int (stmt, 6, e->parent_execution_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    /* The handle runs with extended result codes (see db.c), so an FK
     * violation comes back as SQLITE_CONSTRAINT_FOREIGNKEY, not the
     * generic SQLITE_CONSTRAINT (same convention as skill.c).
     * The only constraints on this INSERT are the four FKs, so a
     * constraint failure means a referenced row does not exist. */
    if (rc == SQLITE_CONSTRAINT_FOREIGNKEY) return ACTA_DB_ERR_FK;
    if (rc != SQLITE_DONE)                  return ACTA_DB_ERR_SQL;

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

    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_PENDING,
                               "start requires status 'pending'");
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
    if (changes > 0) return ACTA_DB_OK;
    /* Race: the status moved between the check and the guarded UPDATE. */
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d changed concurrently; start not applied", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_INVALID;
}

int acta_db_execution_cancel(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Allow cancel from pending or running.  A row in some other state
     * makes the pending check fail with INVALID (not NOT_FOUND), so fall
     * through to the running check before giving up.  The requirement
     * sentence covers both allowed states, so the message is correct
     * whichever check produced the refusal. */
    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_PENDING,
                               "cancel requires status 'pending' or 'running'");
    if (rc == ACTA_DB_ERR_INVALID)
        rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_RUNNING,
                               "cancel requires status 'pending' or 'running'");
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
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d changed concurrently; cancel not applied", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_INVALID;
}

int acta_db_execution_complete(db_t *db, int id, const char *result)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_RUNNING,
                               "complete requires status 'running'");
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
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d changed concurrently; complete not applied", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_INVALID;
}

int acta_db_execution_fail(db_t *db, int id, const char *error)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    int rc = exec_verify_status(db, id, ACTA_EXEC_STATUS_RUNNING,
                               "fail requires status 'running'");
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
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d changed concurrently; fail not applied", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_INVALID;
}

int acta_db_execution_reset(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* A deleted execution is inert: it must be restored before it can
     * be reset/retried.  Missing and deleted rows both map to
     * NOT_FOUND; only a live non-failed row is INVALID. */
    {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle,
                "SELECT status, deleted_at FROM executions WHERE id = ?;",
                -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, id);

        int rc = ACTA_DB_ERR_NOT_FOUND;
        char msg[192];
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *s = sqlite3_column_text(stmt, 0);
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL)
                rc = ACTA_DB_ERR_NOT_FOUND;          /* soft-deleted row */
            else if (s && strcmp((const char *)s, ACTA_EXEC_STATUS_FAILED) == 0)
                rc = ACTA_DB_OK;
            else
                rc = ACTA_DB_ERR_INVALID;           /* live, non-failed */
            if (rc == ACTA_DB_ERR_NOT_FOUND)
                snprintf(msg, sizeof msg,
                         "execution %d is soft-deleted; restore it before reset",
                         id);
            else if (rc == ACTA_DB_ERR_INVALID)
                snprintf(msg, sizeof msg,
                         "execution %d is '%s'; reset requires status 'failed'",
                         id, s ? (const char *)s : "(null)");
            if (rc != ACTA_DB_OK)
                db_set_error(db, msg);
        }
        else {
            snprintf(msg, sizeof msg, "execution %d does not exist", id);
            db_set_error(db, msg);
        }
        sqlite3_finalize(stmt);
        if (rc != ACTA_DB_OK) return rc;
    }

    const char *sql =
        "UPDATE executions "
        "SET    status = 'pending', error = NULL, raw_response = NULL, "
        "       started_at = NULL, completed_at = NULL "
        "WHERE  id = ? AND status = 'failed';";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d changed concurrently; reset not applied", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_INVALID;
}

/* ------------------------------------------------------------------ */
/*  Soft-delete lifecycle                                             */
/* ------------------------------------------------------------------ */

int acta_db_execution_delete(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Pre-check for the friendlier error codes; the guarded UPDATE
     * below is the authoritative check in a race (same pattern as the
     * transition functions).  A missing row or an already-deleted row
     * is NOT_FOUND (same contract as acta_db_context_delete); only a
     * `running` row is refused with INVALID. */
    {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle,
                "SELECT status, deleted_at FROM executions WHERE id = ?;",
                -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, id);

        int rc = ACTA_DB_ERR_NOT_FOUND;
        char msg[192];
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *s = sqlite3_column_text(stmt, 0);
            int deleted = sqlite3_column_type(stmt, 1) != SQLITE_NULL;
            int running = s && strcmp((const char *)s, ACTA_EXEC_STATUS_RUNNING) == 0;
            if (deleted)
                rc = ACTA_DB_ERR_NOT_FOUND;
            else if (running)
                rc = ACTA_DB_ERR_INVALID;
            else
                rc = ACTA_DB_OK;
            if (rc == ACTA_DB_ERR_INVALID)
                snprintf(msg, sizeof msg,
                         "cannot delete execution %d while its status is 'running'",
                         id);
            else if (rc == ACTA_DB_ERR_NOT_FOUND)
                snprintf(msg, sizeof msg,
                         "execution %d is already deleted", id);
            if (rc != ACTA_DB_OK)
                db_set_error(db, msg);
        }
        else {
            snprintf(msg, sizeof msg, "execution %d does not exist", id);
            db_set_error(db, msg);
        }
        sqlite3_finalize(stmt);
        if (rc != ACTA_DB_OK) return rc;
    }

    const char *sql =
        "UPDATE executions "
        "SET    deleted_at = datetime('now') "
        "WHERE  id = ? AND status IN ('pending', 'completed', 'failed', 'cancelled') "
        "AND    deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d changed concurrently; delete not applied", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_INVALID;
}

int acta_db_execution_restore(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Strict undelete: only a DELETED row may be restored (a live or
     * missing row is NOT_FOUND, like acta_db_context_restore).  The
     * status is untouched: a deleted failed row restores to failed and
     * can then be reset. */
    const char *sql =
        "UPDATE executions SET deleted_at = NULL "
        "WHERE id = ? AND deleted_at IS NOT NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int step_rc = sqlite3_step(stmt);
    int changes = (step_rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "execution %d is not deleted (nothing to restore)", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
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
    if (changes > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg, "execution %d does not exist", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
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
    free(e->deleted_at);
    free(e);
}

void acta_db_execution_list_free(execution_t **items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_execution_free(items[i]);
    free(items);
}
