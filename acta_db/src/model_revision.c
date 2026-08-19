#include "internal.h"
#include "model_revision.h"

#include <stdio.h>

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

#define REV_SELECT "SELECT id, model_id, revision, folder_id, name, description, backend, base_url, model_identifier, configuration, created_at, updated_at, deleted_at FROM model_revisions"

model_revision_t *acta_db_model_revision_get(db_t *db, int id) {
    if (!db) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, id);

    model_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_model_revision(stmt);
    sqlite3_finalize(stmt);
    return result;
}

model_revision_t *acta_db_model_revision_get_by_model_and_rev(db_t *db, int model_id, int revision) {
    if (!db) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE model_id = ? AND revision = ?;", REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, model_id);
    sqlite3_bind_int(stmt, 2, revision);

    model_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_model_revision(stmt);
    sqlite3_finalize(stmt);
    return result;
}

model_revision_t *acta_db_model_revision_list_by_model(db_t *db, int model_id, int *out_count) {
    if (!db || !out_count) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE model_id = ? ORDER BY revision;", REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, model_id);

    int count = 0;
    model_revision_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        model_revision_t *item = row_to_model_revision(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        model_revision_t *tmp = realloc(items, sizeof(model_revision_t) * (count + 1));
        if (!tmp) { acta_db_model_revision_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

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

void acta_db_model_revision_list_free(model_revision_t *items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        free(items[i].name);
        free(items[i].description);
        free(items[i].backend);
        free(items[i].base_url);
        free(items[i].model_identifier);
        free(items[i].configuration);
        free(items[i].created_at);
        free(items[i].updated_at);
        free(items[i].deleted_at);
    }
    free(items);
}

