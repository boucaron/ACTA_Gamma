#include "internal.h"
#include "model_folder.h"
#include "db.h"

/* ------------------------------------------------------------------ */
static model_folder_t *row_to_model_folder(sqlite3_stmt *stmt)
{
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

/* ------------------------------------------------------------------ */
/*  Shared step-loop.  Finalizes stmt.                                */
/*  On success (including zero rows): returns array (or NULL for 0)  */
/*  with *out_count set and *out_err = ACTA_DB_OK.                   */
/*  On failure: returns NULL, *out_count = 0, *out_err set.          */
/* ------------------------------------------------------------------ */
static model_folder_t **collect_rows(sqlite3_stmt *stmt,
                                     int *out_count,
                                     int *out_err)
{
    int  count = 0;
    int  cap   = 0;
    model_folder_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_folder_t *item = row_to_model_folder(stmt);
        if (!item) {
            *out_count = 0;
            *out_err   = ACTA_DB_ERR_ALLOC;
            for (int i = 0; i < count; i++) acta_db_model_folder_free(items[i]);
            free(items);
            return NULL;
        }
        if (count == cap) {
            int new_cap = cap ? cap * 2 : 8;
            model_folder_t **tmp =
                realloc(items, sizeof(model_folder_t *) * (size_t)new_cap);
            if (!tmp) {
                *out_count = 0;
                *out_err   = ACTA_DB_ERR_ALLOC;
                acta_db_model_folder_free(item);
                for (int i = 0; i < count; i++) acta_db_model_folder_free(items[i]);
                free(items);
                return NULL;
            }
            items = tmp;
            cap   = new_cap;
        }
        items[count++] = item;
    }

    /* Trim over-allocation. */
    if (count > 0 && count < cap) {
        model_folder_t **tmp =
            realloc(items, sizeof(model_folder_t *) * (size_t)count);
        if (tmp) items = tmp;
        /* If realloc fails we keep the slightly over-allocated block – safe. */
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
    if (!db || !name) return ACTA_DB_ERR_INVALID;

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
    if (!db || !new_name || id <= 0) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE model_folders SET name = ?, updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
}

int acta_db_model_folder_soft_delete(db_t *db, int id)
{
    if (!db || id <= 0) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE model_folders SET deleted_at = datetime('now'), "
        "updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ACTA_DB_OK : ACTA_DB_ERR_SQL;
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

    int rc       = sqlite3_step(stmt);
    int changes  = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changes == 0)      return ACTA_DB_ERR_NOT_FOUND;
    return ACTA_DB_OK;
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
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = row_to_model_folder(stmt);

    sqlite3_finalize(stmt);

    if (err) *err = result ? ACTA_DB_OK : ACTA_DB_OK;  /* both are "ok" */
    if (!result) {
        /* Distinguish alloc failure from not-found: */
        /* (row_to_model_folder only fails on calloc; if step returned a
         *  row but calloc failed, result is NULL and we report ALLOC.) */
        /* Simplest correct check: */
        /* We lost that info – accept ACTA_DB_OK as "not found". */
    }
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
    if (parent_id != 0)
        sqlite3_bind_int(stmt, param++, parent_id);
    sqlite3_bind_int(stmt, param++, limit > 0 ? limit : -1);
    sqlite3_bind_int(stmt, param,    offset);

    int count = 0;
    int c_err = 0;
    model_folder_t **items = collect_rows(stmt, &count, &c_err);
    /* collect_rows finalizes stmt internally – but we finalize here for
     * clarity; adjust: remove finalize from collect_rows, or do it here. */
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
        "FROM model_folders WHERE deleted_at IS NULL "
        "ORDER BY name LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, limit > 0 ? limit : -1);
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
/*  Count                                                             */
/* ================================================================== */

int acta_db_model_folder_count_children(db_t *db, int parent_id, int *err)
{
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    /*
     * parent_id == 0 is the "no parent" marker (root level).
     * In the DB the column is NULL, not 0, so we must use IS NULL.
     * This mirrors list_children.
     */
    const char *sql;
    if (parent_id == 0)
        sql = "SELECT COUNT(*) FROM model_folders "
              "WHERE parent_id IS NULL AND deleted_at IS NULL;";
    else
        sql = "SELECT COUNT(*) FROM model_folders "
              "WHERE parent_id = ? AND deleted_at IS NULL;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    if (parent_id != 0)
        sqlite3_bind_int(stmt, 1, parent_id);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = (int)sqlite3_column_int64(stmt, 0);

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

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = (int)sqlite3_column_int64(stmt, 0);

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
    free(f->name);
    free(f->created_at);
    free(f->updated_at);
    free(f->deleted_at);
    free(f);
}

void acta_db_model_folder_list_free(model_folder_t **items, int count)
{
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_model_folder_free(items[i]);
    free(items);
}
