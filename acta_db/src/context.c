#include "internal.h"
#include "context.h"
#include "db.h"

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

int acta_db_context_create(db_t *db, const context_t *c, int *out_id) {
    if (!db || !c || !c->type || !c->content || !c->content_hash || !out_id)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO contexts (type, content, content_hash, metadata) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, c->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, c->content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, c->content_hash, -1, SQLITE_TRANSIENT);
    if (c->metadata) sqlite3_bind_text(stmt, 4, c->metadata, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 4);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;

    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

context_t *acta_db_context_get(db_t *db, int id) {
    if (!db) return NULL;

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    sqlite3_bind_int(stmt, 1, id);
    context_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = row_to_context(stmt);
    sqlite3_finalize(stmt);
    return result;
}

context_t *acta_db_context_list_by_hash(db_t *db, const char *hash, int *out_count) {
    if (!db || !out_count) return NULL;
    if (!hash) {
        *out_count = 0;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts WHERE content_hash = ? ORDER BY id;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_count = 0;
        return NULL;
    }
    sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_TRANSIENT);

    int count = 0;
    context_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        context_t *item = row_to_context(stmt);
        if (!item) {
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            *out_count = 0;
            return NULL;
        }
        context_t *tmp = realloc(items, sizeof(context_t) * (count + 1));
        if (!tmp) {
            acta_db_context_free(item);
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

context_t *acta_db_context_list_all(db_t *db, int *out_count) {
    if (!db || !out_count) return NULL;

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts ORDER BY id;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_count = 0;
        return NULL;
    }

    int count = 0;
    context_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        context_t *item = row_to_context(stmt);
        if (!item) {
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            *out_count = 0;
            return NULL;
        }
        context_t *tmp = realloc(items, sizeof(context_t) * (count + 1));
        if (!tmp) {
            acta_db_context_free(item);
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

context_t *acta_db_context_list_by_type(db_t *db, const char *type, int *out_count) {
    if (!db || !out_count) return NULL;
    if (!type) {
        *out_count = 0;
        return NULL;
    }

    const char *sql =
        "SELECT id, type, content, content_hash, metadata, created_at "
        "FROM contexts WHERE type = ? ORDER BY id;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_count = 0;
        return NULL;
    }
    sqlite3_bind_text(stmt, 1, type, -1, SQLITE_TRANSIENT);

    int count = 0;
    context_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        context_t *item = row_to_context(stmt);
        if (!item) {
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            *out_count = 0;
            return NULL;
        }
        context_t *tmp = realloc(items, sizeof(context_t) * (count + 1));
        if (!tmp) {
            acta_db_context_free(item);
            sqlite3_finalize(stmt);
            acta_db_context_list_free(items, count);
            *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

void acta_db_context_free(context_t *c) {
    if (!c) return;
    free(c->type);
    free(c->content);
    free(c->content_hash);
    free(c->metadata);
    free(c->created_at);
    free(c);
}

void acta_db_context_list_free(context_t *items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        free(items[i].type);
        free(items[i].content);
        free(items[i].content_hash);
        free(items[i].metadata);
        free(items[i].created_at);
    }
    free(items);
}
