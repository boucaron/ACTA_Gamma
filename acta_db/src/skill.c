/* skill.c */
#include <stdio.h>
#include "internal.h"
#include "skill.h"
#include "db.h"

#include <stdlib.h>


/* ═══════════════════════════════════════════════════════════════════
 *  Row helpers
 * ═══════════════════════════════════════════════════════════════════ */

#define SKILL_COLS "id, folder_id, name, description, prompt_template," \
                  " output_schema, created_at, updated_at, deleted_at"

static skill_t *row_to_skill(sqlite3_stmt *stmt) {
    skill_t *s = calloc(1, sizeof(skill_t));
    if (!s) return NULL;

    int alloc_err = ACTA_DB_OK;
    s->id              = db_col_int(stmt, 0);
    s->folder_id       = db_col_int_or_zero(stmt, 1);
    s->name            = db_col_text(stmt, 2, &alloc_err);
    s->description     = db_col_text(stmt, 3, &alloc_err);
    s->prompt_template = db_col_text(stmt, 4, &alloc_err);
    s->output_schema   = db_col_text(stmt, 5, &alloc_err);
    s->created_at      = db_col_text(stmt, 6, &alloc_err);
    s->updated_at      = db_col_text(stmt, 7, &alloc_err);
    s->deleted_at      = db_col_text(stmt, 8, &alloc_err);

    if (alloc_err) {
        acta_db_skill_free(s);
        return NULL;
    }
    return s;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Shared: grow-and-collect loop
 * ═══════════════════════════════════════════════════════════════════ */

#define SKILL_INIT_CAP 16

static skill_t **collect_skill_rows(sqlite3_stmt *stmt,
                                   int *out_count, int *err)
{
    skill_t **items = NULL;
    int count = 0, cap = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            int new_cap = (cap == 0) ? SKILL_INIT_CAP : cap * 2;
            skill_t **tmp = realloc(items,
                                    sizeof(skill_t *) * (size_t)new_cap);
            if (!tmp) {
                for (int i = 0; i < count; i++)
                    acta_db_skill_free(items[i]);
                free(items);
                sqlite3_finalize(stmt);
                if (err)       *err       = ACTA_DB_ERR_ALLOC;
                if (out_count) *out_count = 0;
                return NULL;
            }
            items = tmp;
            cap   = new_cap;
        }

        skill_t *item = row_to_skill(stmt);
        if (!item) {
            for (int i = 0; i < count; i++)
                acta_db_skill_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err)       *err       = ACTA_DB_ERR_ALLOC;
            if (out_count) *out_count = 0;
            return NULL;
        }
        items[count++] = item;
    }

    sqlite3_finalize(stmt);
    if (out_count) *out_count = count;
    if (err)       *err       = ACTA_DB_OK;
    return items;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Mutators
 *
 *  NOTE: the skill_revision row is NOT inserted here. It is snapshotted
 *  by the `skills_create_initial_revision` / `skills_update_revision`
 *  / `skills_soft_delete_revision` triggers in acta_gui/db/schema.sql,
 *  which fire in the same transaction for every client (C library,
 *  CLI, GUI, raw SQL). Revision rows are immutable — a separate trigger
 *  rejects updates/deletes on them.
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_skill_create(db_t *db, const skill_t *s, int *out_id)
{
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
    if (s->description)   sqlite3_bind_text(stmt, 3, s->description,
                                            -1, SQLITE_TRANSIENT);
    else                  sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, s->prompt_template, -1, SQLITE_TRANSIENT);
    if (s->output_schema) sqlite3_bind_text(stmt, 5, s->output_schema,
                                            -1, SQLITE_TRANSIENT);
    else                  sqlite3_bind_null(stmt, 5);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_CONSTRAINT_UNIQUE)
            return ACTA_DB_ERR_DUPLICATE;
        if (rc == SQLITE_CONSTRAINT_FOREIGNKEY)
            return ACTA_DB_ERR_FK;
        fprintf(stderr,
                "[skill_create DEBUG] folder_id=%d name=%s: "
                "sqlite3_step rc=%d: %s\n",
                s->folder_id, s->name, rc, sqlite3_errmsg(db->handle));
        return ACTA_DB_ERR_SQL;
    }

    if (out_id)
        *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_skill_update(db_t *db, const skill_t *s)
{
    if (!db || !s || s->id <= 0 || !s->name || !s->prompt_template)
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
    if (s->description)   sqlite3_bind_text(stmt, 3, s->description,
                                            -1, SQLITE_TRANSIENT);
    else                  sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, s->prompt_template, -1, SQLITE_TRANSIENT);
    if (s->output_schema) sqlite3_bind_text(stmt, 5, s->output_schema,
                                            -1, SQLITE_TRANSIENT);
    else                  sqlite3_bind_null(stmt, 5);
    sqlite3_bind_int(stmt, 6, s->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_CONSTRAINT_UNIQUE)
            return ACTA_DB_ERR_DUPLICATE;
        if (rc == SQLITE_CONSTRAINT_FOREIGNKEY)
            return ACTA_DB_ERR_FK;
        return ACTA_DB_ERR_SQL;
    }

    if (sqlite3_changes(db->handle) > 0)
        return ACTA_DB_OK;
    /* C-level decision without a SQL error: record the detail for the
     * CLI's error message (KI-7). */
    char msg[192];
    snprintf(msg, sizeof msg,
             "skill %d does not exist or is soft-deleted", s->id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_skill_soft_delete(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE skills SET deleted_at = datetime('now'),"
        "     updated_at = datetime('now')"
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
    if (changed > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "skill %d does not exist or is soft-deleted", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_skill_restore(db_t *db, int id)
{
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

    if (changed > 0)
        return ACTA_DB_OK;

    /* Distinguish "already live" (no-op) from "row absent". */
    sqlite3_stmt *chk;
    if (sqlite3_prepare_v2(db->handle,
                          "SELECT 1 FROM skills WHERE id = ? LIMIT 1;",
                          -1, &chk, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(chk, 1, id);
    int exists = (sqlite3_step(chk) == SQLITE_ROW);
    sqlite3_finalize(chk);
    if (exists) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg, "skill %d does not exist", id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_skill_move_to_folder(db_t *db, int skill_id, int folder_id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    if (folder_id != 0) {
        sqlite3_stmt *chk;
        const char *chk_sql =
            "SELECT 1 FROM skill_folders"
            " WHERE id = ? AND deleted_at IS NULL;";
        if (sqlite3_prepare_v2(db->handle, chk_sql, -1, &chk, NULL)
            != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(chk, 1, folder_id);
        int found = (sqlite3_step(chk) == SQLITE_ROW);
        sqlite3_finalize(chk);
        if (!found) {
            char msg[192];
            snprintf(msg, sizeof msg,
                     "skill_folder %d does not exist or is soft-deleted",
                     folder_id);
            db_set_error(db, msg);
            return ACTA_DB_ERR_NOT_FOUND;
        }
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
    if (changed > 0) return ACTA_DB_OK;
    char msg[192];
    snprintf(msg, sizeof msg,
             "skill %d does not exist or is soft-deleted", skill_id);
    db_set_error(db, msg);
    return ACTA_DB_ERR_NOT_FOUND;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Getters
 * ═══════════════════════════════════════════════════════════════════ */

skill_t *acta_db_skill_get(db_t *db, int id, int *err)
{
    if (!db || id <= 0) {
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

    int    row_found = 0;
    skill_t *result  = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        row_found = 1;
        result    = row_to_skill(stmt);
    }
    sqlite3_finalize(stmt);

    if (err)
        *err = (row_found && !result) ? ACTA_DB_ERR_ALLOC : ACTA_DB_OK;
    return result;
}

skill_t *acta_db_skill_get_live(db_t *db, int id, int *err)
{
    if (!db || id <= 0) {
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

    int    row_found = 0;
    skill_t *result  = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        row_found = 1;
        result    = row_to_skill(stmt);
    }
    sqlite3_finalize(stmt);

    if (err)
        *err = (row_found && !result) ? ACTA_DB_ERR_ALLOC : ACTA_DB_OK;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Listers
 * ═══════════════════════════════════════════════════════════════════ */

skill_t **acta_db_skill_list_in_folder(db_t *db, int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql = (folder_id == 0) ?
        "SELECT " SKILL_COLS " FROM skills"
        " WHERE folder_id IS NULL AND deleted_at IS NULL"
        " ORDER BY id LIMIT ? OFFSET ?;" :
        "SELECT " SKILL_COLS " FROM skills"
        " WHERE folder_id = ? AND deleted_at IS NULL"
        " ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    if (folder_id != 0)
        sqlite3_bind_int(stmt, param++, folder_id);
    sqlite3_bind_int(stmt, param++, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, param++, offset);

    return collect_skill_rows(stmt, out_count, err);
}

skill_t **acta_db_skill_list_in_folder_with_deleted(db_t *db,
                                                    int folder_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql = (folder_id == 0) ?
        "SELECT " SKILL_COLS " FROM skills"
        " WHERE folder_id IS NULL"
        " ORDER BY id LIMIT ? OFFSET ?;" :
        "SELECT " SKILL_COLS " FROM skills"
        " WHERE folder_id = ?"
        " ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    if (folder_id != 0)
        sqlite3_bind_int(stmt, param++, folder_id);
    sqlite3_bind_int(stmt, param++, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, param++, offset);

    return collect_skill_rows(stmt, out_count, err);
}

skill_t **acta_db_skill_list_all_with_deleted(db_t *db,
                                              int offset, int limit,
                                              int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql =
        "SELECT " SKILL_COLS " FROM skills"
        " ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, 2, offset);

    return collect_skill_rows(stmt, out_count, err);
}

skill_t **acta_db_skill_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (out_count) *out_count = 0;

    const char *sql =
        "SELECT " SKILL_COLS " FROM skills"
        " WHERE deleted_at IS NULL"
        " ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, 2, offset);

    return collect_skill_rows(stmt, out_count, err);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Count
 * ═══════════════════════════════════════════════════════════════════ */

int acta_db_skill_count_in_folder(db_t *db, int folder_id, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql = (folder_id == 0) ?
        "SELECT COUNT(*) FROM skills"
        " WHERE folder_id IS NULL AND deleted_at IS NULL;" :
        "SELECT COUNT(*) FROM skills"
        " WHERE folder_id = ? AND deleted_at IS NULL;";

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

int acta_db_skill_count_in_folder_with_deleted(db_t *db, int folder_id,
                                               int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql = (folder_id == 0) ?
        "SELECT COUNT(*) FROM skills WHERE folder_id IS NULL;" :
        "SELECT COUNT(*) FROM skills WHERE folder_id = ?;";

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

int acta_db_skill_count_all_with_deleted(db_t *db, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, "SELECT COUNT(*) FROM skills;",
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

int acta_db_skill_count_all(db_t *db, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle,
        "SELECT COUNT(*) FROM skills WHERE deleted_at IS NULL;",
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

/* ═══════════════════════════════════════════════════════════════════
 *  Free
 * ═══════════════════════════════════════════════════════════════════ */

void acta_db_skill_free(skill_t *s) {
    if (!s) return;
    DB_FREE_STR(s->name);
    DB_FREE_STR(s->description);
    DB_FREE_STR(s->prompt_template);
    DB_FREE_STR(s->output_schema);
    DB_FREE_STR(s->created_at);
    DB_FREE_STR(s->updated_at);
    DB_FREE_STR(s->deleted_at);
    free(s);
}

void acta_db_skill_list_free(skill_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_skill_free(items[i]);
    free(items);
}
