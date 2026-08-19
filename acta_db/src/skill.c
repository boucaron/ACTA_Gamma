/* skill.c */
#include "internal.h"
#include "skill.h"
#include "db.h"

/* ---------- row helpers ---------- */

static skill_t *row_to_skill(sqlite3_stmt *stmt) {
    skill_t *s = calloc(1, sizeof(skill_t));
    if (!s) return NULL;
    s->id              = db_col_int(stmt, 0);
    s->folder_id       = db_col_int_or_zero(stmt, 1);
    s->name            = db_col_text(stmt, 2);
    s->description     = db_col_text(stmt, 3);
    s->prompt_template = db_col_text(stmt, 4);
    s->output_schema   = db_col_text(stmt, 5);
    s->created_at      = db_col_text(stmt, 6);
    s->updated_at      = db_col_text(stmt, 7);
    s->deleted_at      = db_col_text(stmt, 8);
    return s;
}

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

/* ---------- skill_folder: action functions (unchanged signatures) ---------- */

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
        "UPDATE skill_folders SET name = ?, updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_skill_folder_soft_delete(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Reject if folder has live children */
    sqlite3_stmt *check;
    const char *check_sql = "SELECT COUNT(*) FROM skill_folders WHERE parent_id = ? AND deleted_at IS NULL;";
    if (sqlite3_prepare_v2(db->handle, check_sql, -1, &check, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(check, 1, id);
    int child_count = 0;
    if (sqlite3_step(check) == SQLITE_ROW)
        child_count = sqlite3_column_int(check, 0);
    sqlite3_finalize(check);

    if (child_count > 0) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE skill_folders SET deleted_at = datetime('now'), updated_at = datetime('now') WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

/* ---------- skill_folder: getter ---------- */

skill_folder_t *acta_db_skill_folder_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at FROM skill_folders WHERE id = ?;";
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

/* ---------- skill_folder: listers ---------- */

skill_folder_t **acta_db_skill_folder_list_children(db_t *db, int parent_id, int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql = parent_id == 0
        ? "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
          " FROM skill_folders WHERE parent_id IS NULL AND deleted_at IS NULL ORDER BY name;"
        : "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
          " FROM skill_folders WHERE parent_id = ? AND deleted_at IS NULL ORDER BY name;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    if (parent_id != 0) sqlite3_bind_int(stmt, 1, parent_id);

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

    if (count == 0) { free(items); return NULL; }  /* NULL + OK = empty list */
    return items;
}

skill_folder_t **acta_db_skill_folder_list_all(db_t *db, int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
        " FROM skill_folders WHERE deleted_at IS NULL ORDER BY name;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
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

/* ---------- skill: action functions (unchanged signatures) ---------- */

int acta_db_skill_create(db_t *db, const skill_t *s, int *out_id) {
    if (!db || !s || !s->name || !s->prompt_template || !out_id)
        return ACTA_DB_ERR_INVALID;
    const char *sql =
        "INSERT INTO skills (folder_id, name, description, prompt_template, output_schema)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (s->folder_id == 0) sqlite3_bind_null(stmt, 1);
    else sqlite3_bind_int(stmt, 1, s->folder_id);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_TRANSIENT);
    if (s->description) sqlite3_bind_text(stmt, 3, s->description, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, s->prompt_template, -1, SQLITE_TRANSIENT);
    if (s->output_schema) sqlite3_bind_text(stmt, 5, s->output_schema, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 5);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_skill_update(db_t *db, const skill_t *s) {
    if (!db || !s) return ACTA_DB_ERR_INVALID;
    const char *sql =
        "UPDATE skills SET folder_id=?, name=?, description=?, prompt_template=?,"
        " output_schema=?, updated_at=datetime('now') WHERE id=? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (s->folder_id == 0) sqlite3_bind_null(stmt, 1);
    else sqlite3_bind_int(stmt, 1, s->folder_id);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_TRANSIENT);
    if (s->description) sqlite3_bind_text(stmt, 3, s->description, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, s->prompt_template, -1, SQLITE_TRANSIENT);
    if (s->output_schema) sqlite3_bind_text(stmt, 5, s->output_schema, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 5);
    sqlite3_bind_int(stmt, 6, s->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_skill_soft_delete(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;
    const char *sql =
        "UPDATE skills SET deleted_at = datetime('now'), updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

/* ---------- skill: getters ---------- */

skill_t *acta_db_skill_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, folder_id, name, description, prompt_template, output_schema,"
        " created_at, updated_at, deleted_at FROM skills WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    skill_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_skill(stmt);
        if (!result) {
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
    }
    sqlite3_finalize(stmt);

    if (err) *err = ACTA_DB_OK;
    return result;
}

skill_t *acta_db_skill_get_live(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, folder_id, name, description, prompt_template, output_schema,"
        " created_at, updated_at, deleted_at FROM skills WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    skill_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_skill(stmt);
        if (!result) {
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
    }
    sqlite3_finalize(stmt);

    if (err) *err = ACTA_DB_OK;
    return result;
}

/* ---------- skill: listers ---------- */

skill_t **acta_db_skill_list_in_folder(db_t *db, int folder_id, int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql = folder_id == 0
        ? "SELECT id, folder_id, name, description, prompt_template, output_schema,"
          " created_at, updated_at, deleted_at FROM skills"
          " WHERE folder_id IS NULL AND deleted_at IS NULL ORDER BY name;"
        : "SELECT id, folder_id, name, description, prompt_template, output_schema,"
          " created_at, updated_at, deleted_at FROM skills"
          " WHERE folder_id = ? AND deleted_at IS NULL ORDER BY name;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    if (folder_id != 0) sqlite3_bind_int(stmt, 1, folder_id);

    int count = 0;
    skill_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        skill_t *item = row_to_skill(stmt);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        skill_t **tmp = realloc(items, sizeof(skill_t *) * (count + 1));
        if (!tmp) {
            acta_db_skill_free(item);
            for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
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

skill_t **acta_db_skill_list_all(db_t *db, int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql =
        "SELECT id, folder_id, name, description, prompt_template, output_schema,"
        " created_at, updated_at, deleted_at FROM skills WHERE deleted_at IS NULL ORDER BY name;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int count = 0;
    skill_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        skill_t *item = row_to_skill(stmt);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            return NULL;
        }
        skill_t **tmp = realloc(items, sizeof(skill_t *) * (count + 1));
        if (!tmp) {
            acta_db_skill_free(item);
            for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
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

/* ---------- skill: free ---------- */

void acta_db_skill_free(skill_t *s) {
    if (!s) return;
    free(s->name);
    free(s->description);
    free(s->prompt_template);
    free(s->output_schema);
    free(s->created_at);
    free(s->updated_at);
    free(s->deleted_at);
    free(s);
}

void acta_db_skill_list_free(skill_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_skill_free(items[i]);
    free(items);
}

/* ---------- skill: remaining action functions ---------- */

int acta_db_skill_restore(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;
    const char *sql =
        "UPDATE skills SET deleted_at = NULL, updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NOT NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_skill_move_to_folder(db_t *db, int skill_id, int folder_id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Validate target folder exists and is live */
    if (folder_id != 0) {
        sqlite3_stmt *check;
        const char *check_sql = "SELECT 1 FROM skill_folders WHERE id = ? AND deleted_at IS NULL;";
        if (sqlite3_prepare_v2(db->handle, check_sql, -1, &check, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(check, 1, folder_id);
        int found = (sqlite3_step(check) == SQLITE_ROW);
        sqlite3_finalize(check);
        if (!found) return ACTA_DB_ERR_NOT_FOUND;
    }

    const char *sql =
        "UPDATE skills SET folder_id = ?, updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (folder_id == 0) sqlite3_bind_null(stmt, 1);
    else sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, skill_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return sqlite3_changes(db->handle) > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}
