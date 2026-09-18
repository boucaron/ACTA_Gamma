#include "internal.h"
#include "context.h"
#include "db.h"

#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Row decoder
 * ═══════════════════════════════════════════════════════════════════
 *
 *  light – 0: full projection, column order
 *              [id, type, content, content_hash, metadata, created_at,
 *               deleted_at]
 *          1: light projection (blob column omitted), column order
 *              [id, type, content_hash, metadata, created_at, deleted_at]
 *            c->content is left NULL (the struct is calloc'd and the
 *            free functions are NULL-safe).
 */

static context_t *row_to_context(sqlite3_stmt *stmt, int *err, int light) {
    context_t *c = calloc(1, sizeof(context_t));
    if (!c) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    c->id           = db_col_int(stmt, 0);
    c->type         = db_col_text(stmt, 1, &alloc_err);
    if (!light)               /* light projection: content column omitted */
        c->content      = db_col_text(stmt, 2, &alloc_err);
    c->content_hash = db_col_text(stmt, light ? 2 : 3, &alloc_err);
    c->metadata     = db_col_text(stmt, light ? 3 : 4, &alloc_err);
    c->created_at   = db_col_text(stmt, light ? 4 : 5, &alloc_err);
    c->deleted_at   = db_col_text(stmt, light ? 5 : 6, &alloc_err);

    if (alloc_err) {
        acta_db_context_free(c);
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }
    return c;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Row collector (shared by query / legacy listers)
 * ═══════════════════════════════════════════════════════════════════ */

static context_t **collect_rows(sqlite3_stmt *stmt,
                                int *out_count, int *err, int light) {
    int  count = 0;
    context_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int row_err = ACTA_DB_OK;
        context_t *item = row_to_context(stmt, &row_err, light);
        if (!item) {
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            if (err)       *err       = row_err;
            if (out_count) *out_count = 0;
            return NULL;
        }

        context_t **tmp = realloc(items,
                                  sizeof(context_t *) * (size_t)(count + 1));
        if (!tmp) {
            acta_db_context_free(item);
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            if (err)       *err       = ACTA_DB_ERR_ALLOC;
            if (out_count) *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }

    sqlite3_finalize(stmt);
    if (out_count) *out_count = count;
    if (err)       *err       = ACTA_DB_OK;
    return items;   /* NULL when count == 0 – caller treats as "empty" */
}

/* ═══════════════════════════════════════════════════════════════════
 *  Dynamic SQL builders
 *
 *  Both builders write into a fixed-size buffer and return the number
 *  of characters written, or -1 on overflow.  All user data is bound
 *  via sqlite3_bind_*; these functions only emit static fragments and
 *  '?' placeholders.
 *
 *  Precondition (enforced by the caller):
 *      limit  – already clamped to [1, ACTA_DB_MAX_PAGE].
 *
 *  Bind order (must match call sites):
 *      [type, hash, limit, (offset)]
 *  i.e. WHERE params first, then LIMIT, then OFFSET.
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Builds " WHERE <predicates>" for the context query family.
 *
 * live_only – when non-zero, appends the static clause
 *             "AND deleted_at IS NULL" (no extra bind; the constant
 *             clause keeps the bind order [type, hash] unchanged).
 *
 * Returns the length written, or -1 on buffer overflow.
 */
static int build_where(char *buf, size_t sz, const context_query_t *q,
                       int live_only) {
    size_t pos = 0;
    int    rc;
    int    have_predicate = 0;

    if (q && q->type) {
        rc = snprintf(buf + pos, sz - pos, " WHERE type = ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += (size_t)rc;
        have_predicate = 1;

        if (q->hash) {
            rc = snprintf(buf + pos, sz - pos, " AND content_hash = ?");
            if (rc < 0 || (size_t)rc >= sz - pos) return -1;
            pos += (size_t)rc;
        }
    } else if (q && q->hash) {
        rc = snprintf(buf + pos, sz - pos, " WHERE content_hash = ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += (size_t)rc;
        have_predicate = 1;
    }

    /* The live-only clause is constant (no bind). It opens the WHERE
     * clause itself when no type/hash predicate is present. */
    if (live_only) {
        rc = snprintf(buf + pos, sz - pos,
                      "%s deleted_at IS NULL", have_predicate ? " AND" : " WHERE");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += (size_t)rc;
    }
    return (int)pos;
}

static int build_select_sql(char *buf, size_t sz,
                            const context_query_t *q,
                            int offset, int limit,
                            int live_only, int light)
{
    size_t pos;
    int    rc;

    /* limit is guaranteed > 0 by db_clamp_limit at the call site. */
    (void)limit;   /* silence unused-param warning if compiler inlines */

    /* Light projection omits the content blob column; the column order
     * must match row_to_context(light). */
    rc = snprintf(buf, sz,
                  light
                      ? "SELECT id, type, content_hash, metadata, "
                        "created_at, deleted_at FROM contexts"
                      : "SELECT id, type, content, content_hash, metadata, "
                        "created_at, deleted_at FROM contexts");
    if (rc < 0 || (size_t)rc >= sz) return -1;
    pos = (size_t)rc;

    rc = build_where(buf + pos, sz - pos, q, live_only);
    if (rc < 0) return -1;
    pos += (size_t)rc;

    rc = snprintf(buf + pos, sz - pos, " ORDER BY id");
    if (rc < 0 || (size_t)rc >= sz - pos) return -1;
    pos += (size_t)rc;

    /* Always emit LIMIT; the clamp guarantees a positive value. */
    rc = snprintf(buf + pos, sz - pos, " LIMIT ?");
    if (rc < 0 || (size_t)rc >= sz - pos) return -1;
    pos += (size_t)rc;

    if (offset > 0) {
        rc = snprintf(buf + pos, sz - pos, " OFFSET ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += (size_t)rc;
    }

    return (int)pos;
}

static int build_count_sql(char *buf, size_t sz, const context_query_t *q,
                           int live_only) {
    size_t pos;
    int    rc;

    rc = snprintf(buf, sz, "SELECT COUNT(*) FROM contexts");
    if (rc < 0 || (size_t)rc >= sz) return -1;
    pos = (size_t)rc;

    rc = build_where(buf + pos, sz - pos, q, live_only);
    if (rc < 0) return -1;
    pos += (size_t)rc;

    return (int)pos;
}

/* ── Bind helpers ───────────────────────────────────────────────────
 * Binds the WHERE parameters (type, hash) in the same order the
 * builder emitted them.  Returns the next free bind index so the
 * caller can continue with LIMIT / OFFSET.
 *
 * Returns ACTA_DB_ERR_SQL if any bind fails, ACTA_DB_OK on success. */

static int bind_where(sqlite3_stmt *stmt, const context_query_t *q,
                      int *next_idx) {
    int idx = 1;

    if (q) {
        if (q->type) {
            if (sqlite3_bind_text(stmt, idx++, q->type,
                                  -1, SQLITE_TRANSIENT) != SQLITE_OK)
                return ACTA_DB_ERR_SQL;
        }
        if (q->hash) {
            if (sqlite3_bind_text(stmt, idx++, q->hash,
                                  -1, SQLITE_TRANSIENT) != SQLITE_OK)
                return ACTA_DB_ERR_SQL;
        }
    }
    if (next_idx) *next_idx = idx;
    return ACTA_DB_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 *  create
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_context_create(db_t *db, const context_t *c, int *out_id) {
    if (!db || !c || !c->type || !c->content || !c->content_hash)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO contexts (type, content, content_hash, metadata) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    int rc = ACTA_DB_OK;
    if (sqlite3_bind_text(stmt, 1, c->type, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(stmt, 2, c->content, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(stmt, 3, c->content_hash, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        (c->metadata
             ? sqlite3_bind_text(stmt, 4, c->metadata, -1, SQLITE_TRANSIENT)
             : sqlite3_bind_null(stmt, 4)) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            if (out_id)
                *out_id = (int)sqlite3_last_insert_rowid(db->handle);
        } else {
            rc = ACTA_DB_ERR_SQL;
        }
    } else {
        rc = ACTA_DB_ERR_SQL;
    }

    sqlite3_finalize(stmt);
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════
 *  get
 * ═══════════════════════════════════════════════════════════════════ */

context_t *acta_db_context_get(db_t *db, int id, int *err) {
    if (err) *err = ACTA_DB_OK;

    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at, "
        "deleted_at "
        "FROM contexts WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);

    context_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = row_to_context(stmt, err, 0);

    sqlite3_finalize(stmt);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  get_live  (live-only single-row fetch)
 * ═══════════════════════════════════════════════════════════════════ */

context_t *acta_db_context_get_live(db_t *db, int id, int *err) {
    if (err) *err = ACTA_DB_OK;

    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at, "
        "deleted_at "
        "FROM contexts WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);

    context_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = row_to_context(stmt, err, 0);

    sqlite3_finalize(stmt);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  delete / restore  (soft-delete lifecycle)
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_context_delete(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE contexts SET deleted_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    int changed = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    /* 0 rows: either the id does not exist or the row is already
     * deleted – both map to NOT_FOUND by contract. */
    if (changed > 0) return ACTA_DB_OK;
    /* C-level decision without a SQL error: record the detail for the
     * CLI's error message (KI-7). */
    char msg[192];
    snprintf(msg, sizeof msg,
             "context %d does not exist or is already deleted", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_context_restore(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Strict undelete: only a DELETED row may be restored.  A live
     * row or a missing row both yield NOT_FOUND (unlike
     * acta_db_skill_restore, which tolerates the already-live no-op). */
    const char *sql =
        "UPDATE contexts SET deleted_at = NULL "
        "WHERE id = ? AND deleted_at IS NOT NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    int changed = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changed > 0) return ACTA_DB_OK;
    /* 0 rows: either the id does not exist or the row is live —
     * distinguish so the refusal reason is accurate (KI-7). */
    char msg[192];
    const char *probe_sql = "SELECT 1 FROM contexts WHERE id = ?;";
    sqlite3_stmt *probe;
    if (sqlite3_prepare_v2(db->handle, probe_sql, -1, &probe, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(probe, 1, id);
    int exists = (sqlite3_step(probe) == SQLITE_ROW);
    sqlite3_finalize(probe);
    if (exists)
        snprintf(msg, sizeof msg,
                 "context %d is not deleted (nothing to restore)", id);
    else
        snprintf(msg, sizeof msg, "context %d does not exist", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
}

/* ═══════════════════════════════════════════════════════════════════
 *  query  (paginated fetch)
 *
 *  All four lister entry points (live/light × with-deleted/light)
 *  share this implementation; only live_only and light differ.
 * ═══════════════════════════════════════════════════════════════════ */

static context_t **context_query_impl(db_t *db,
                                      const context_query_t *q,
                                      int offset,
                                      int limit,
                                      int *out_count,
                                      int *err,
                                      int live_only,
                                      int light)
{
    if (err)       *err       = ACTA_DB_OK;
    if (out_count) *out_count = 0;

    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    /* Enforce the hard page cap. */
    limit = db_clamp_limit(limit);

    char sql[512];
    if (build_select_sql(sql, sizeof(sql), q, offset, limit,
                         live_only, light) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int bind = 1;
    if (bind_where(stmt, q, &bind) != ACTA_DB_OK) {
        sqlite3_finalize(stmt);
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    /* limit is always > 0 after the clamp – bind unconditionally. */
    if (sqlite3_bind_int(stmt, bind++, limit) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    if (offset > 0) {
        if (sqlite3_bind_int(stmt, bind++, offset) != SQLITE_OK) {
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_SQL;
            return NULL;
        }
    }

    return collect_rows(stmt, out_count, err, light);
}

context_t **acta_db_context_query(db_t *db,
                                  const context_query_t *q,
                                  int offset,
                                  int limit,
                                  int *out_count,
                                  int *err)
{
    return context_query_impl(db, q, offset, limit, out_count, err,
                              /*live_only*/1, /*light*/0);
}

context_t **acta_db_context_query_light(db_t *db,
                                         const context_query_t *q,
                                         int offset,
                                         int limit,
                                         int *out_count,
                                         int *err)
{
    return context_query_impl(db, q, offset, limit, out_count, err,
                              /*live_only*/1, /*light*/1);
}

/* ═══════════════════════════════════════════════════════════════════
 *  count
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_context_count(db_t *db,
                          const context_query_t *q,
                          int *err)
{
    if (err) *err = ACTA_DB_OK;

    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    char sql[256];
    if (build_count_sql(sql, sizeof(sql), q, 1) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int next = 1;
    if (bind_where(stmt, q, &next) != ACTA_DB_OK) {
        sqlite3_finalize(stmt);
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = db_col_int(stmt, 0);
    sqlite3_finalize(stmt);

    return count;
}

/* ═══════════════════════════════════════════════════════════════════
 *  query / count with deleted rows
 * ═══════════════════════════════════════════════════════════════════ */

context_t **acta_db_context_query_with_deleted(db_t *db,
                                               const context_query_t *q,
                                               int offset,
                                               int limit,
                                               int *out_count,
                                               int *err)
{
    return context_query_impl(db, q, offset, limit, out_count, err,
                              /*live_only*/0, /*light*/0);
}

context_t **acta_db_context_query_with_deleted_light(db_t *db,
                                                     const context_query_t *q,
                                                     int offset,
                                                     int limit,
                                                     int *out_count,
                                                     int *err)
{
    return context_query_impl(db, q, offset, limit, out_count, err,
                              /*live_only*/0, /*light*/1);
}

int acta_db_context_count_with_deleted(db_t *db,
                                       const context_query_t *q,
                                       int *err)
{
    if (err) *err = ACTA_DB_OK;

    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    char sql[256];
    if (build_count_sql(sql, sizeof(sql), q, 0) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int next = 1;
    if (bind_where(stmt, q, &next) != ACTA_DB_OK) {
        sqlite3_finalize(stmt);
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = db_col_int(stmt, 0);
    sqlite3_finalize(stmt);

    return count;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Legacy listers  (thin wrappers – kept for one release)
 * ═══════════════════════════════════════════════════════════════════ */

context_t **acta_db_context_list_all(db_t *db,
                                     int offset, int limit,
                                     int *out_count, int *err)
{
    return acta_db_context_query(db, NULL, offset, limit,
                                 out_count, err);
}

context_t **acta_db_context_list_by_type(db_t *db,
                                         const char *type,
                                         int offset, int limit,
                                         int *out_count,
                                         int *err)
{
    if (!type) {
        if (err)       *err       = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

    context_query_t q = { .type = type, .hash = NULL };
    return acta_db_context_query(db, &q, offset, limit,
                                 out_count, err);
}

context_t **acta_db_context_list_by_hash(db_t *db,
                                         const char *hash,
                                         int offset, int limit,
                                         int *out_count,
                                         int *err)
{
    if (!hash) {
        if (err)       *err       = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

    context_query_t q = { .type = NULL, .hash = hash };
    return acta_db_context_query(db, &q, offset, limit,
                                 out_count, err);
}

/* ═══════════════════════════════════════════════════════════════════
 *  free
 * ═══════════════════════════════════════════════════════════════════ */

void acta_db_context_free(context_t *c) {
    if (!c) return;
    DB_FREE_STR(c->type);
    DB_FREE_STR(c->content);
    DB_FREE_STR(c->content_hash);
    DB_FREE_STR(c->metadata);
    DB_FREE_STR(c->created_at);
    DB_FREE_STR(c->deleted_at);
    free(c);
}

void acta_db_context_list_free(context_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_context_free(items[i]);
    free(items);
}
