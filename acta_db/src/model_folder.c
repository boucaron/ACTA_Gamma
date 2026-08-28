#include "internal.h"
#include "model_folder.h"
#include "db.h"

/* ------------------------------------------------------------------ */
/*  Row decoding                                                      */
/* ------------------------------------------------------------------ */

static model_folder_t *row_to_model_folder(sqlite3_stmt *stmt, int *err)
{
    model_folder_t *f = calloc(1, sizeof *f);
    if (!f) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    f->id         = db_col_int(stmt, 0);
    f->name       = db_col_text(stmt, 1, &alloc_err);
    f->parent_id  = db_col_int_or_zero(stmt, 2);
    f->created_at = db_col_text(stmt, 3, &alloc_err);
    f->updated_at = db_col_text(stmt, 4, &alloc_err);
    f->deleted_at = db_col_text(stmt, 5, &alloc_err);

    if (alloc_err) {
        acta_db_model_folder_free(f);
        if (err) *err = alloc_err;
        return NULL;
    }
    return f;
}

/* ------------------------------------------------------------------ */
/*  Shared step-loop                                                  */
/* ------------------------------------------------------------------ */

static model_folder_t **collect_rows(sqlite3_stmt *stmt,
                                     int *out_count,
                                     int *out_err)
{
    int  count = 0;
    int  cap   = 0;
    model_folder_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int row_err = ACTA_DB_OK;
        model_folder_t *item = row_to_model_folder(stmt, &row_err);
        if (!item) {
            *out_count = 0;
            *out_err   = row_err;
            for (int i = 0; i < count; i++)
                acta_db_model_folder_free(items[i]);
            free(items);
            return NULL;
        }
        if (count == cap) {
            int new_cap = cap ? cap * 2 : 8;
            model_folder_t **tmp =
                realloc(items, sizeof *items * (size_t)new_cap);
            if (!tmp) {
                *out_count = 0;
                *out_err   = ACTA_DB_ERR_ALLOC;
                acta_db_model_folder_free(item);
                for (int i = 0; i < count; i++)
                    acta_db_model_folder_free(items[i]);
                free(items);
                return NULL;
            }
            items = tmp;
            cap   = new_cap;
        }
        items[count++] = item;
    }

    /* Shrink to exact size (best-effort; keep larger buffer on failure). */
    if (count > 0 && count < cap) {
        model_folder_t **tmp =
            realloc(items, sizeof *items * (size_t)count);
        if (tmp) items = tmp;
    }

    *out_count = count;
    *out_err   = ACTA_DB_OK;
    return items;   /* NULL is valid for count == 0 */
}

/* ================================================================== */
/*  Mutators                                                          */
/* ================================================================== */

int acta_db_model_folder_create(db_t *db, const char *name, int parent_id,
                                int *out_id)
{
    if (!db || !name || !*name) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO model_folders (name, parent_id) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    if (parent_id == 0)
        sqlite3_bind_null(stmt, 2);
    else
        sqlite3_bind_int(stmt, 2, parent_id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE && out_id)
        *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_model_folder_rename(db_t *db, int id, const char *new_name)
{
    if (!db || !new_name || !*new_name || id <= 0)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE model_folders SET name = ?, updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_model_folder_soft_delete(db_t *db, int id)
{
    if (!db || id <= 0) return ACTA_DB_ERR_INVALID;

    /* Reject if the folder has live child sub-folders. */
    {
        const char *check_sql =
            "SELECT COUNT(*) FROM model_folders"
            " WHERE parent_id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *check;
        if (sqlite3_prepare_v2(db->handle, check_sql, -1, &check, NULL)
            != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(check, 1, id);

        int child_count = 0;
        if (sqlite3_step(check) == SQLITE_ROW)
            child_count = (int)sqlite3_column_int64(check, 0);
        sqlite3_finalize(check);

        if (child_count > 0) return ACTA_DB_ERR_INVALID;
    }

    /* Reject if live models are assigned to this folder. */
    {
        const char *check_sql =
            "SELECT COUNT(*) FROM models"
            " WHERE folder_id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *check;
        if (sqlite3_prepare_v2(db->handle, check_sql, -1, &check, NULL)
            != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(check, 1, id);

        int model_count = 0;
        if (sqlite3_step(check) == SQLITE_ROW)
            model_count = (int)sqlite3_column_int64(check, 0);
        sqlite3_finalize(check);

        if (model_count > 0) return ACTA_DB_ERR_INVALID;
    }

    const char *sql =
        "UPDATE model_folders"
        " SET deleted_at = datetime('now'), updated_at = datetime('now')"
        " WHERE id = ? AND deleted_at IS NULL;";
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

int acta_db_model_folder_restore(db_t *db, int id)
{
    if (!db || id <= 0) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE model_folders SET deleted_at = NULL, "
        "updated_at = CASE WHEN deleted_at IS NOT NULL "
        "                  THEN datetime('now') ELSE updated_at END "
        "WHERE id = ?;";
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

/* ------------------------------------------------------------------ */
/*  Cycle detection: does `target_id` appear in the ancestor chain    */
/*  starting at `start_id`?                                           */
/* ------------------------------------------------------------------ */

static int ancestor_reaches(db_t *db, int start_id, int target_id)
{
    const char *sql =
        "SELECT parent_id FROM model_folders "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    int cur   = start_id;
    int found = 0;
    int depth = 0;
    const int MAX_DEPTH = 10000;   /* guard against corrupted circular data */

    while (cur > 0 && depth < MAX_DEPTH) {
        if (cur == target_id) { found = 1; break; }
        sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, cur);
        if (sqlite3_step(stmt) != SQLITE_ROW) break;
        cur = db_col_int_or_zero(stmt, 0);
        depth++;
    }
    sqlite3_finalize(stmt);
    return found;
}

int acta_db_model_folder_move_to(db_t *db, int folder_id, int new_parent_id)
{
    if (!db || folder_id <= 0) return ACTA_DB_ERR_INVALID;

    /* 1. Source folder must exist and be live. */
    int cur_parent;
    {
        const char *sql =
            "SELECT parent_id FROM model_folders "
            "WHERE id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, folder_id);

        cur_parent = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            cur_parent = db_col_int_or_zero(stmt, 0);
        sqlite3_finalize(stmt);

        if (cur_parent < 0) return ACTA_DB_ERR_NOT_FOUND;
        if (cur_parent == new_parent_id) return ACTA_DB_OK;  /* no-op */
    }

    /* 2. Target parent must exist and be live (if not root). */
    if (new_parent_id > 0) {
        const char *sql =
            "SELECT 1 FROM model_folders "
            "WHERE id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, new_parent_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_ROW) return ACTA_DB_ERR_NOT_FOUND;
    }

    /* 3. Cycle check. */
    if (new_parent_id > 0) {
        int cycle = ancestor_reaches(db, new_parent_id, folder_id);
        if (cycle < 0) return ACTA_DB_ERR_SQL;
        if (cycle > 0) return ACTA_DB_ERR_INVALID;
    }

    /* 4. Update. */
    {
        const char *sql =
            "UPDATE model_folders SET parent_id = ?, "
            "updated_at = datetime('now') "
            "WHERE id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        if (new_parent_id == 0)
            sqlite3_bind_null(stmt, 1);
        else
            sqlite3_bind_int(stmt, 1, new_parent_id);
        sqlite3_bind_int(stmt, 2, folder_id);

        int rc = sqlite3_step(stmt);
        int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
        return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
    }
}

/* ================================================================== */
/*  Getters                                                           */
/* ================================================================== */

model_folder_t *acta_db_model_folder_get(db_t *db, int id, int *err)
{
    if (!db || id <= 0) {
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
        int decode_err = ACTA_DB_OK;
        result = row_to_model_folder(stmt, &decode_err);
        if (err) *err = result ? ACTA_DB_OK : decode_err;
    } else {
        if (err) *err = ACTA_DB_OK;   /* not found – not an error */
    }
    sqlite3_finalize(stmt);

    return result;
}

/* ================================================================== */
/*  Listers                                                           */
/* ================================================================== */

model_folder_t **acta_db_model_folder_list_children(
    db_t *db, int parent_id,
    int offset, int limit,
    int *out_count, int *err)
{
    if (!db || offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql = (parent_id == 0)
        ? "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
          "FROM model_folders "
          "WHERE parent_id IS NULL AND deleted_at IS NULL "
          "ORDER BY id LIMIT ? OFFSET ?;"
        : "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
          "FROM model_folders "
          "WHERE parent_id = ? AND deleted_at IS NULL "
          "ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int param = 1;
    if (parent_id != 0)
        sqlite3_bind_int(stmt, param++, parent_id);
    sqlite3_bind_int(stmt, param++, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, param,    offset);

    int count = 0;
    int c_err = 0;
    model_folder_t **items = collect_rows(stmt, &count, &c_err);
    sqlite3_finalize(stmt);

    if (out_count) *out_count = (c_err == ACTA_DB_OK) ? count : 0;
    if (err)       *err       = c_err;
    return (c_err == ACTA_DB_OK) ? items : NULL;
}

model_folder_t **acta_db_model_folder_list_all(
    db_t *db, int offset, int limit,
    int *out_count, int *err)
{
    if (!db || offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT id, name, parent_id, created_at, updated_at, deleted_at "
        "FROM model_folders "
        "WHERE deleted_at IS NULL "
        "ORDER BY id LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, 2, offset);

    int count = 0;
    int c_err = 0;
    model_folder_t **items = collect_rows(stmt, &count, &c_err);
    sqlite3_finalize(stmt);

    if (out_count) *out_count = (c_err == ACTA_DB_OK) ? count : 0;
    if (err)       *err       = c_err;
    return (c_err == ACTA_DB_OK) ? items : NULL;
}

/* ================================================================== */
/*  Counts                                                            */
/* ================================================================== */

int acta_db_model_folder_count_children(db_t *db, int parent_id, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql = (parent_id == 0)
        ? "SELECT COUNT(*) FROM model_folders "
          "WHERE parent_id IS NULL AND deleted_at IS NULL;"
        : "SELECT COUNT(*) FROM model_folders "
          "WHERE parent_id = ? AND deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    if (parent_id != 0)
        sqlite3_bind_int(stmt, 1, parent_id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int count = (int)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    if (err) *err = ACTA_DB_OK;
    return count;
}

int acta_db_model_folder_count_all(db_t *db, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql =
        "SELECT COUNT(*) FROM model_folders WHERE deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int count = (int)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    if (err) *err = ACTA_DB_OK;
    return count;
}

/* ================================================================== */
/*  Free                                                              */
/* ================================================================== */

void acta_db_model_folder_free(model_folder_t *f)
{
    if (!f) return;
    DB_FREE_STR(f->name);
    DB_FREE_STR(f->created_at);
    DB_FREE_STR(f->updated_at);
    DB_FREE_STR(f->deleted_at);
    free(f);
}

void acta_db_model_folder_list_free(model_folder_t **items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_model_folder_free(items[i]);
    free(items);
}
