#include "skill_folder.h"
#include "internal.h"
#include "db.h"

#include <stdio.h>

static skill_folder_t *row_to_skill_folder(sqlite3_stmt *stmt) {
    skill_folder_t *f = calloc(1, sizeof(skill_folder_t));
    if (!f) return NULL;
    f->id         = db_col_int(stmt, 0);
    f->name       = db_col_text(stmt, 1);
    f->parent_id  = db_col_int_or_zero(stmt, 2);
    f->created_at = db_col_text(stmt, 3);
    f->updated_at = db_col_text(stmt, 4);
    f->deleted_at = db_col_text(stmt, 5);
    return f;
}


/* ---------- skill_folder: action functions ---------- */

int acta_db_skill_folder_create(db_t *db, const char *name, int parent_id, int *out_id) {
    if (!db || !name || !out_id) return ACTA_DB_ERR_INVALID;
    const char *sql = "INSERT INTO skill_folders (name, parent_id) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    if (parent_id == 0) sqlite3_bind_null(stmt, 2);
    else sqlite3_bind_int(stmt, 2, parent_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_skill_folder_rename(db_t *db, int id, const char *new_name) {
    if (!db || !new_name) return ACTA_DB_ERR_INVALID;
    const char *sql =
        "UPDATE skill_folders SET name = ?, updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_skill_folder_soft_delete(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Reject if folder has live children */
    sqlite3_stmt *check;
    const char *check_sql =
        "SELECT COUNT(*) FROM skill_folders WHERE parent_id = ? AND deleted_at IS NULL;";
    if (sqlite3_prepare_v2(db->handle, check_sql, -1, &check, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(check, 1, id);
    int child_count = 0;
    if (sqlite3_step(check) == SQLITE_ROW)
        child_count = sqlite3_column_int(check, 0);
    sqlite3_finalize(check);

    if (child_count > 0) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE skill_folders SET deleted_at = datetime('now'), updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

/* ---------- skill_folder: getter ---------- */

skill_folder_t *acta_db_skill_folder_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
        " FROM skill_folders WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    skill_folder_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_skill_folder(stmt);
        if (!result) {
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
    }
    sqlite3_finalize(stmt);

    /* success or not-found */
    if (err) *err = ACTA_DB_OK;
    return result;   /* valid ptr or NULL (not-found) */
}

/* ---------- skill_folder: listers (paginated) ---------- */

skill_folder_t **acta_db_skill_folder_list_children(db_t *db, int parent_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;
    if (offset < 0) offset = 0;

    const char *base = parent_id == 0
        ? "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
          " FROM skill_folders WHERE parent_id IS NULL AND deleted_at IS NULL ORDER BY name"
        : "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
          " FROM skill_folders WHERE parent_id = ? AND deleted_at IS NULL ORDER BY name";

    char sql_buf[512];
    int  bind_idx = (parent_id != 0) ? 1 : 0;

    if (limit > 0) {
        snprintf(sql_buf, sizeof(sql_buf), "%s LIMIT ? OFFSET ?;", base);
    } else {
        snprintf(sql_buf, sizeof(sql_buf), "%s;", base);
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql_buf, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    if (parent_id != 0) sqlite3_bind_int(stmt, 1, parent_id);
    if (limit > 0) {
        sqlite3_bind_int(stmt, ++bind_idx, limit);
        sqlite3_bind_int(stmt, ++bind_idx, offset);
    }

    int count = 0;
    skill_folder_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        skill_folder_t *item = row_to_skill_folder(stmt);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_skill_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        skill_folder_t **tmp = realloc(items, sizeof(skill_folder_t *) * (count + 1));
        if (!tmp) {
            acta_db_skill_folder_free(item);
            for (int i = 0; i < count; i++) acta_db_skill_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;

    if (count == 0) { free(items); return NULL; }
    return items;
}

skill_folder_t **acta_db_skill_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;
    if (offset < 0) offset = 0;

    const char *base =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
        " FROM skill_folders WHERE deleted_at IS NULL ORDER BY name";

    char sql_buf[512];
    if (limit > 0) {
        snprintf(sql_buf, sizeof(sql_buf), "%s LIMIT ? OFFSET ?;", base);
    } else {
        snprintf(sql_buf, sizeof(sql_buf), "%s;", base);
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql_buf, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    if (limit > 0) {
        sqlite3_bind_int(stmt, 1, limit);
        sqlite3_bind_int(stmt, 2, offset);
    }

    int count = 0;
    skill_folder_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        skill_folder_t *item = row_to_skill_folder(stmt);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_skill_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        skill_folder_t **tmp = realloc(items, sizeof(skill_folder_t *) * (count + 1));
        if (!tmp) {
            acta_db_skill_folder_free(item);
            for (int i = 0; i < count; i++) acta_db_skill_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;

    if (count == 0) { free(items); return NULL; }
    return items;
}

/* ---------- skill_folder: free ---------- */

void acta_db_skill_folder_free(skill_folder_t *f) {
    if (!f) return;
    free(f->name);
    free(f->created_at);
    free(f->updated_at);
    free(f->deleted_at);
    free(f);
}

void acta_db_skill_folder_list_free(skill_folder_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_skill_folder_free(items[i]);
    free(items);
}
