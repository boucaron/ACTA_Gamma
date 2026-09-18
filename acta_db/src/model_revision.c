#include "internal.h"
#include "model_revision.h"
#include "db.h"

#include <stdio.h>

/* ── constants ────────────────────────────────────────────────────── */

#define REV_SELECT \
    "SELECT id, model_id, revision, folder_id, name, description, " \
    "backend, base_url, model_identifier, configuration, " \
    "created_at, updated_at, deleted_at FROM model_revisions"

#define REV_SQL_BUF 512

/* ── internal helpers ─────────────────────────────────────────────── */

static model_revision_t *row_to_model_revision(sqlite3_stmt *stmt, int *err)
{
    model_revision_t *r = calloc(1, sizeof(model_revision_t));
    if (!r) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    r->id               = db_col_int(stmt, 0);
    r->model_id         = db_col_int(stmt, 1);
    r->revision         = db_col_int(stmt, 2);
    r->folder_id        = db_col_int_or_zero(stmt, 3);
    r->name             = db_col_text(stmt, 4,  &alloc_err);
    r->description      = db_col_text(stmt, 5,  &alloc_err);
    r->backend          = db_col_text(stmt, 6,  &alloc_err);
    r->base_url         = db_col_text(stmt, 7,  &alloc_err);
    r->model_identifier = db_col_text(stmt, 8,  &alloc_err);
    r->configuration    = db_col_text(stmt, 9,  &alloc_err);
    r->created_at       = db_col_text(stmt, 10, &alloc_err);
    r->updated_at       = db_col_text(stmt, 11, &alloc_err);
    r->deleted_at       = db_col_text(stmt, 12, &alloc_err);

    if (alloc_err) {
        acta_db_model_revision_free(r);
        free(r);
        if (err) *err = alloc_err;
        return NULL;
    }
    return r;
}

static int rev_build_sql(char *buf, size_t buf_sz, const char *suffix)
{
    int n = snprintf(buf, buf_sz, "%s %s;", REV_SELECT, suffix);
    if (n < 0 || (size_t)n >= buf_sz) return -1;
    return 0;
}

/* ── single-row getters ────────────────────────────────────────────── */

model_revision_t *acta_db_model_revision_get(db_t *db, int id, int *err)
{
    if (err) *err = ACTA_DB_OK;
    if (!db || id <= 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[REV_SQL_BUF];
    if (rev_build_sql(sql, sizeof(sql),
                      "WHERE id = ? AND deleted_at IS NULL") != 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    model_revision_t *result = NULL;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = row_to_model_revision(stmt, err);
    } else if (rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

model_revision_t *acta_db_model_revision_get_by_model_and_rev(
        db_t *db, int model_id, int revision, int *err)
{
    if (err) *err = ACTA_DB_OK;
    if (!db || model_id <= 0 || revision <= 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[REV_SQL_BUF];
    if (rev_build_sql(sql, sizeof(sql),
                      "WHERE model_id = ? AND revision = ?") != 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, model_id);
    sqlite3_bind_int(stmt, 2, revision);

    model_revision_t *result = NULL;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = row_to_model_revision(stmt, err);
    } else if (rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

model_revision_t *acta_db_model_revision_get_latest(db_t *db, int model_id, int *err)
{
    if (err) *err = ACTA_DB_OK;
    if (!db || model_id <= 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[REV_SQL_BUF];
    if (rev_build_sql(sql, sizeof(sql),
                      "WHERE model_id = ? AND deleted_at IS NULL "
                      "ORDER BY revision DESC LIMIT 1") != 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, model_id);

    model_revision_t *result = NULL;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = row_to_model_revision(stmt, err);
    } else if (rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

/* ── paginated lister ──────────────────────────────────────────────── */

model_revision_t **acta_db_model_revision_list_by_model(
        db_t *db, int model_id,
        int offset, int limit,
        int *out_count, int *err)
{
    if (err)       *err       = ACTA_DB_OK;
    if (out_count) *out_count = 0;
    if (!db || model_id <= 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    int effective_limit = db_clamp_limit(limit);   /* ← was: limit > 0 ? limit : -1 */

    char sql[REV_SQL_BUF];
    if (rev_build_sql(sql, sizeof(sql),
                      "WHERE model_id = ? ORDER BY revision LIMIT ? OFFSET ?")
        != 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, model_id);
    sqlite3_bind_int(stmt, 2, effective_limit);
    sqlite3_bind_int(stmt, 3, offset);

    int               count = 0;
    int               cap   = 0;
    model_revision_t **items = NULL;

    while (1) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            if (err) *err = ACTA_DB_ERR_SQL;
            acta_db_model_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            return NULL;
        }

        int alloc_err = ACTA_DB_OK;
        model_revision_t *item = row_to_model_revision(stmt, &alloc_err);
        if (!item) {
            if (err) *err = alloc_err;
            acta_db_model_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            return NULL;
        }

        if (count >= cap) {
            int new_cap = (cap == 0) ? 8 : cap * 2;
            model_revision_t **tmp =
                realloc(items, sizeof(model_revision_t *) * (size_t)new_cap);
            if (!tmp) {
                if (err) *err = ACTA_DB_ERR_ALLOC;
                acta_db_model_revision_free(item);
                acta_db_model_revision_list_free(items, count);
                sqlite3_finalize(stmt);
                return NULL;
            }
            items = tmp;
            cap = new_cap;
        }
        items[count++] = item;
    }

    sqlite3_finalize(stmt);
    if (out_count) *out_count = count;
    return items;
}

/* ── count ─────────────────────────────────────────────────────────── */

int acta_db_model_revision_count(db_t *db, int model_id, int *err)
{
    if (err) *err = ACTA_DB_OK;
    if (!db || model_id <= 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle,
            "SELECT COUNT(*) FROM model_revisions WHERE model_id = ?;",
            -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    sqlite3_bind_int(stmt, 1, model_id);

    int count = -1;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        count = (int)sqlite3_column_int64(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return count;
}

/* ── destructors ───────────────────────────────────────────────────── */

void acta_db_model_revision_free(model_revision_t *r) {
    if (!r) return;
    DB_FREE_STR(r->name);
    DB_FREE_STR(r->description);
    DB_FREE_STR(r->backend);
    DB_FREE_STR(r->base_url);
    DB_FREE_STR(r->model_identifier);
    DB_FREE_STR(r->configuration);
    DB_FREE_STR(r->created_at);
    DB_FREE_STR(r->updated_at);
    DB_FREE_STR(r->deleted_at);
    free(r);
}

void acta_db_model_revision_list_free(model_revision_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        acta_db_model_revision_free(items[i]);
    }
    free(items);
}
