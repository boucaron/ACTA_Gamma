#include "internal.h"
#include "context.h"
#include "db.h"

#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Internal helpers
 * ═══════════════════════════════════════════════════════════════════ */

static context_t *row_to_context(sqlite3_stmt *stmt) {
    context_t *c = calloc(1, sizeof(context_t));
    if (!c) return NULL;
    c->id           = db_col_int(stmt, 0);
    c->type         = db_col_text(stmt, 1);
    c->content      = db_col_text(stmt, 2);
    c->content_hash = db_col_text(stmt, 3);
    c->metadata     = db_col_text(stmt, 4);
    c->created_at   = db_col_text(stmt, 5);
    return c;
}

static context_t **collect_rows(sqlite3_stmt *stmt,
                                int *out_count, int *err) {
    int  count = 0;
    context_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        context_t *item = row_to_context(stmt);
        if (!item) {
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            if (err)       *err       = ACTA_DB_ERR_ALLOC;
            if (out_count) *out_count = 0;
            return NULL;
        }

        context_t **tmp = realloc(items,
                                  sizeof(context_t *) * (count + 1));
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
    return items;
}

/* ── Dynamic SQL builder ────────────────────────────────────────────
 *
 * Builds a "SELECT … FROM contexts [WHERE …] [AND id > ?]
 *          ORDER BY id [LIMIT ?] [OFFSET ?]" statement into buf.
 *
 * Returns the number of characters written, or -1 on overflow.
 * *out_n_where receives the count of WHERE-clause parameters
 * (type, hash, after_id – in that order) so the caller knows
 * where the LIMIT/OFFSET parameters start. */
static int build_select_sql(char *buf, size_t sz,
                            const context_query_t *q,
                            const context_page_t  *p,
                            int *out_n_where)
{
    int pos       = 0;
    int n_where   = 0;
    int has_where = 0;
    int rc;

    rc = snprintf(buf + pos, sz - pos,
                  "SELECT id, type, content, content_hash, metadata, "
                  "created_at FROM contexts");
    if (rc < 0 || (size_t)rc >= sz) return -1;
    pos += rc;

    /* WHERE type = ? */
    if (q && q->type) {
        rc = snprintf(buf + pos, sz - pos,
                      has_where ? " AND type = ?"
                                : " WHERE type = ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += rc;
        has_where = 1;
        n_where++;
    }

    /* WHERE content_hash = ? */
    if (q && q->hash) {
        rc = snprintf(buf + pos, sz - pos,
                      has_where ? " AND content_hash = ?"
                                : " WHERE content_hash = ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += rc;
        has_where = 1;
        n_where++;
    }

    /* Keyset: AND id > ? */
    if (p && p->after_id > 0) {
        rc = snprintf(buf + pos, sz - pos,
                      has_where ? " AND id > ?"
                                : " WHERE id > ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += rc;
        has_where = 1;
        n_where++;
    }

    rc = snprintf(buf + pos, sz - pos, " ORDER BY id");
    if (rc < 0 || (size_t)rc >= sz - pos) return -1;
    pos += rc;

    /* LIMIT / OFFSET
     * SQLite requires a LIMIT expression before OFFSET.
     * If OFFSET is needed but no LIMIT, emit "LIMIT -1 OFFSET ?". */
    int has_limit  = (p && p->limit  > 0);
    int has_offset = (p && p->offset > 0);

    if (has_limit || has_offset) {
        if (has_limit) {
            rc = snprintf(buf + pos, sz - pos, " LIMIT ?");
        } else {
            rc = snprintf(buf + pos, sz - pos, " LIMIT -1");
        }
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += rc;

        if (has_offset) {
            rc = snprintf(buf + pos, sz - pos, " OFFSET ?");
            if (rc < 0 || (size_t)rc >= sz - pos) return -1;
            pos += rc;
        }
    }

    *out_n_where = n_where;
    return pos;
}


/* Builds a "SELECT COUNT(*) FROM contexts [WHERE …]" statement.
 * Returns chars written or -1. */
static int build_count_sql(char *buf, size_t sz,
                           const context_query_t *q)
{
    int pos = 0;
    int has_where = 0;
    int rc;

    rc = snprintf(buf + pos, sz - pos,
                  "SELECT COUNT(*) FROM contexts");
    if (rc < 0 || (size_t)rc >= sz) return -1;
    pos += rc;

    if (q && q->type) {
        rc = snprintf(buf + pos, sz - pos,
                      has_where ? " AND type = ?"
                                : " WHERE type = ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += rc;
        has_where = 1;
    }

    if (q && q->hash) {
        rc = snprintf(buf + pos, sz - pos,
                      has_where ? " AND content_hash = ?"
                                : " WHERE content_hash = ?");
        if (rc < 0 || (size_t)rc >= sz - pos) return -1;
        pos += rc;
    }

    return pos;
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
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, c->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, c->content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, c->content_hash, -1, SQLITE_TRANSIENT);
    if (c->metadata)
        sqlite3_bind_text(stmt, 4, c->metadata, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 4);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return ACTA_DB_ERR_SQL;

    if (out_id)
        *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
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
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, id);
    context_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_context(stmt);
        if (!result) {
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  query  (unified paginated fetch)
 * ═══════════════════════════════════════════════════════════════════ */

context_t **acta_db_context_query(db_t *db,
                                  const context_query_t *q,
                                  const context_page_t  *p,
                                  int *out_count,
                                  int *err)
{
    if (err)       *err       = ACTA_DB_OK;
    if (out_count) *out_count = 0;

    /* ── validate ─────────────────────────────────────────────────── */
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (p) {
        if (p->offset < 0 || p->after_id < 0) {
            if (err) *err = ACTA_DB_ERR_INVALID;
            return NULL;
        }
        if (p->offset > 0 && p->after_id > 0) {
            /* mutually exclusive */
            if (err) *err = ACTA_DB_ERR_INVALID;
            return NULL;
        }
    }


    /* ── build SQL ────────────────────────────────────────────────── */
    char sql[512];
    int n_where;
    if (build_select_sql(sql, sizeof(sql), q, p, &n_where) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    /* ── bind WHERE params (1-based, in clause order) ────────────── */
    int bind = 1;

    if (q && q->type) {
        sqlite3_bind_text(stmt, bind++, q->type, -1, SQLITE_TRANSIENT);
    }
    if (q && q->hash) {
        sqlite3_bind_text(stmt, bind++, q->hash, -1, SQLITE_TRANSIENT);
    }
    if (p && p->after_id > 0) {
        sqlite3_bind_int(stmt, bind++, p->after_id);
    }

    /* ── bind LIMIT / OFFSET ─────────────────────────────────────── */
    if (p && p->limit > 0) {
        sqlite3_bind_int(stmt, bind++, p->limit);
    }
    if (p && p->offset > 0) {
        sqlite3_bind_int(stmt, bind++, p->offset);
    }

    (void)n_where;  /* used implicitly via bind order */

    /* ── collect ──────────────────────────────────────────────────── */
    return collect_rows(stmt, out_count, err);
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
    if (build_count_sql(sql, sizeof(sql), q) < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int bind = 1;
    if (q && q->type)
        sqlite3_bind_text(stmt, bind++, q->type, -1, SQLITE_TRANSIENT);
    if (q && q->hash)
        sqlite3_bind_text(stmt, bind++, q->hash, -1, SQLITE_TRANSIENT);

    int count = 0;
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
    context_page_t p = { .offset = offset,
                         .limit  = limit,
                         .after_id = 0 };
    return acta_db_context_query(db, NULL, &p, out_count, err);
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
    context_page_t  p = { .offset = offset,
                          .limit  = limit,
                          .after_id = 0 };
    return acta_db_context_query(db, &q, &p, out_count, err);
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
    context_page_t  p = { .offset = offset,
                          .limit  = limit,
                          .after_id = 0 };
    return acta_db_context_query(db, &q, &p, out_count, err);
}

/* ═══════════════════════════════════════════════════════════════════
 *  free
 * ═══════════════════════════════════════════════════════════════════ */

void acta_db_context_free(context_t *c) {
    if (!c) return;
    free(c->type);
    free(c->content);
    free(c->content_hash);
    free(c->metadata);
    free(c->created_at);
    free(c);
}

void acta_db_context_list_free(context_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_context_free(items[i]);
    free(items);
}
