/* model.c */
#include "internal.h"
#include "model.h"
#include "db.h"

/* ---------- helpers ---------- */

/*
 * Decode one result row into a heap-allocated model_t.
 *
 * On success returns the struct.  On failure (calloc or any
 * db_col_text allocation) the partial struct is freed internally
 * and NULL is returned with *err set to ACTA_DB_ERR_ALLOC.
 *
 * The caller must initialise *err to ACTA_DB_OK before calling.
 */
static model_t *row_to_model(sqlite3_stmt *stmt, int *err) {
    model_t *m = calloc(1, sizeof(model_t));
    if (!m) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    m->id               = db_col_int(stmt, 0);
    m->folder_id        = db_col_int_or_zero(stmt, 1);
    m->name             = db_col_text(stmt, 2, err);
    m->description      = db_col_text(stmt, 3, err);
    m->backend          = db_col_text(stmt, 4, err);
    m->base_url         = db_col_text(stmt, 5, err);
    m->model_identifier = db_col_text(stmt, 6, err);
    m->configuration    = db_col_text(stmt, 7, err);
    m->created_at       = db_col_text(stmt, 8, err);
    m->updated_at       = db_col_text(stmt, 9, err);
    m->deleted_at       = db_col_text(stmt, 10, err);

    if (err && *err != ACTA_DB_OK) {
        acta_db_model_free(m);   /* frees the fields that were set */
        free(m);
        return NULL;
    }
    return m;
}

/* Append one item to a growable array.
 * Returns the new array pointer, or NULL on realloc failure
 * (the original array is still valid in that case). */
static model_t **list_push(model_t **items, int *count, model_t *item) {
    model_t **tmp = realloc(items, sizeof(model_t *) * (size_t)(*count + 1));
    if (!tmp) return NULL;
    tmp[*count] = item;
    (*count)++;
    return tmp;
}

/* ---------- mutators ---------- */

int acta_db_model_create(db_t *db, const model_t *m, int *out_id) {
    if (!db || !m || !m->name || !m->backend || !m->model_identifier)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO models (folder_id, name, description, backend, base_url, "
        "model_identifier, configuration) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (m->folder_id == 0)
        sqlite3_bind_null(stmt, 1);
    else
        sqlite3_bind_int(stmt, 1, m->folder_id);
    sqlite3_bind_text(stmt, 2, m->name, -1, SQLITE_TRANSIENT);
    if (m->description)
        sqlite3_bind_text(stmt, 3, m->description, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, m->backend, -1, SQLITE_TRANSIENT);
    if (m->base_url)
        sqlite3_bind_text(stmt, 5, m->base_url, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 5);
    sqlite3_bind_text(stmt, 6, m->model_identifier, -1, SQLITE_TRANSIENT);
    if (m->configuration)
        sqlite3_bind_text(stmt, 7, m->configuration, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 7);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;

    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_model_update(db_t *db, const model_t *m) {
    if (!db || !m) return ACTA_DB_ERR_INVALID;
    if (!m->name || !m->backend || !m->model_identifier)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE models SET folder_id=?, name=?, description=?, backend=?, "
        "base_url=?, model_identifier=?, configuration=?, "
        "updated_at=datetime('now') "
        "WHERE id=? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (m->folder_id == 0) sqlite3_bind_null(stmt, 1);
    else                  sqlite3_bind_int(stmt, 1, m->folder_id);
    sqlite3_bind_text(stmt, 2, m->name, -1, SQLITE_TRANSIENT);
    if (m->description)
        sqlite3_bind_text(stmt, 3, m->description, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, m->backend, -1, SQLITE_TRANSIENT);
    if (m->base_url)
        sqlite3_bind_text(stmt, 5, m->base_url, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 5);
    sqlite3_bind_text(stmt, 6, m->model_identifier, -1, SQLITE_TRANSIENT);
    if (m->configuration)
        sqlite3_bind_text(stmt, 7, m->configuration, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 7);
    sqlite3_bind_int(stmt, 8, m->id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_model_soft_delete(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE models SET deleted_at = datetime('now'), "
        "updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_model_restore(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE models SET deleted_at = NULL, updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NOT NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changes > 0) return ACTA_DB_OK;

    /* No row updated: either absent or already live. */
    sqlite3_stmt *chk;
    if (sqlite3_prepare_v2(db->handle,
                          "SELECT 1 FROM models WHERE id = ? LIMIT 1;",
                          -1, &chk, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(chk, 1, id);
    int exists = (sqlite3_step(chk) == SQLITE_ROW);
    sqlite3_finalize(chk);

    return exists ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_model_move_to_folder(db_t *db, int model_id, int folder_id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    if (folder_id != 0) {
        sqlite3_stmt *chk;
        if (sqlite3_prepare_v2(db->handle,
            "SELECT 1 FROM model_folders "
            "WHERE id = ? AND deleted_at IS NULL LIMIT 1;",
            -1, &chk, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(chk, 1, folder_id);
        int found = (sqlite3_step(chk) == SQLITE_ROW);
        sqlite3_finalize(chk);
        if (!found) return ACTA_DB_ERR_NOT_FOUND;
    }

    const char *sql =
        "UPDATE models SET folder_id = ?, updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (folder_id == 0)
        sqlite3_bind_null(stmt, 1);
    else
        sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, model_id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

/* ---------- getters ---------- */

model_t *acta_db_model_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, folder_id, name, description, backend, base_url, "
        "model_identifier, configuration, created_at, updated_at, deleted_at "
        "FROM models WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    model_t *result = NULL;
    int found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) {
        int alloc_err = ACTA_DB_OK;
        result = row_to_model(stmt, &alloc_err);
        if (!result && err) *err = alloc_err;
    }
    sqlite3_finalize(stmt);

    if (err && !result)
        *err = (found) ? *err : ACTA_DB_OK;

    return result;
}

model_t *acta_db_model_get_live(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, folder_id, name, description, backend, base_url, "
        "model_identifier, configuration, created_at, updated_at, deleted_at "
        "FROM models WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    model_t *result = NULL;
    int found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) {
        int alloc_err = ACTA_DB_OK;
        result = row_to_model(stmt, &alloc_err);
        if (!result && err) *err = alloc_err;
    }
    sqlite3_finalize(stmt);

    if (err && !result)
        *err = (found) ? *err : ACTA_DB_OK;

    return result;
}

/* ---------- listers ---------- */

model_t **acta_db_model_list_in_folder(db_t *db,
                                       int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (out_count) *out_count = 0;
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql = folder_id == 0
        ? "SELECT id, folder_id, name, description, backend, base_url, "
          "model_identifier, configuration, created_at, updated_at, deleted_at "
          "FROM models WHERE folder_id IS NULL AND deleted_at IS NULL "
          "ORDER BY id LIMIT ? OFFSET ?;"
        : "SELECT id, folder_id, name, description, backend, base_url, "
          "model_identifier, configuration, created_at, updated_at, deleted_at "
          "FROM models WHERE folder_id = ? AND deleted_at IS NULL "
          "ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int p = 1;
    if (folder_id != 0)
        sqlite3_bind_int(stmt, p++, folder_id);
    sqlite3_bind_int(stmt, p++, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, p,   offset);

    int count = 0;
    model_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int alloc_err = ACTA_DB_OK;
        model_t *item = row_to_model(stmt, &alloc_err);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_model_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            if (err) *err = alloc_err;
            return NULL;
        }
        model_t **tmp = list_push(items, &count, item);
        if (!tmp) {
            acta_db_model_free(item);
            for (int i = 0; i < count; i++) acta_db_model_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        items = tmp;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}

model_t **acta_db_model_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (out_count) *out_count = 0;
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, folder_id, name, description, backend, base_url, "
        "model_identifier, configuration, created_at, updated_at, deleted_at "
        "FROM models WHERE deleted_at IS NULL "
        "ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, 2, offset);

    int count = 0;
    model_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int alloc_err = ACTA_DB_OK;
        model_t *item = row_to_model(stmt, &alloc_err);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_model_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            if (err) *err = alloc_err;
            return NULL;
        }
        model_t **tmp = list_push(items, &count, item);
        if (!tmp) {
            acta_db_model_free(item);
            for (int i = 0; i < count; i++) acta_db_model_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        items = tmp;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}

/* ---------- count ---------- */

int acta_db_model_count_in_folder(db_t *db, int folder_id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql = folder_id == 0
        ? "SELECT COUNT(*) FROM models "
          "WHERE folder_id IS NULL AND deleted_at IS NULL;"
        : "SELECT COUNT(*) FROM models "
          "WHERE folder_id = ? AND deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    if (folder_id != 0)
        sqlite3_bind_int(stmt, 1, folder_id);

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = (int)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    if (count < 0) {
        if (err) *err = ACTA_DB_ERR_SQL;
    } else if (err) {
        *err = ACTA_DB_OK;
    }
    return count;
}

int acta_db_model_count_all(db_t *db, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle,
        "SELECT COUNT(*) FROM models WHERE deleted_at IS NULL;",
        -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = (int)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    if (count < 0) {
        if (err) *err = ACTA_DB_ERR_SQL;
    } else if (err) {
        *err = ACTA_DB_OK;
    }
    return count;
}

/* ---------- free ---------- */

void acta_db_model_free(model_t *m) {
    if (!m) return;
    free(m->name);
    free(m->description);
    free(m->backend);
    free(m->base_url);
    free(m->model_identifier);
    free(m->configuration);
    free(m->created_at);
    free(m->updated_at);
    free(m->deleted_at);
    free(m);
}

void acta_db_model_list_free(model_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_model_free(items[i]);
    free(items);
}
