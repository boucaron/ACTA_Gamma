#include "skill_folder.h"
#include "internal.h"
#include "db.h"

#include <stdio.h>

/* ================================================================
 *  Row mapping
 * ================================================================ */

static skill_folder_t *row_to_skill_folder(sqlite3_stmt *stmt)
{
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

/* ================================================================
 *  Shared lister helper
 * ================================================================ */

/* Drive a prepared statement and collect rows into a growable array.
 * Frees `stmt` on exit.  Returns:
 *   valid array  – one or more rows collected
 *   NULL + *err = ACTA_DB_OK       – zero rows (normal)
 *   NULL + *err = ACTA_DB_ERR_ALLOC– OOM
 *   NULL + *err = ACTA_DB_ERR_SQL  – step failure
 */
static skill_folder_t **collect_rows(sqlite3_stmt *stmt,
                                     int *out_count, int *err)
{
    int count = 0;
    skill_folder_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        skill_folder_t *item = row_to_skill_folder(stmt);
        if (!item) goto alloc_fail;

        skill_folder_t **tmp = realloc(items,
                                       sizeof(skill_folder_t *) * (count + 1));
        if (!tmp) {
            acta_db_skill_folder_free(item);
            goto alloc_fail_existing;
        }
        items = tmp;
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    if (count == 0) { free(items); return NULL; }
    return items;

alloc_fail:
    /* current row could not be allocated */
    for (int i = 0; i < count; i++) acta_db_skill_folder_free(items[i]);
    free(items);
    sqlite3_finalize(stmt);
    if (err) *err = ACTA_DB_ERR_ALLOC;
    return NULL;

alloc_fail_existing:
    for (int i = 0; i < count; i++) acta_db_skill_folder_free(items[i]);
    free(items);
    sqlite3_finalize(stmt);
    if (err) *err = ACTA_DB_ERR_ALLOC;
    return NULL;
}

/* Build "SELECT … FROM skill_folders WHERE … ORDER BY name [LIMIT ? OFFSET ?];"
 * and run it.  Returns a prepared+bound statement ready to step, or NULL. */
static sqlite3_stmt *prepare_folder_query(db_t *db,
                                          int parent_id,
                                          int has_parent_filter,
                                          int offset,
                                          int limit,
                                          int *err)
{
    char sql[512];

    /* SELECT list + table + WHERE */
    const char *where;
    if (has_parent_filter && parent_id == 0)
        where = " WHERE parent_id IS NULL AND deleted_at IS NULL";
    else if (has_parent_filter)
        where = " WHERE parent_id = ? AND deleted_at IS NULL";
    else
        where = " WHERE deleted_at IS NULL";

    int idx = 1;
    int len;

    len = snprintf(sql, sizeof(sql),
                   "SELECT id, name, parent_id, created_at, updated_at, deleted_at"
                   " FROM skill_folders%s ORDER BY name", where);

    if (limit > 0)
        len += snprintf(sql + len, sizeof(sql) - (size_t)len,
                        " LIMIT ? OFFSET ?");
    len += snprintf(sql + len, sizeof(sql) - (size_t)len, ";");

    (void)len;

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    if (has_parent_filter && parent_id != 0) {
        sqlite3_bind_int(stmt, idx++, parent_id);
    }
    if (limit > 0) {
        sqlite3_bind_int(stmt, idx++, limit);
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
    if (!db || !name || !out_id) return ACTA_DB_ERR_INVALID;

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
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;

    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}

int acta_db_skill_folder_rename(db_t *db, int id, const char *new_name)
{
    if (!db || !new_name || !*new_name) return ACTA_DB_ERR_INVALID;

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

/**
 * Soft-delete a skill_folder by setting deleted_at.
 *
 * Invariant (shared with model_folder_soft_delete):
 *   A folder that still has live (non-deleted) children cannot be
 *   deleted — the caller must delete or re-parent the children first.
 *
 * Returns:
 *   ACTA_DB_OK          row was soft-deleted
 *   ACTA_DB_ERR_INVALID db handle is NULL, OR the folder has live children
 *   ACTA_DB_ERR_NOT_FOUND no live row with that id
 *   ACTA_DB_ERR_SQL   any SQLite failure
 */
int acta_db_skill_folder_soft_delete(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Reject if the folder has live children */
    const char *check_sql =
        "SELECT COUNT(*) FROM skill_folders"
        " WHERE parent_id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *check;
    if (sqlite3_prepare_v2(db->handle, check_sql, -1, &check, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(check, 1, id);

    int child_count = 0;
    if (sqlite3_step(check) == SQLITE_ROW)
        child_count = (int)sqlite3_column_int64(check, 0);
    sqlite3_finalize(check);

    if (child_count > 0) return ACTA_DB_ERR_INVALID;

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

int acta_db_skill_folder_restore(db_t *db, int id)
{
    if (!db) return ACTA_DB_ERR_INVALID;

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
    }
    sqlite3_finalize(stmt);

    if (!result && err) {
        *err = ACTA_DB_OK;   /* not-found is "success" */
    } else if (err) {
        *err = ACTA_DB_OK;
    }
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

    sqlite3_stmt *stmt = prepare_folder_query(db, parent_id, 1,
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

    sqlite3_stmt *stmt = prepare_folder_query(db, 0, 0,
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

    const char *sql =
        "SELECT COUNT(*) FROM skill_folders"
        " WHERE parent_id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    sqlite3_bind_int(stmt, 1, parent_id);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = (int)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (result >= 0 && err) *err = ACTA_DB_OK;
    else if (err) *err = ACTA_DB_ERR_SQL;
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

    if (result >= 0 && err) *err = ACTA_DB_OK;
    else if (err) *err = ACTA_DB_ERR_SQL;
    return result;
}

/* ================================================================
 *  Free
 * ================================================================ */

void acta_db_skill_folder_free(skill_folder_t *f)
{
    if (!f) return;
    free(f->name);
    free(f->created_at);
    free(f->updated_at);
    free(f->deleted_at);
    free(f);
}

void acta_db_skill_folder_list_free(skill_folder_t **items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_skill_folder_free(items[i]);
    free(items);
}
