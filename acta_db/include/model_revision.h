#ifndef ACTA_DB_MODEL_REVISION_H
#define ACTA_DB_MODEL_REVISION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A new revision row is inserted by a DB trigger every time an
 * insert/update is called on the parent model.  The `revision` column
 * is an auto-incremented sequence scoped per model.
 *
 * All query functions accept an optional `int *err` out-parameter.
 * Pass NULL if the caller does not need the specific error code.
 *
 * Semantics:
 *   – Success (row found)         → valid pointer,  *err = ACTA_DB_OK
 *   – Not-found (no matching row) → NULL,           *err = ACTA_DB_OK
 *   – Real failure                → NULL,           *err = negative ACTA_DB_ERR_*
 *
 * ACTA_DB_ERR_NOT_FOUND is NOT returned by these functions;
 * not-found maps to ACTA_DB_OK.
 */

/* --- Row --- */

typedef struct {
    int     id;
    int     model_id;
    int     revision;
    int     folder_id;      /* 0 = root */
    char   *name;
    char   *description;
    char   *backend;
    char   *base_url;
    char   *model_identifier;
    char   *configuration;
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;     /* NULL if live */
} model_revision_t;

/* --- Getters --- */

model_revision_t *acta_db_model_revision_get(
        db_t *db, int id, int *err);

model_revision_t *acta_db_model_revision_get_by_model_and_rev(
        db_t *db, int model_id, int revision, int *err);

model_revision_t *acta_db_model_revision_get_latest(
        db_t *db, int model_id, int *err);

/* --- Lister --- */

/*
 * Paginated listing of revisions belonging to `model_id`,
 * ordered by revision ASC.
 *
 *   offset – number of rows to skip (0-based; 0 = first page).
 *   limit  – maximum number of rows to return.
 *            <= 0 or > ACTA_DB_MAX_PAGE → clamped to ACTA_DB_MAX_PAGE
 *            (see the common pagination contract in db.h).
 *
 * Returns a heap-allocated array of model_revision_t* (free with
 * acta_db_model_revision_list_free), or NULL on real failure.
 *
 * If out_count is non-NULL it receives the number of rows actually
 * returned.  If err is non-NULL it is set to ACTA_DB_OK on success
 * (including an empty result) or a negative ACTA_DB_ERR_* code.
 * Either out_count or err (or both) may be NULL.
 */
model_revision_t **acta_db_model_revision_list_by_model(
        db_t *db, int model_id,
        int offset, int limit,
        int *out_count, int *err);

/* --- Count --- */

/*
 * Return the total number of revision rows for the given model.
 *
 * Returns >= 0 on success (the row count), or -1 on failure
 * (*err set to a negative ACTA_DB_ERR_* code).
 *
 * err may be NULL if the caller does not need the error code.
 */
int acta_db_model_revision_count(
        db_t *db, int model_id,
        int *err);

/* --- Free --- */

void acta_db_model_revision_free(model_revision_t *r);
void acta_db_model_revision_list_free(model_revision_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_REVISION_H */
