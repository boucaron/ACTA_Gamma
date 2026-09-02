#include "skill_folder.h"
#include "internal.h"
#include "db.h"

#include <stdio.h>

/* ================================================================
 *  Row mapping
 * ================================================================ */

static skill_folder_t *row_to_skill_folder(sqlite3_stmt *stmt, int *alloc_err)
{
    skill_folder_t *f = calloc(1, sizeof(skill_folder_t));
    if (!f) {
        if (alloc_err) *alloc_err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }
    f->id         = db_col_int(stmt, 0);
    f->name       = db_col_text(stmt, 1, alloc_err);
    f->parent_id  = db_col_int_or_zero(stmt, 2);
    f->created_at = db_col_text(stmt, 3, alloc_err);
    f->updated_at = db_col_text(stmt, 4, alloc_err);
    f->deleted_at = db_col_text(stmt, 5, alloc_err);
    return f;
}

/* ================================================================
 *  Shared lister helper
 * ================================================================ */

static skill_folder_t **collect_rows(sqlite3_stmt *stmt,
                                     int *out_count, int *err)
{
    int  count = 0;
    size_t cap = 0;
    skill_folder_t **items = NULL;

    while (1) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            /* Mid-query error (I/O, etc.) – free what we have. */
            for (int i = 0; i < count; i++)
                acta_db_skill_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = ACTA_DB_ERR_SQL;
            return NULL;
        }

        int alloc_err = ACTA_DB_OK;
        skill_folder_t *item = row_to_skill_folder(stmt, &alloc_err);
        if (!item || alloc_err) {
            if (item) acta_db_skill_folder_free(item);
            for (int i = 0; i < count; i++)
                acta_db_skill_folder_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (err) *err = alloc_err ? alloc_err : ACTA_DB_ERR_ALLOC;
            return NULL;
        }

        /* Grow the pointer array. */
        if (count >= (int)cap) {
            size_t new_cap = cap ? cap * 2 : 16;
            skill_folder_t **tmp = realloc(items,
                                           new_cap * sizeof(skill_folder_t *));
            if (!tmp) {
                acta_db_skill_folder_free(item);
                for (int i = 0; i < count; i++)
                    acta_db_skill_folder_free(items[i]);
                free(items);
                sqlite3_finalize(stmt);
                if (err) *err = ACTA_DB_ERR_ALLOC;
                return NULL;
            }
            items = tmp;
            cap = new_cap;
        }
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;

    if (count == 0) { free(items); return NULL; }  /* items == NULL */
    return items;
}

/* ================================================================
 *  Prepare helper
 * ================================================================ */

/*
 * Builds and prepares the SELECT for the folder listers
 * (list_children / list_all / list_all_with_deleted).
 *
 * The SQL is at most ~140 characters; a 256-byte buffer is sufficient
 * and the snprintf truncation check makes the bound explicit.
 */
static sqlite3_stmt *prepare_folder_query(db_t *db,
                                          int parent_id,
                                          int has_parent_filter,
                                          int include_deleted,
                                          int offset,
                                          int limit,
                                          int *err)
{
    char sql[256];
    int  len;

    const char *where;
    if (has_parent_filter && parent_id == 0)
        where = include_deleted
                    ? " WHERE parent_id IS NULL"
                    : " WHERE parent_id IS NULL AND deleted_at IS NULL";
    else if (has_parent_filter)
        where = include_deleted
                    ? " WHERE parent_id = ?"
                    : " WHERE parent_id = ? AND deleted_at IS NULL";
    else
        where = include_deleted ? "" : " WHERE deleted_at IS NULL";

    len = snprintf(sql, sizeof(sql),
                   "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
                   " FROM skill_folders%s ORDER BY id", where);
    if (len < 0 || (size_t)len >= sizeof(sql)) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    if (limit > 0 || offset > 0) {
        int n = snprintf(sql + len, sizeof(sql) - (size_t)len,
                        " LIMIT ? OFFSET ?");
        if (n < 0 || len + n >= (int)sizeof(sql)) {
            if (err) *err = ACTA_DB_ERR_SQL;
            return NULL;
        }
        len += n;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int idx = 1;
    if (has_parent_filter && parent_id != 0)
        sqlite3_bind_int(stmt, idx++, parent_id);
    if (limit > 0 || offset > 0) {
        sqlite3_bind_int(stmt, idx++, limit > 0 ? limit : -1);
        sqlite3_bind_int(stmt, idx++, offset);
    }
    return stmt;
}


/* ================================================================
 *  Mutators
 * ================================================================ */

int acta_db_skill_folder_create(db_t *db, const char *name,
                               int parent_id, int *out_id)
{
    if (!db || !name || !*name) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO skill_folders (name, parent_id) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    if (parent_id == 0)
        sqlite3_bind_null(stmt, 2);
    else
        sqlite3_bind_int(stmt, 2, parent_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_CONSTRAINT_UNIQUE)
            return ACTA_DB_ERR_DUPLICATE;
        if (rc == SQLITE_CONSTRAINT_FOREIGNKEY)
            return ACTA_DB_ERR_FK;
        return ACTA_DB_ERR_SQL;
    }

    if (out_id)
        *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_skill_folder_rename(db_t *db, int id, const char *new_name)
{
    if (!db || id <= 0 || !new_name || !*new_name)
        return ACTA_DB_ERR_INVALID;

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
        if (rc == SQLITE_CONSTRAINT_UNIQUE)
            return ACTA_DB_ERR_DUPLICATE;
        if (rc == SQLITE_CONSTRAINT_FOREIGNKEY)
            return ACTA_DB_ERR_FK;
        return ACTA_DB_ERR_SQL;
    }
    int changed = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changed > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_skill_folder_move_to(db_t *db, int id, int new_parent_id)
{
    if (!db || id <= 0) return ACTA_DB_ERR_INVALID;

    /* 1 – folder must exist and be live */
    {
        const char *sql =
            "SELECT 1 FROM skill_folders WHERE id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_ROW) return ACTA_DB_ERR_NOT_FOUND;
    }

    if (new_parent_id != 0) {
        /* 2 – target parent must exist and be live */
        {
            const char *sql =
                "SELECT 1 FROM skill_folders"
                " WHERE id = ? AND deleted_at IS NULL;";
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
                return ACTA_DB_ERR_SQL;
            sqlite3_bind_int(stmt, 1, new_parent_id);
            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_ROW) return ACTA_DB_ERR_NOT_FOUND;
        }

        /* 3 – cycle detection: walk up from new_parent_id;
         *    if we reach `id`, the move would create a cycle.
         *    Statement is prepared once and reused each iteration. */
        {
            const char *sql =
                "SELECT parent_id FROM skill_folders WHERE id = ?;";
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
                return ACTA_DB_ERR_SQL;

            int cursor = new_parent_id;
            for (int depth = 0; depth < 4096; depth++) {
                if (cursor == id) {
                    sqlite3_finalize(stmt);
                    return ACTA_DB_ERR_INVALID;
                }
                sqlite3_reset(stmt);
                sqlite3_bind_int(stmt, 1, cursor);
                if (sqlite3_step(stmt) != SQLITE_ROW) break;
                if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) break;
                cursor = (int)sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
            /* depth == 4096 without reaching root or `id`:
             * treat as "no cycle detected" (pathological case). */
        }
    }

    /* 4 – perform the move */
    {
        const char *sql =
            "UPDATE skill_folders"
            " SET parent_id = ?, updated_at = datetime('now')"
            " WHERE id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        if (new_parent_id == 0)
            sqlite3_bind_null(stmt, 1);
        else
            sqlite3_bind_int(stmt, 1, new_parent_id);
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
}

int acta_db_skill_folder_soft_delete(db_t *db, int id)
{
    if (!db || id <= 0) return ACTA_DB_ERR_INVALID;

    /* Reject if live sub-folders exist */
    {
        const char *sql =
            "SELECT COUNT(*) FROM skill_folders"
            " WHERE parent_id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, id);
        int child_count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            child_count = (int)sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        if (child_count > 0) return ACTA_DB_ERR_INVALID;
    }

    /* Reject if live skills are assigned to this folder */
    {
        const char *sql =
            "SELECT COUNT(*) FROM skills"
            " WHERE folder_id = ? AND deleted_at IS NULL;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(stmt, 1, id);
        int skill_count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            skill_count = (int)sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        if (skill_count > 0) return ACTA_DB_ERR_INVALID;
    }

    /* Soft-delete */
    {
        const char *sql =
            "UPDATE skill_folders"
            " SET deleted_at = datetime('now'), updated_at = datetime('now')"
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
}

int acta_db_skill_folder_restore(db_t *db, int id)
{
    if (!db || id <= 0) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE skill_folders"
        " SET deleted_at = NULL,"
        "     updated_at = CASE WHEN deleted_at IS NOT NULL"
        "                       THEN datetime('now') ELSE updated_at END"
        " WHERE id = ?;";
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

/* ================================================================
 *  Getter
 * ================================================================ */

skill_folder_t *acta_db_skill_folder_get(db_t *db, int id, int *err)
{
    if (!db || id <= 0) {
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
        int alloc_err = ACTA_DB_OK;
        result = row_to_skill_folder(stmt, &alloc_err);
        if (!result && !alloc_err)
            alloc_err = ACTA_DB_ERR_ALLOC;   /* calloc failed */
        if (alloc_err) {
            if (result) acta_db_skill_folder_free(result);
            sqlite3_finalize(stmt);
            if (err) *err = alloc_err;
            return NULL;
        }
    }
    sqlite3_finalize(stmt);

    if (err) *err = ACTA_DB_OK;
    return result;
}

/* ================================================================
 *  Listers (paginated)
 * ================================================================ */

skill_folder_t **acta_db_skill_folder_list_children(
    db_t *db, int parent_id,
    int offset, int limit,
    int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    sqlite3_stmt *stmt = prepare_folder_query(db, parent_id, 1, 0,
                                              offset, limit, err);
    if (!stmt) return NULL;

    return collect_rows(stmt, out_count, err);
}

skill_folder_t **acta_db_skill_folder_list_all(
    db_t *db,
    int offset, int limit,
    int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    sqlite3_stmt *stmt = prepare_folder_query(db, 0, 0, 0,
                                              offset, limit, err);
    if (!stmt) return NULL;

    return collect_rows(stmt, out_count, err);
}

skill_folder_t **acta_db_skill_folder_list_all_with_deleted(
    db_t *db,
    int offset, int limit,
    int *out_count, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = 0;

    sqlite3_stmt *stmt = prepare_folder_query(db, 0, 0, 1,
                                              offset, limit, err);
    if (!stmt) return NULL;

    return collect_rows(stmt, out_count, err);
}

/* ================================================================
 *  Counts
 * ================================================================ */

int acta_db_skill_folder_count_children(db_t *db, int parent_id, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql;
    if (parent_id == 0)
        sql = "SELECT COUNT(*) FROM skill_folders"
              " WHERE parent_id IS NULL AND deleted_at IS NULL;";
    else
        sql = "SELECT COUNT(*) FROM skill_folders"
              " WHERE parent_id = ? AND deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    if (parent_id != 0)
        sqlite3_bind_int(stmt, 1, parent_id);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = (int)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (result >= 0) { if (err) *err = ACTA_DB_OK; }
    else             { if (err) *err = ACTA_DB_ERR_SQL; }
    return result;
}

int acta_db_skill_folder_count_all(db_t *db, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql =
        "SELECT COUNT(*) FROM skill_folders WHERE deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = (int)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (result >= 0) { if (err) *err = ACTA_DB_OK; }
    else             { if (err) *err = ACTA_DB_ERR_SQL; }
    return result;
}

/* ================================================================
 *  Free
 * ================================================================ */

void acta_db_skill_folder_free(skill_folder_t *f)
{
    if (!f) return;
    DB_FREE_STR(f->name);
    DB_FREE_STR(f->created_at);
    DB_FREE_STR(f->updated_at);
    DB_FREE_STR(f->deleted_at);
    free(f);
}

void acta_db_skill_folder_list_free(skill_folder_t **items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_skill_folder_free(items[i]);
    free(items);
}
