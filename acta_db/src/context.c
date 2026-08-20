#include "internal.h"
#include "context.h"
#include "db.h"

/* ---------- internal helpers ---------- */

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

/* ---------- create ---------- */

int acta_db_context_create(db_t *db, const context_t *c, int *out_id) {
    if (!db || !c || !c->type || !c->content || !c->content_hash)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO contexts (type, content, content_hash, metadata) VALUES (?, ?, ?, ?);";
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

/* ---------- get ---------- */

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

/* ---------- list helpers ---------- */

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

/* ---------- list_all ---------- */

context_t **acta_db_context_list_all(db_t *db, int offset, int limit,
                                      int *out_count, int *err) {
    if (err) *err = ACTA_DB_OK;

    if (!db || offset < 0) {
        if (err)       *err       = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts ORDER BY id LIMIT ? OFFSET ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err)       *err       = ACTA_DB_ERR_SQL;
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* SQLite: LIMIT -1 means "no limit". */
    sqlite3_bind_int(stmt, 1, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, 2, offset);

    return collect_rows(stmt, out_count, err);
}

/* ---------- list_by_type ---------- */

context_t **acta_db_context_list_by_type(db_t *db, const char *type,
                                          int offset, int limit,
                                          int *out_count, int *err) {
    if (err) *err = ACTA_DB_OK;

    if (!db || !type || offset < 0) {
        if (err)       *err       = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts WHERE type = ? ORDER BY id LIMIT ? OFFSET ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err)       *err       = ACTA_DB_ERR_SQL;
        if (out_count) *out_count = 0;
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, 3, offset);

    return collect_rows(stmt, out_count, err);
}

/* ---------- list_by_hash ---------- */

context_t **acta_db_context_list_by_hash(db_t *db, const char *hash,
                                          int offset, int limit,
                                          int *out_count, int *err) {
    if (err) *err = ACTA_DB_OK;

    if (!db || !hash || offset < 0) {
        if (err)       *err       = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts WHERE content_hash = ? ORDER BY id LIMIT ? OFFSET ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err)       *err       = ACTA_DB_ERR_SQL;
        if (out_count) *out_count = 0;
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, 3, offset);

    return collect_rows(stmt, out_count, err);
}

/* ---------- free ---------- */

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
