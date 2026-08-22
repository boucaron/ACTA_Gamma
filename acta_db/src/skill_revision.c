#include "internal.h"
#include "skill_revision.h"
#include "db.h"

#include <stdio.h>

/* ── Row mapping ─────────────────────────────────────────────────────────── */

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

#define SKILL_REV_SELECT \
    "SELECT id, skill_id, revision, folder_id, name, description, " \
    "prompt_template, output_schema, created_at, updated_at, deleted_at " \
    "FROM skill_revisions"

/* ── Getters ─────────────────────────────────────────────────────────────── */

skill_revision_t *acta_db_skill_revision_get(db_t *db, int id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return NULL;
    }

    sqlite3_stmt *stmt = NULL;
    skill_revision_t *result = NULL;
    const char *sql = SKILL_REV_SELECT " WHERE id = ?;";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result && err) *err = ACTA_DB_ERR_ALLOC;
    } else if (rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
    }
    /* SQLITE_DONE → not found, err stays OK */

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

    sqlite3_stmt *stmt = NULL;
    skill_revision_t *result = NULL;
    const char *sql = SKILL_REV_SELECT " WHERE skill_id = ? AND revision = ?;";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);
    sqlite3_bind_int(stmt, 2, revision);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result && err) *err = ACTA_DB_ERR_ALLOC;
    } else if (rc != SQLITE_DONE) {
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

    sqlite3_stmt *stmt = NULL;
    skill_revision_t *result = NULL;
    const char *sql = SKILL_REV_SELECT
                      " WHERE skill_id = ? ORDER BY revision DESC LIMIT 1;";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, skill_id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = row_to_skill_revision(stmt);
        if (!result && err) *err = ACTA_DB_ERR_ALLOC;
    } else if (rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
    }

    sqlite3_finalize(stmt);
    return result;
}

/* ── Lister ──────────────────────────────────────────────────────────────── */

skill_revision_t **acta_db_skill_revision_list_by_skill(
        db_t *db, int skill_id, int offset, int limit,
        int *out_count, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (offset < 0) offset = 0;

    /* Build SQL: LIMIT/OFFSET only when limit > 0 */
    const char *sql = (limit > 0)
        ? SKILL_REV_SELECT " WHERE skill_id = ? ORDER BY revision LIMIT ? OFFSET ?;"
        : SKILL_REV_SELECT " WHERE skill_id = ? ORDER BY revision;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        if (out_count) *out_count = 0;
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
            if (out_count) *out_count = 0;
            return NULL;
        }
        skill_revision_t **tmp = realloc(items,
                                         sizeof(skill_revision_t *) * (count + 1));
        if (!tmp) {
            if (err) *err = ACTA_DB_ERR_ALLOC;
            acta_db_skill_revision_free(item);
            acta_db_skill_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            return NULL;
        }
        items = tmp;
        items[count++] = item;
    }

    if (step_rc != SQLITE_DONE) {
        if (err) *err = ACTA_DB_ERR_SQL;
        acta_db_skill_revision_list_free(items, count);
        sqlite3_finalize(stmt);
        if (out_count) *out_count = 0;
        return NULL;
    }

    sqlite3_finalize(stmt);
    if (out_count) *out_count = count;
    if (err) *err = ACTA_DB_OK;
    return items;  /* may be NULL if 0 rows (empty result) */
}

/* ── Count ───────────────────────────────────────────────────────────────── */

int acta_db_skill_revision_count(db_t *db, int skill_id, int *err) {
    if (err) *err = ACTA_DB_OK;
    if (!db) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        return -1;
    }

    const char *sql = "SELECT COUNT(*) FROM skill_revisions WHERE skill_id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (err) *err = ACTA_DB_ERR_SQL;
        return -1;
    }
    sqlite3_bind_int(stmt, 1, skill_id);

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = (int)sqlite3_column_int64(stmt, 0);
    } else {
        if (err) *err = ACTA_DB_ERR_SQL;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── Free ────────────────────────────────────────────────────────────────── */

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
