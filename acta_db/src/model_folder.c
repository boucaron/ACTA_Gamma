#include "internal.h"
#include "model_folder.h"
#include "db.h"


static model_folder_t *row_to_model_folder(sqlite3_stmt *stmt) {
    model_folder_t *f = calloc(1, sizeof(model_folder_t));
    if (!f) return NULL;
    f->id         = db_col_int(stmt, 0);
    f->name       = db_col_text(stmt, 1);
    f->parent_id  = db_col_int_or_zero(stmt, 2);
    f->created_at = db_col_text(stmt, 3);
    f->updated_at = db_col_text(stmt, 4);
    f->deleted_at = db_col_text(stmt, 5);
    return f;
}


/* ---------- model_folder ---------- */

int acta_db_model_folder_create(db_t *db, const char *name, int parent_id, int *out_id) {
    if (!db || !name || !out_id) return ACTA_DB_ERR_INVALID;
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO model_folders (name, parent_id) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    if (parent_id == 0) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_int(stmt, 2, parent_id);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

model_folder_t *acta_db_model_folder_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    const char *sql =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
        "FROM model_folders WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    model_folder_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model_folder(stmt);
        if (!result && err) *err = ACTA_DB_ERR_ALLOC;
    }
    sqlite3_finalize(stmt);
    if (err && !result) *err = ACTA_DB_OK;   /* not-found is still "ok" */
    return result;
}

int acta_db_model_folder_rename(db_t *db, int id, const char *new_name) {
    if (!db || !new_name) return ACTA_DB_ERR_INVALID;
    const char *sql = "UPDATE model_folders SET name = ?, updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return ACTA_DB_ERR_SQL;
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_model_folder_soft_delete(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;
    const char *sql = "UPDATE model_folders SET deleted_at = datetime('now'), updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}


void acta_db_model_folder_free(model_folder_t *f) {
    if (!f) return;
    free(f->name);
    free(f->created_at);
    free(f->updated_at);
    free(f->deleted_at);
    free(f);
}

void acta_db_model_folder_list_free(model_folder_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        acta_db_model_folder_free(items[i]);
    }
    free(items);
}model_folder_t **acta_db_model_folder_list_children(db_t *db, int parent_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err) {
    if (!db || !out_count) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql = parent_id == 0
        ? "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
          "FROM model_folders WHERE parent_id IS NULL AND deleted_at IS NULL "
          "ORDER BY name LIMIT ? OFFSET ?;"
        : "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
          "FROM model_folders WHERE parent_id = ? AND deleted_at IS NULL "
          "ORDER BY name LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    if (parent_id != 0) {
        sqlite3_bind_int(stmt, param++, parent_id);
    }
    sqlite3_bind_int(stmt, param++, limit > 0 ? limit : -1);  /* -1 = no limit */
    sqlite3_bind_int(stmt, param,   offset > 0 ? offset : 0);

    int count = 0;
    model_folder_t **items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_folder_t *item = row_to_model_folder(stmt);
        if (!item) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            for (int i = 0; i < count; i++) acta_db_model_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            return NULL;
        }
        model_folder_t **tmp = realloc(items, sizeof(model_folder_t *) * (count + 1));
        if (!tmp) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            acta_db_model_folder_free(item);
            for (int i = 0; i < count; i++) acta_db_model_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}

model_folder_t **acta_db_model_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count, int *err) {
    if (!db || !out_count) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
        "FROM model_folders WHERE deleted_at IS NULL "
        "ORDER BY name LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, 2, offset > 0 ? offset : 0);

    int count = 0;
    model_folder_t **items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_folder_t *item = row_to_model_folder(stmt);
        if (!item) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            for (int i = 0; i < count; i++) acta_db_model_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            return NULL;
        }
        model_folder_t **tmp = realloc(items, sizeof(model_folder_t *) * (count + 1));
        if (!tmp) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            acta_db_model_folder_free(item);
            for (int i = 0; i < count; i++) acta_db_model_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}
