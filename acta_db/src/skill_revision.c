#include "internal.h"
#include "skill_revision.h"
#include "db.h"

/* ── Row mapping ─────────────────────────────────────────────────────────── */

static skill_revision_t *row_to_skill_revision(sqlite3_stmt *stmt, int *err) {
    skill_revision_t *r = calloc(1, sizeof(skill_revision_t));
    if (!r) {
        if (err) *err = ACTA_DB_ERR_ALLOC;
        return NULL;
    }

    int alloc_err = ACTA_DB_OK;
    r->id              = db_col_int(stmt, 0);
    r->skill_id        = db_col_int(stmt, 1);
    r->revision        = db_col_int(stmt, 2);
    r->folder_id       = db_col_int_or_zero(stmt, 3);
    r->name            = db_col_text(stmt, 4, &alloc_err);
    r->description     = db_col_text(stmt, 5, &alloc_err);
    r->prompt_template = db_col_text(stmt, 6, &alloc_err);
    r->output_schema   = db_col_text(stmt, 7, &alloc_err);
    r->created_at      = db_col_text(stmt, 8, &alloc_err);
    r->updated_at      = db_col_text(stmt, 9, &alloc_err);
    r->deleted_at      = db_col_text(stmt, 10, &alloc_err);

    if (alloc_err) {
        acta_db_skill_revision_free(r);
        if (err) *err = alloc_err;
        return NULL;
    }
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
        result = row_to_skill_revision(stmt, err);
    } else if (rc != SQLITE_DONE) {
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
        result = row_to_skill_revision(stmt, err);
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
        result = row_to_skill_revision(stmt, err);
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
    if (offset < 0) {
        if (err) *err = ACTA_DB_ERR_INVALID;
        if (out_count) *out_count = 0;
        return NULL;
    }

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

    int      count = 0;
    int      cap   = 0;
    int      step_rc;
    skill_revision_t **items = NULL;

    while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        /* Grow geometrically: 8, 16, 32, … */
        if (count >= cap) {
            int new_cap = (cap == 0) ? 8 : cap * 2;
            skill_revision_t **tmp = realloc(items,
                sizeof(skill_revision_t *) * (size_t)new_cap);
            if (!tmp) {
                if (err) *err = ACTA_DB_ERR_ALLOC;
                acta_db_skill_revision_list_free(items, count);
                sqlite3_finalize(stmt);
                if (out_count) *out_count = 0;
                return NULL;
            }
            items = tmp;
            cap   = new_cap;
        }

        skill_revision_t *item = row_to_skill_revision(stmt, err);
        if (!item) {
            acta_db_skill_revision_list_free(items, count);
            sqlite3_finalize(stmt);
            if (out_count) *out_count = 0;
            return NULL;
        }
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
    return items;   /* NULL when count == 0 (empty, not an error) */
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
    DB_FREE_STR(r->name);
    DB_FREE_STR(r->description);
    DB_FREE_STR(r->prompt_template);
    DB_FREE_STR(r->output_schema);
    DB_FREE_STR(r->created_at);
    DB_FREE_STR(r->updated_at);
    DB_FREE_STR(r->deleted_at);
    free(r);
}


void acta_db_skill_revision_list_free(skill_revision_t **items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        acta_db_skill_revision_free(items[i]);
    }
    free(items);
}
