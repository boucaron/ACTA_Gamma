#ifndef ACTA_DB_MODEL_REVISION_H
#define ACTA_DB_MODEL_REVISION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/*
 * All query functions accept an optional int *err out-parameter.
 * Pass NULL if the caller does not need the specific error code.
 *
 * Error codes (from db.h):
 *   ACTA_DB_OK            – success (or "not found" for single-row getters)
 *   ACTA_DB_ERR_NOT_FOUND – query succeeded but no matching row
 *   ACTA_DB_ERR_SQL       – sqlite3_prepare / step failure
 *   ACTA_DB_ERR_ALLOC     – calloc / realloc failure
 *   ACTA_DB_ERR_INVALID   – NULL db / out_count
 */

model_revision_t *acta_db_model_revision_get(
        db_t *db, int id, int *err);

model_revision_t *acta_db_model_revision_get_by_model_and_rev(
        db_t *db, int model_id, int revision, int *err);

model_revision_t *acta_db_model_revision_list_by_model(
        db_t *db, int model_id, int *out_count, int *err);

model_revision_t *acta_db_model_revision_get_latest(
        db_t *db, int model_id, int *err);

void acta_db_model_revision_free(model_revision_t *r);
void acta_db_model_revision_list_free(model_revision_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_REVISION_H */
