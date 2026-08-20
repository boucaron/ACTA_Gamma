/* skill.c */
#include "internal.h"
#include "skill.h"
#include "db.h"

#include <stdio.h>

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

/* ---------- skill: action functions ---------- */

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
    if (!db || !s || !s->name || !s->prompt_template)
        return ACTA_DB_ERR_INVALID;
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
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
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
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
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

/* ---------- skill: listers (paginated) ---------- */

skill_t **acta_db_skill_list_in_folder(db_t *db, int folder_id,
                                        int offset, int limit,
                                        int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;
    if (offset < 0) offset = 0;

    const char *base = folder_id == 0
        ? "SELECT id, folder_id, name, description, prompt_template, output_schema,"
          " created_at, updated_at, deleted_at FROM skills"
          " WHERE folder_id IS NULL AND deleted_at IS NULL ORDER BY name"
        : "SELECT id, folder_id, name, description, prompt_template, output_schema,"
          " created_at, updated_at, deleted_at FROM skills"
          " WHERE folder_id = ? AND deleted_at IS NULL ORDER BY name";

    char sql_buf[512];
    int  bind_idx = (folder_id != 0) ? 1 : 0;

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
    if (folder_id != 0) sqlite3_bind_int(stmt, 1, folder_id);
    if (limit > 0) {
        sqlite3_bind_int(stmt, ++bind_idx, limit);
        sqlite3_bind_int(stmt, ++bind_idx, offset);
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

skill_t **acta_db_skill_list_all(db_t *db,
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
        "SELECT id, folder_id, name, description, prompt_template, output_schema,"
        " created_at, updated_at, deleted_at FROM skills WHERE deleted_at IS NULL ORDER BY name";

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
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_skill_move_to_folder(db_t *db, int skill_id, int folder_id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Validate target folder exists and is live */
    if (folder_id != 0) {
        sqlite3_stmt *check;
        const char *check_sql =
            "SELECT 1 FROM skill_folders WHERE id = ? AND deleted_at IS NULL;";
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
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}
