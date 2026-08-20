#ifndef ACTA_DB_MODEL_H
#define ACTA_DB_MODEL_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- model_t ---------- */

typedef struct {
    int     id;
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
} model_t;

int      acta_db_model_create(db_t *db, const model_t *m, int *out_id);
model_t *acta_db_model_get(db_t *db, int id, int *err);
model_t *acta_db_model_get_live(db_t *db, int id, int *err);   /* NULL if soft-deleted */

/**
 * Update an existing model (full-row write).
 *
 * All fields are written; pass the full struct.
 * NULL optional fields (description, base_url, configuration) are stored as
 * SQL NULL, overwriting any previous value.  folder_id == 0 is stored as
 * NULL.
 *
 * name, backend, and model_identifier must be non-NULL (no internal guard);
 * passing NULL for any of them results in undefined behaviour.
 *
 * To change a single field, first read the row (acta_db_model_get),
 * modify that field, then call this function with the complete struct.
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on
 * failure.  Returns ACTA_DB_ERR_NOT_FOUND if no live row matches s->id.
 */
int      acta_db_model_update(db_t *db, const model_t *m);
int      acta_db_model_soft_delete(db_t *db, int id);
model_t **acta_db_model_list_in_folder(db_t *db,
                                        int folder_id,
                                        int offset, int limit,
                                        int *out_count, int *err);
model_t **acta_db_model_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err);
void     acta_db_model_free(model_t *m);
void     acta_db_model_list_free(model_t **items, int count);

int  acta_db_model_restore(db_t *db, int id);
int  acta_db_model_move_to_folder(db_t *db, int model_id, int folder_id);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_H */
