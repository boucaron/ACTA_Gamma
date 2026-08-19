#include "internal.h"
#include "model.h"

/* ---------- helpers ---------- */

static model_t *row_to_model(sqlite3_stmt *stmt) {
    model_t *m = calloc(1, sizeof(model_t));
    if (!m) return NULL;
    m->id               = db_col_int(stmt, 0);
    m->folder_id        = db_col_int_or_zero(stmt, 1);
    m->name             = db_col_text(stmt, 2);
    m->description      = db_col_text(stmt, 3);
    m->backend          = db_col_text(stmt, 4);
    m->base_url         = db_col_text(stmt, 5);
    m->model_identifier = db_col_text(stmt, 6);
    m->configuration    = db_col_text(stmt, 7);
    m->created_at       = db_col_text(stmt, 8);
    m->updated_at       = db_col_text(stmt, 9);
    m->deleted_at       = db_col_text(stmt, 10);
    return m;
}

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
    if (!db || !name || !out_id) return -1;
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO model_folders (name, parent_id) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    if (parent_id == 0) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_int(stmt, 2, parent_id);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return 0;
}

model_folder_t *acta_db_model_folder_get(db_t *db, int id) {
    if (!db) return NULL;
    const char *sql = "SELECT id, name, parent_id, created_at, updated_at, deleted_at FROM model_folders WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, id);

    model_folder_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model_folder(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

int acta_db_model_folder_rename(db_t *db, int id, const char *new_name) {
    if (!db || !new_name) return -1;
    const char *sql = "UPDATE model_folders SET name = ?, updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int acta_db_model_folder_soft_delete(db_t *db, int id) {
    if (!db) return -1;
    const char *sql = "UPDATE model_folders SET deleted_at = datetime('now'), updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

model_folder_t *acta_db_model_folder_list_children(db_t *db, int parent_id, int *out_count) {
    if (!db || !out_count) return NULL;
    const char *sql = parent_id == 0
        ? "SELECT id, name, parent_id, created_at, updated_at, deleted_at FROM model_folders WHERE parent_id IS NULL AND deleted_at IS NULL ORDER BY name;"
        : "SELECT id, name, parent_id, created_at, updated_at, deleted_at FROM model_folders WHERE parent_id = ? AND deleted_at IS NULL ORDER BY name;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    if (parent_id != 0) sqlite3_bind_int(stmt, 1, parent_id);

    int count = 0;
    model_folder_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_folder_t *item = row_to_model_folder(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        model_folder_t *tmp = realloc(items, sizeof(model_folder_t) * (count + 1));
        if (!tmp) { acta_db_model_folder_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

model_folder_t *acta_db_model_folder_list_all(db_t *db, int *out_count) {
    if (!db || !out_count) return NULL;
    const char *sql = "SELECT id, name, parent_id, created_at, updated_at, deleted_at FROM model_folders WHERE deleted_at IS NULL ORDER BY name;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    int count = 0;
    model_folder_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_folder_t *item = row_to_model_folder(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        model_folder_t *tmp = realloc(items, sizeof(model_folder_t) * (count + 1));
        if (!tmp) { acta_db_model_folder_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

void acta_db_model_folder_free(model_folder_t *f) {
    if (!f) return;
    free(f->name);
    free(f->created_at);
    free(f->updated_at);
    free(f->deleted_at);
    free(f);
}

void acta_db_model_folder_list_free(model_folder_t *items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        free(items[i].name);
        free(items[i].created_at);
        free(items[i].updated_at);
        free(items[i].deleted_at);
    }
    free(items);
}


/* ---------- model_t ---------- */

int acta_db_model_create(db_t *db, const model_t *m, int *out_id) {
    if (!db || !m || !m->name || !m->backend || !m->model_identifier || !out_id) return -1;

    const char *sql =
        "INSERT INTO models (folder_id, name, description, backend, base_url, model_identifier, configuration) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    if (m->folder_id == 0) {
        sqlite3_bind_null(stmt, 1);
    } else {
        sqlite3_bind_int(stmt, 1, m->folder_id);
    }
    sqlite3_bind_text(stmt, 2, m->name, -1, SQLITE_TRANSIENT);
    if (m->description) sqlite3_bind_text(stmt, 3, m->description, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, m->backend, -1, SQLITE_TRANSIENT);
    if (m->base_url) sqlite3_bind_text(stmt, 5, m->base_url, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 5);
    sqlite3_bind_text(stmt, 6, m->model_identifier, -1, SQLITE_TRANSIENT);
    if (m->configuration) sqlite3_bind_text(stmt, 7, m->configuration, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 7);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return 0;
}

model_t *acta_db_model_get(db_t *db, int id) {
    if (!db) return NULL;
    const char *sql =
        "SELECT id, folder_id, name, description, backend, base_url, model_identifier, configuration, "
        "created_at, updated_at, deleted_at FROM models WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, id);

    model_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

model_t *acta_db_model_get_live(db_t *db, int id) {
    if (!db) return NULL;
    const char *sql =
        "SELECT id, folder_id, name, description, backend, base_url, model_identifier, configuration, "
        "created_at, updated_at, deleted_at FROM models WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, id);

    model_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

int acta_db_model_update(db_t *db, const model_t *m) {
    if (!db || !m) return -1;
    const char *sql =
        "UPDATE models SET folder_id=?, name=?, description=?, backend=?, base_url=?, "
        "model_identifier=?, configuration=?, updated_at=datetime('now') WHERE id=? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    if (m->folder_id == 0) sqlite3_bind_null(stmt, 1);
    else sqlite3_bind_int(stmt, 1, m->folder_id);
    sqlite3_bind_text(stmt, 2, m->name, -1, SQLITE_TRANSIENT);
    if (m->description) sqlite3_bind_text(stmt, 3, m->description, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, m->backend, -1, SQLITE_TRANSIENT);
    if (m->base_url) sqlite3_bind_text(stmt, 5, m->base_url, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 5);
    sqlite3_bind_text(stmt, 6, m->model_identifier, -1, SQLITE_TRANSIENT);
    if (m->configuration) sqlite3_bind_text(stmt, 7, m->configuration, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_int(stmt, 8, m->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int acta_db_model_soft_delete(db_t *db, int id) {
    if (!db) return -1;
    const char *sql = "UPDATE models SET deleted_at = datetime('now'), updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

model_t *acta_db_model_list_in_folder(db_t *db, int folder_id, int *out_count) {
    if (!db || !out_count) return NULL;
    const char *sql = folder_id == 0
        ? "SELECT id, folder_id, name, description, backend, base_url, model_identifier, configuration, "
          "created_at, updated_at, deleted_at FROM models WHERE folder_id IS NULL AND deleted_at IS NULL ORDER BY name;"
        : "SELECT id, folder_id, name, description, backend, base_url, model_identifier, configuration, "
          "created_at, updated_at, deleted_at FROM models WHERE folder_id = ? AND deleted_at IS NULL ORDER BY name;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    if (folder_id != 0) sqlite3_bind_int(stmt, 1, folder_id);

    int count = 0;
    model_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_t *item = row_to_model(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        model_t *tmp = realloc(items, sizeof(model_t) * (count + 1));
        if (!tmp) { acta_db_model_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

model_t *acta_db_model_list_all(db_t *db, int *out_count) {
    if (!db || !out_count) return NULL;
    const char *sql =
        "SELECT id, folder_id, name, description, backend, base_url, model_identifier, configuration, "
        "created_at, updated_at, deleted_at FROM models WHERE deleted_at IS NULL ORDER BY name;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    int count = 0;
    model_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_t *item = row_to_model(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        model_t *tmp = realloc(items, sizeof(model_t) * (count + 1));
        if (!tmp) { acta_db_model_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

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

void acta_db_model_list_free(model_t *items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        free(items[i].name);
        free(items[i].description);
        free(items[i].backend);
        free(items[i].base_url);
        free(items[i].model_identifier);
        free(items[i].configuration);
        free(items[i].created_at);
        free(items[i].updated_at);
        free(items[i].deleted_at);
    }
    free(items);
}
