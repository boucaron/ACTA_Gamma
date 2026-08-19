#include "internal.h"
#include "skill_revision.h"

#include <stdio.h>

static skill_revision_t *row_to_skill_revision(sqlite3_stmt *stmt) {
    skill_revision_t *r = calloc(1, sizeof(skill_revision_t));
    if (!r) return NULL;
    r->id              = db_col_int(stmt, 0);
    r->skill_id        = db_col_int(stmt, 1);
    r->revision        = db_col_int(stmt, 2);
    r->folder_id       = db_col_int_or_zero(stmt, 3);
    r->name            = db_col_text(stmt, 4);
    r->description     = db_col_text(stmt, 5);
    r->prompt_template = db_col_text(stmt, 6);
    r->output_schema   = db_col_text(stmt, 7);
    r->created_at      = db_col_text(stmt, 8);
    r->updated_at      = db_col_text(stmt, 9);
    r->deleted_at      = db_col_text(stmt, 10);
    return r;
}

#define SKILL_REV_SELECT "SELECT id, skill_id, revision, folder_id, name, description, prompt_template, output_schema, created_at, updated_at, deleted_at FROM skill_revisions"

skill_revision_t *acta_db_skill_revision_get(db_t *db, int id) {
    if (!db) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, id);
    skill_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_skill_revision(stmt);
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_get_by_skill_and_rev(db_t *db, int skill_id, int revision) {
    if (!db) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE skill_id = ? AND revision = ?;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, skill_id);
    sqlite3_bind_int(stmt, 2, revision);
    skill_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_skill_revision(stmt);
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_get_latest(db_t *db, int skill_id) {
    if (!db) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql),
             "%s WHERE skill_id = ? ORDER BY revision DESC LIMIT 1;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, skill_id);
    skill_revision_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_skill_revision(stmt);
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_list_by_skill(db_t *db, int skill_id, int *out_count) {
    if (!db || !out_count) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE skill_id = ? ORDER BY revision;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, skill_id);

    int count = 0;
    skill_revision_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        skill_revision_t *item = row_to_skill_revision(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        skill_revision_t *tmp = realloc(items, sizeof(skill_revision_t) * (count + 1));
        if (!tmp) { acta_db_skill_revision_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

void acta_db_skill_revision_free(skill_revision_t *r) {
    if (!r) return;
    free(r->name); free(r->description); free(r->prompt_template);
    free(r->output_schema); free(r->created_at); free(r->updated_at); free(r->deleted_at); free(r);
}

void acta_db_skill_revision_list_free(skill_revision_t *items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        free(items[i].name);
        free(items[i].description);
        free(items[i].prompt_template);
        free(items[i].output_schema);
        free(items[i].created_at);
        free(items[i].updated_at);
        free(items[i].deleted_at);
    }
    free(items);
}
