#include "internal.h"
#include "skill_revision.h"
#include "db.h"

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

skill_revision_t *acta_db_skill_revision_get(db_t *db, int id, int *out_err) {
    if (out_err) *out_err = ACTA_DB_OK;
    if (!db) { if (out_err) *out_err = ACTA_DB_ERR_INVALID; return NULL; }
    if (!out_err) return NULL;

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    skill_revision_t *result = NULL;
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result) *out_err = ACTA_DB_ERR_ALLOC;
    } else if (rc == SQLITE_DONE) {
        *out_err = ACTA_DB_ERR_NOT_FOUND;
    } else {
        *out_err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_get_by_skill_and_rev(db_t *db, int skill_id, int revision, int *out_err) {
    if (out_err) *out_err = ACTA_DB_OK;
    if (!db) { if (out_err) *out_err = ACTA_DB_ERR_INVALID; return NULL; }
    if (!out_err) return NULL;

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE skill_id = ? AND revision = ?;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);
    sqlite3_bind_int(stmt, 2, revision);
    int rc = sqlite3_step(stmt);
    skill_revision_t *result = NULL;
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result) *out_err = ACTA_DB_ERR_ALLOC;
    } else if (rc == SQLITE_DONE) {
        *out_err = ACTA_DB_ERR_NOT_FOUND;
    } else {
        *out_err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_get_latest(db_t *db, int skill_id, int *out_err) {
    if (out_err) *out_err = ACTA_DB_OK;
    if (!db) { if (out_err) *out_err = ACTA_DB_ERR_INVALID; return NULL; }
    if (!out_err) return NULL;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "%s WHERE skill_id = ? ORDER BY revision DESC LIMIT 1;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);
    int rc = sqlite3_step(stmt);
    skill_revision_t *result = NULL;
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result) *out_err = ACTA_DB_ERR_ALLOC;
    } else if (rc == SQLITE_DONE) {
        *out_err = ACTA_DB_ERR_NOT_FOUND;
    } else {
        *out_err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_list_by_skill(db_t *db, int skill_id, int *out_count, int *out_err) {
    if (out_err) *out_err = ACTA_DB_OK;
    if (!db || !out_count || !out_err) {
        if (out_err) *out_err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE skill_id = ? ORDER BY revision;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_err = ACTA_DB_ERR_SQL;
        *out_count = 0;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);

    int count = 0;
    skill_revision_t *items = NULL;
    int step_rc;
    while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        skill_revision_t *item = row_to_skill_revision(stmt);
        if (!item) {
            *out_err = ACTA_DB_ERR_ALLOC;
            acta_db_skill_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            *out_count = 0;
            return NULL;
        }
        skill_revision_t *tmp = realloc(items, sizeof(skill_revision_t) * (count + 1));
        if (!tmp) {
            *out_err = ACTA_DB_ERR_ALLOC;
            acta_db_skill_revision_free(item);
            acta_db_skill_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    if (step_rc != SQLITE_DONE) {
        *out_err = ACTA_DB_ERR_SQL;
        acta_db_skill_revision_list_free(items, count);
        sqlite3_finalize(stmt);
        *out_count = 0;
        return NULL;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    *out_err = ACTA_DB_OK;
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
