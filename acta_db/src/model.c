/* model.c */
#include "internal.h"
#include "model.h"
#include "db.h"

/* ---------- constants ---------- */

#define MODEL_LIST_INIT_CAP 16

#define SQL_COLS \
    "id, folder_id, name, description, backend, base_url, " \
    "model_identifier, configuration, created_at, updated_at, deleted_at"

static const char *SQL_LIST_IN_FOLDER =
    "SELECT " SQL_COLS " FROM models "
    "WHERE folder_id = ? AND deleted_at IS NULL "
    "ORDER BY id LIMIT ? OFFSET ?;";

static const char *SQL_LIST_ROOT =
    "SELECT " SQL_COLS " FROM models "
    "WHERE folder_id IS NULL AND deleted_at IS NULL "
    "ORDER BY id LIMIT ? OFFSET ?;";

static const char *SQL_LIST_ALL =
    "SELECT " SQL_COLS " FROM models "
    "WHERE deleted_at IS NULL "
    "ORDER BY id LIMIT ? OFFSET ?;";


/* ---------- row decoding ---------- */

/*
 * Decode one result row into a heap-allocated model_t.
 *
 * Uses the documented "read-all-then-bail" pattern: every column is
 * read into the struct (passing the same &alloc_err to each call),
 * and after the last column we check whether any allocation failed.
 * On failure the partial struct is freed and NULL is returned.
 */
static model_t *row_to_model(sqlite3_stmt *stmt, int *err) {
    model_t *m = calloc(1, sizeof(model_t));
    if (!m) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    m->id               = db_col_int(stmt, 0);
    m->folder_id        = db_col_int_or_zero(stmt, 1);
    m->name             = db_col_text(stmt, 2, err);
    m->description      = db_col_text(stmt, 3, err);
    m->backend          = db_col_text(stmt, 4, err);
    m->base_url         = db_col_text(stmt, 5, err);
    m->model_identifier = db_col_text(stmt, 6, err);
    m->configuration    = db_col_text(stmt, 7, err);
    m->created_at       = db_col_text(stmt, 8, err);
    m->updated_at       = db_col_text(stmt, 9, err);
    m->deleted_at       = db_col_text(stmt, 10, err);

    if (err && *err != ACTA_DB_OK) {
        acta_db_model_free(m);
        free(m);
        return NULL;
    }
    return m;
}

/* ---------- shared lister loop ---------- */

/*
 * Iterate a prepared SELECT over models, collecting rows into a
 * growable array.  Finalises the statement on every path.
 *
 * On success returns the array (possibly empty) and sets *out_count.
 * On failure returns NULL; *out_count (if non-NULL) is set to 0,
 * and *err (if non-NULL) receives the error code.
 */
static model_t **run_model_query(sqlite3_stmt *stmt,
                                 int *out_count, int *err) {
    int count = 0;
    int cap   = 0;
    model_t **items = NULL;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        /* Grow capacity (geometric). */
        if (count >= cap) {
            int new_cap = (cap == 0) ? MODEL_LIST_INIT_CAP : cap * 2;
            model_t **tmp = realloc(items, sizeof(model_t *) * (size_t)new_cap);
            if (!tmp) {
                for (int i = 0; i < count; i++) acta_db_model_free(items[i]);
                free(items);
                sqlite3_finalize(stmt);
                if (out_count) *out_count = 0;
                if (err) *err = ACTA_DB_ERR_ALLOC;
                return NULL;
            }
            items = tmp;
            cap   = new_cap;
        }

        int alloc_err = ACTA_DB_OK;
        model_t *item = row_to_model(stmt, &alloc_err);
        if (!item) {
            for (int i = 0; i < count; i++) acta_db_model_free(items[i]);
            free(items);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            if (err) *err = alloc_err;
            return NULL;
        }
        items[count++] = item;
    }
    sqlite3_finalize(stmt);

    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}

/* ---------- mutators ---------- */

int acta_db_model_create(db_t *db, const model_t *m, int *out_id)
{
    if (!db || !m || !m->name || !m->backend || !m->model_identifier)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "INSERT INTO models (folder_id, name, description, backend, base_url, "
        "model_identifier, configuration) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (m->folder_id == 0)
        sqlite3_bind_null(stmt, 1);
    else
        sqlite3_bind_int(stmt, 1, m->folder_id);
    sqlite3_bind_text(stmt, 2, m->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, m->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, m->backend, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, m->base_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, m->model_identifier, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, m->configuration, -1, SQLITE_TRANSIENT);

    int step_rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) {
        const char *msg = sqlite3_errmsg(db->handle);
        if (strstr(msg, "UNIQUE constraint failed"))
            return ACTA_DB_ERR_DUPLICATE;
        if (strstr(msg, "FOREIGN KEY"))
            return ACTA_DB_ERR_FK;
        return ACTA_DB_ERR_SQL;
    }

    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return ACTA_DB_OK;
}


int acta_db_model_update(db_t *db, const model_t *m) {
    if (!db || !m || m->id <= 0 || !m->name || !m->backend || !m->model_identifier)
        return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE models SET folder_id=?, name=?, description=?, backend=?, "
        "base_url=?, model_identifier=?, configuration=?, "
        "updated_at=datetime('now') "
        "WHERE id=? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (m->folder_id == 0)
        sqlite3_bind_null(stmt, 1);
    else
        sqlite3_bind_int(stmt, 1, m->folder_id);
    sqlite3_bind_text(stmt, 2, m->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, m->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, m->backend, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, m->base_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, m->model_identifier, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, m->configuration, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, m->id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_model_soft_delete(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    const char *sql =
        "UPDATE models SET deleted_at = datetime('now'), "
        "updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
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

int acta_db_model_restore(db_t *db, int id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    /* Attempt the restore.  If the row was already live this is a
     * no-op (0 rows changed) and we fall through to the existence
     * check below. */
    const char *sql_update =
        "UPDATE models SET deleted_at = NULL, updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NOT NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql_update, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    int changed = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    if (changed > 0) return ACTA_DB_OK;

    /* 0 rows changed: either the row doesn't exist, or it is already
     * live (deleted_at IS NULL).  Distinguish the two. */
    const char *sql_check =
        "SELECT 1 FROM models WHERE id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db->handle, sql_check, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;
    sqlite3_bind_int(stmt, 1, id);
    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

int acta_db_model_move_to_folder(db_t *db, int model_id, int folder_id) {
    if (!db) return ACTA_DB_ERR_INVALID;

    if (folder_id != 0) {
        const char *sql_chk =
            "SELECT 1 FROM model_folders "
            "WHERE id = ? AND deleted_at IS NULL LIMIT 1;";
        sqlite3_stmt *chk;
        if (sqlite3_prepare_v2(db->handle, sql_chk, -1, &chk, NULL) != SQLITE_OK)
            return ACTA_DB_ERR_SQL;
        sqlite3_bind_int(chk, 1, folder_id);
        int found = (sqlite3_step(chk) == SQLITE_ROW);
        sqlite3_finalize(chk);
        if (!found) return ACTA_DB_ERR_NOT_FOUND;
    }

    const char *sql =
        "UPDATE models SET folder_id = ?, updated_at = datetime('now') "
        "WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ACTA_DB_ERR_SQL;

    if (folder_id == 0)
        sqlite3_bind_null(stmt, 1);
    else
        sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, model_id);

    int rc = sqlite3_step(stmt);
    int changes = (rc == SQLITE_DONE) ? sqlite3_changes(db->handle) : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return ACTA_DB_ERR_SQL;
    return changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND;
}

/* ---------- getters ---------- */

model_t *acta_db_model_get(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT " SQL_COLS " FROM models WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    int found = (sqlite3_step(stmt) == SQLITE_ROW);
    model_t *result = NULL;

    if (found) {
        int alloc_err = ACTA_DB_OK;
        result = row_to_model(stmt, &alloc_err);
        if (!result && err)
            *err = alloc_err;
    }
    sqlite3_finalize(stmt);

    /* Not found is a successful query, not an error. */
    if (!result && err && !found)
        *err = ACTA_DB_OK;

    return result;
}

model_t *acta_db_model_get_live(db_t *db, int id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql =
        "SELECT " SQL_COLS " FROM models WHERE id = ? AND deleted_at IS NULL;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    int found = (sqlite3_step(stmt) == SQLITE_ROW);
    model_t *result = NULL;

    if (found) {
        int alloc_err = ACTA_DB_OK;
        result = row_to_model(stmt, &alloc_err);
        if (!result && err)
            *err = alloc_err;
    }
    sqlite3_finalize(stmt);

    if (!result && err && !found)
        *err = ACTA_DB_OK;

    return result;
}

/* ---------- listers ---------- */

model_t **acta_db_model_list_in_folder(db_t *db,
                                       int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (out_count) *out_count = 0;
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    const char *sql = (folder_id == 0) ? SQL_LIST_ROOT : SQL_LIST_IN_FOLDER;
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    int p = 1;
    if (folder_id != 0)
        sqlite3_bind_int(stmt, p++, folder_id);
    sqlite3_bind_int(stmt, p++, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, p, offset);

    return run_model_query(stmt, out_count, err);
}

model_t **acta_db_model_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (out_count) *out_count = 0;
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, SQL_LIST_ALL, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, db_clamp_limit(limit));
    sqlite3_bind_int(stmt, 2, offset);

    return run_model_query(stmt, out_count, err);
}

/* ---------- count ---------- */

int acta_db_model_count_in_folder(db_t *db, int folder_id, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql = (folder_id == 0)
        ? "SELECT COUNT(*) FROM models "
          "WHERE folder_id IS NULL AND deleted_at IS NULL;"
        : "SELECT COUNT(*) FROM models "
          "WHERE folder_id = ? AND deleted_at IS NULL;";

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

int acta_db_model_count_all(db_t *db, int *err) {
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle,
        "SELECT COUNT(*) FROM models WHERE deleted_at IS NULL;",
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

/* ---------- free ---------- */

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

void acta_db_model_list_free(model_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++)
        acta_db_model_free(items[i]);
    free(items);
}
