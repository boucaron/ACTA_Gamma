/* skill.c */
#include "internal.h"
#include "skill.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Row helpers
 * ═══════════════════════════════════════════════════════════════════ */

#define SKILL_COLS "id, folder_id, name, description, prompt_template," \
                  " output_schema, created_at, updated_at, deleted_at"

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

/* ═══════════════════════════════════════════════════════════════════
 *  Mutators
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_skill_create(db_t *db, const skill_t *s, int *out_id) {
    if (!db || !s || !s->name || !s->prompt_template)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO skills (folder_id, name, description,"
        " prompt_template, output_schema)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (s->folder_id == 0) sqlite3_bind_null(stmt, 1);
    else                  sqlite3_bind_int(stmt, 1, s->folder_id);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_TRANSIENT);
    if (s->description) sqlite3_bind_text(stmt, 3, s->description, -1, SQLITE_TRANSIENT);
    else               sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, s->prompt_template, -1, SQLITE_TRANSIENT);
    if (s->output_schema) sqlite3_bind_text(stmt, 5, s->output_schema, -1, SQLITE_TRANSIENT);
    else                 sqlite3_bind_null(stmt, 5);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;

    if (out_id)
        *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_skill_update(db_t *db, const skill_t *s) {
    if (!db || !s || !s->name || !s->prompt_template)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE skills"
        " SET folder_id = ?, name = ?, description = ?,"
        "     prompt_template = ?, output_schema = ?,"
        "     updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (s->folder_id == 0) sqlite3_bind_null(stmt, 1);
    else                  sqlite3_bind_int(stmt, 1, s->folder_id);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_TRANSIENT);
    if (s->description) sqlite3_bind_text(stmt, 3, s->description, -1, SQLITE_TRANSIENT);
    else               sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, s->prompt_template, -1, SQLITE_TRANSIENT);
    if (s->output_schema) sqlite3_bind_text(stmt, 5, s->output_schema, -1, SQLITE_TRANSIENT);
    else                 sqlite3_bind_null(stmt, 5);
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

    /* Validate target folder is live (skip check for root). */
    if (folder_id != 0) {
        sqlite3_stmt *chk;
        const char *chk_sql =
            "SELECT 1 FROM skill_folders WHERE id = ? AND deleted_at IS NULL;";
        if (sqlite3_prepare_v2(db->handle, chk_sql, -1, &chk, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(chk, 1, folder_id);
        int found = (sqlite3_step(chk) == SQLITE_ROW);
        sqlite3_finalize(chk);
        if (!found) return ACTA_DB_ERR_NOT_FOUND;
    }

    const char *sql =
        "UPDATE skills SET folder_id = ?, updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (folder_id == 0) sqlite3_bind_null(stmt, 1);
    else               sqlite3_bind_int(stmt, 1, folder_id);
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

/* ═══════════════════════════════════════════════════════════════════
 *  Getters
 * ═══════════════════════════════════════════════════════════════════ */

skill_t *acta_db_skill_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT " SKILL_COLS " FROM skills WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    skill_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = row_to_skill(stmt);
    sqlite3_finalize(stmt);

    if (!result && (err == ACTA_DB_ERR_ALLOC))
        ; /* unreachable; defensive */
    if (err) *err = ACTA_DB_OK;
    return result;
}

skill_t *acta_db_skill_get_live(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT " SKILL_COLS " FROM skills"
        " WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    skill_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = row_to_skill(stmt);
    sqlite3_finalize(stmt);

    if (err) *err = ACTA_DB_OK;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Listers (paginated)
 * ═══════════════════════════════════════════════════════════════════ */

#define SKILL_INIT_CAP 16

/* Shared helper: run a SELECT, collect rows into a growable array.
 * Returns the array (or NULL if count == 0).
 * On OOM frees everything, sets *err, returns NULL. */
static skill_t **collect_rows(db_t *db, const char *sql,
                             int n_bind_int, int bind_int_vals[],
                             int n_bind_text, const char *bind_text_vals[],
                             int *out_count, int *err)
{
    (void)n_bind_int;
    (void)bind_int_vals;
    (void)n_bind_text;
    (void)bind_text_vals;
    /* This helper is replaced by direct inline code below for clarity.
       Kept as a placeholder for a future shared collector. */
    return NULL;
}

/* ── list_in_folder ─────────────────────────────────────────────── */

skill_t **acta_db_skill_list_in_folder(db_t *db, int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err)
{
    if (out_count) *out_count = 0;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) offset = 0;

    /* Build SQL */
    char sql_buf[512];
    if (limit > 0) {
        if (folder_id == 0)
            snprintf(sql_buf, sizeof(sql_buf),
                     "SELECT " SKILL_COLS " FROM skills"
                     " WHERE folder_id IS NULL AND deleted_at IS NULL"
                     " ORDER BY name LIMIT ? OFFSET ?;");
        else
            snprintf(sql_buf, sizeof(sql_buf),
                     "SELECT " SKILL_COLS " FROM skills"
                     " WHERE folder_id = ? AND deleted_at IS NULL"
                     " ORDER BY name LIMIT ? OFFSET ?;");
    } else {
        if (folder_id == 0)
            snprintf(sql_buf, sizeof(sql_buf),
                     "SELECT " SKILL_COLS " FROM skills"
                     " WHERE folder_id IS NULL AND deleted_at IS NULL"
                     " ORDER BY name;");
        else
            snprintf(sql_buf, sizeof(sql_buf),
                     "SELECT " SKILL_COLS " FROM skills"
                     " WHERE folder_id = ? AND deleted_at IS NULL"
                     " ORDER BY name;");
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql_buf, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    if (folder_id != 0)
        sqlite3_bind_int(stmt, param++, folder_id);
    if (limit > 0) {
        sqlite3_bind_int(stmt, param++, limit);
        sqlite3_bind_int(stmt, param++, offset);
    }

    /* Collect rows (grow-by-2x) */
    skill_t **items = NULL;
    int count = 0, cap = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            int new_cap = (cap == 0) ? SKILL_INIT_CAP : cap * 2;
            skill_t **tmp = realloc(items, sizeof(skill_t *) * new_cap);
            if (!tmp) {
                for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
                free(items);
                sqlite3_finalize(stmt);
                if (err) *err = ACTA_DB_ERR_ALLOC;
                if (out_count) *out_count = 0;
                return NULL;
            }
            items = tmp;
            cap = new_cap;
        }
        skill_t *item = row_to_skill(stmt);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            if (out_count) *out_count = 0;
            return NULL;
        }
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items; /* NULL if count == 0 (never allocated) */
}

/* ── list_all ───────────────────────────────────────────────────── */

skill_t **acta_db_skill_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err)
{
    if (out_count) *out_count = 0;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) offset = 0;

    char sql_buf[512];
    if (limit > 0)
        snprintf(sql_buf, sizeof(sql_buf),
                 "SELECT " SKILL_COLS " FROM skills"
                 " WHERE deleted_at IS NULL"
                 " ORDER BY name LIMIT ? OFFSET ?;");
    else
        snprintf(sql_buf, sizeof(sql_buf),
                 "SELECT " SKILL_COLS " FROM skills"
                 " WHERE deleted_at IS NULL"
                 " ORDER BY name;");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql_buf, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    if (limit > 0) {
        sqlite3_bind_int(stmt, param++, limit);
        sqlite3_bind_int(stmt, param++, offset);
    }

    skill_t **items = NULL;
    int count = 0, cap = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            int new_cap = (cap == 0) ? SKILL_INIT_CAP : cap * 2;
            skill_t **tmp = realloc(items, sizeof(skill_t *) * new_cap);
            if (!tmp) {
                for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
                free(items);
                sqlite3_finalize(stmt);
                if (err) *err = ACTA_DB_ERR_ALLOC;
                if (out_count) *out_count = 0;
                return NULL;
            }
            items = tmp;
            cap = new_cap;
        }
        skill_t *item = row_to_skill(stmt);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_skill_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_ALLOC;
            if (out_count) *out_count = 0;
            return NULL;
        }
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Count
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_skill_count(db_t *db, int folder_id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    /*
     * folder_id semantics (mirrors the listers):
     *   < 0  → all live skills          (matches list_all)
     *   == 0 → root-level only          (matches list_in_folder(db, 0))
     *   > 0  → specific folder          (matches list_in_folder(db, X))
     */

    const char *sql;
    if (folder_id < 0)
        sql = "SELECT COUNT(*) FROM skills WHERE deleted_at IS NULL;";
    else if (folder_id == 0)
        sql = "SELECT COUNT(*) FROM skills"
              " WHERE folder_id IS NULL AND deleted_at IS NULL;";
    else
        sql = "SELECT COUNT(*) FROM skills"
              " WHERE folder_id = ? AND deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    if (folder_id > 0)
        sqlite3_bind_int(stmt, 1, folder_id);

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = db_col_int(stmt, 0);
    else
        if (err) *err = ACTA_DB_ERR_SQL;

    sqlite3_finalize(stmt);
    return count;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Free
 * ═══════════════════════════════════════════════════════════════════ */

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
