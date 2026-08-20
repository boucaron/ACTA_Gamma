#include "internal.h"
#include "model_revision.h"
#include "db.h" /* for ACTA_DB_* error codes */

#include <stdio.h>

/* ── internal row decoder ─────────────────────────────────────────── */

static model_revision_t *row_to_model_revision(sqlite3_stmt *stmt) {
    model_revision_t *r = calloc(1, sizeof(model_revision_t));
    if (!r) return NULL;
    r->id               = db_col_int(stmt, 0);
    r->model_id         = db_col_int(stmt, 1);
    r->revision         = db_col_int(stmt, 2);
    r->folder_id        = db_col_int_or_zero(stmt, 3);
    r->name             = db_col_text(stmt, 4);
    r->description      = db_col_text(stmt, 5);
    r->backend          = db_col_text(stmt, 6);
    r->base_url         = db_col_text(stmt, 7);
    r->model_identifier = db_col_text(stmt, 8);
    r->configuration    = db_col_text(stmt, 9);
    r->created_at       = db_col_text(stmt, 10);
    r->updated_at       = db_col_text(stmt, 11);
    r->deleted_at       = db_col_text(stmt, 12);
    return r;
}

#define REV_SELECT \
    "SELECT id, model_id, revision, folder_id, name, description, " \
    "backend, base_url, model_identifier, configuration, " \
    "created_at, updated_at, deleted_at FROM model_revisions"

/* ── single-row getters ────────────────────────────────────────────── */

model_revision_t *acta_db_model_revision_get(db_t *db, int id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) { if (err) *err = ACTA_DB_ERR_INVALID; return NULL; }

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    model_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model_revision(stmt);
        if (!result) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            sqlite3_finalize(stmt);
            return NULL;
        }
    }
    /* not-found: result stays NULL, *err remains ACTA_DB_OK */
    sqlite3_finalize(stmt);
    return result;
}

model_revision_t *acta_db_model_revision_get_by_model_and_rev(
        db_t *db, int model_id, int revision, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) { if (err) *err = ACTA_DB_ERR_INVALID; return NULL; }

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE model_id = ? AND revision = ?;", REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, model_id);
    sqlite3_bind_int(stmt, 2, revision);

    model_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model_revision(stmt);
        if (!result) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            sqlite3_finalize(stmt);
            return NULL;
        }
    }
    /* not-found: result stays NULL, *err remains ACTA_DB_OK */
    sqlite3_finalize(stmt);
    return result;
}

model_revision_t *acta_db_model_revision_get_latest(db_t *db, int model_id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) { if (err) *err = ACTA_DB_ERR_INVALID; return NULL; }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "%s WHERE model_id = ? ORDER BY revision DESC LIMIT 1;", REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, model_id);

    model_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_model_revision(stmt);
        if (!result) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            sqlite3_finalize(stmt);
            return NULL;
        }
    }
    /* not-found: result stays NULL, *err remains ACTA_DB_OK */
    sqlite3_finalize(stmt);
    return result;
}

/* ── paginated list (multi-row) ────────────────────────────────────── */

model_revision_t **acta_db_model_revision_list_by_model(
        db_t *db, int model_id,
        int offset, int limit,
        int *out_count, int *err)
{
    if (err)       *err       = ACTA_DB_OK;
    if (out_count) *out_count = 0;
    if (!db) { if (err) *err = ACTA_DB_ERR_INVALID; return NULL; }

    char sql[512];

    if (limit > 0) {
        /* Paginated: bind model_id, limit, offset */
        snprintf(sql, sizeof(sql),
                 "%s WHERE model_id = ? ORDER BY revision LIMIT ? OFFSET ?;",
                 REV_SELECT);

        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
            if (err) *err = ACTA_DB_ERR_SQL;
            return NULL;
        }
        sqlite3_bind_int(stmt, 1, model_id);
        sqlite3_bind_int(stmt, 2, limit);
        sqlite3_bind_int(stmt, 3, offset);

        int               count = 0;
        model_revision_t **items = NULL;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            model_revision_t *item = row_to_model_revision(stmt);
            if (!item) {
                if (err) *err = ACTA_DB_ERR_ALLOC;
                acta_db_model_revision_list_free(items, count);
                sqlite3_finalize(stmt);
                return NULL;
            }

            model_revision_t **tmp =
                realloc(items, sizeof(model_revision_t *) * (count + 1));
            if (!tmp) {
                if (err) *err = ACTA_DB_ERR_ALLOC;
                acta_db_model_revision_free(item);
                acta_db_model_revision_list_free(items, count);
                sqlite3_finalize(stmt);
                return NULL;
            }
            items = tmp;
            items[count++] = item;
        }

        sqlite3_finalize(stmt);
        if (out_count) *out_count = count;
        return items;  /* NULL with count 0 – not an error */

    } else {
        /* Unpaginated: no LIMIT/OFFSET clause */
        snprintf(sql, sizeof(sql),
                 "%s WHERE model_id = ? ORDER BY revision;",
                 REV_SELECT);

        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
            if (err) *err = ACTA_DB_ERR_SQL;
            return NULL;
        }
        sqlite3_bind_int(stmt, 1, model_id);

        int               count = 0;
        model_revision_t **items = NULL;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            model_revision_t *item = row_to_model_revision(stmt);
            if (!item) {
                if (err) *err = ACTA_DB_ERR_ALLOC;
                acta_db_model_revision_list_free(items, count);
                sqlite3_finalize(stmt);
                return NULL;
            }

            model_revision_t **tmp =
                realloc(items, sizeof(model_revision_t *) * (count + 1));
            if (!tmp) {
                if (err) *err = ACTA_DB_ERR_ALLOC;
                acta_db_model_revision_free(item);
                acta_db_model_revision_list_free(items, count);
                sqlite3_finalize(stmt);
                return NULL;
            }
            items = tmp;
            items[count++] = item;
        }

        sqlite3_finalize(stmt);
        if (out_count) *out_count = count;
        return items;
    }
}

/* ── destructors ───────────────────────────────────────────────────── */

void acta_db_model_revision_free(model_revision_t *r) {
    if (!r) return;
    free(r->name);
    free(r->description);
    free(r->backend);
    free(r->base_url);
    free(r->model_identifier);
    free(r->configuration);
    free(r->created_at);
    free(r->updated_at);
    free(r->deleted_at);
    free(r);
}

void acta_db_model_revision_list_free(model_revision_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        acta_db_model_revision_free(items[i]);
    }
    free(items);
}
