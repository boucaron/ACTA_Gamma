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

/* ---------------------------------------------------------------------- */
/*  Getters                                                                */
/* ---------------------------------------------------------------------- */

skill_revision_t *acta_db_skill_revision_get(db_t *db, int id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    skill_revision_t *result = NULL;
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
        }
    } else if (rc == SQLITE_DONE) {
        /* Not found: return NULL, err stays ACTA_DB_OK */
    } else {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_get_by_skill_and_rev(
        db_t *db, int skill_id, int revision, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE skill_id = ? AND revision = ?;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);
    sqlite3_bind_int(stmt, 2, revision);
    int rc = sqlite3_step(stmt);
    skill_revision_t *result = NULL;
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
        }
    } else if (rc == SQLITE_DONE) {
        /* Not found: return NULL, err stays ACTA_DB_OK */
    } else {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

skill_revision_t *acta_db_skill_revision_get_latest(db_t *db, int skill_id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "%s WHERE skill_id = ? ORDER BY revision DESC LIMIT 1;", SKILL_REV_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);
    int rc = sqlite3_step(stmt);
    skill_revision_t *result = NULL;
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
        }
    } else if (rc == SQLITE_DONE) {
        /* Not found: return NULL, err stays ACTA_DB_OK */
    } else {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    sqlite3_finalize(stmt);
    return result;
}

/* ---------------------------------------------------------------------- */
/*  Lister                                                                 */
/* ---------------------------------------------------------------------- */
skill_revision_t **acta_db_skill_revision_list_by_skill(
        db_t *db, int skill_id, int offset, int limit,
        int *out_count, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db || !out_count) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (offset < 0) offset = 0;

    char sql[512];
    if (limit > 0) {
        snprintf(sql, sizeof(sql),
                 "%s WHERE skill_id = ? ORDER BY revision LIMIT ? OFFSET ?;",
                 SKILL_REV_SELECT);
    } else {
        snprintf(sql, sizeof(sql),
                 "%s WHERE skill_id = ? ORDER BY revision;", SKILL_REV_SELECT);
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        *out_count = 0;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);
    if (limit > 0) {
        sqlite3_bind_int(stmt, 2, limit);
        sqlite3_bind_int(stmt, 3, offset);
    }

    int count = 0;
    skill_revision_t **items = NULL;
    int step_rc;
    while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        skill_revision_t *item = row_to_skill_revision(stmt);
        if (!item) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            acta_db_skill_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            *out_count = 0;
            return NULL;
        }
        skill_revision_t **tmp = realloc(items,
                                         sizeof(skill_revision_t *) * (count + 1));
        if (!tmp) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            acta_db_skill_revision_free(item);
            acta_db_skill_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }
    if (step_rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
        acta_db_skill_revision_list_free(items, count);
        sqlite3_finalize(stmt);
        *out_count = 0;
        return NULL;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;
}


/* ---------------------------------------------------------------------- */
/*  Free                                                                   */
/* ---------------------------------------------------------------------- */

void acta_db_skill_revision_free(skill_revision_t *r) {
    if (!r) return;
    free(r->name);
    free(r->description);
    free(r->prompt_template);
    free(r->output_schema);
    free(r->created_at);
    free(r->updated_at);
    free(r->deleted_at);
    free(r);
}

void acta_db_skill_revision_list_free(skill_revision_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        acta_db_skill_revision_free(items[i]);
    }
    free(items);
}
