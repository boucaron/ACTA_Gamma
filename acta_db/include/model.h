/* acta_db_model.h */
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

/*
 * Mutator – insert a new model row.
 *
 * Returns ACTA_DB_OK on success; *out_id receives the new row id.
 * ACTA_DB_ERR_INVALID if db, m, m->name, m->backend, or
 * m->model_identifier is NULL.
 * ACTA_DB_ERR_SQL on prepare/step failure.
 *
 * out_id may be NULL if the caller does not need the new id.
 */
int      acta_db_model_create(db_t *db, const model_t *m, int *out_id);

/*
 * Getter – fetch a single model by primary key (includes soft-deleted rows).
 *
 * Returns a heap-allocated model_t, or NULL if the row does not exist.
 * If err is non-NULL:
 *   ACTA_DB_OK         – success (row found) or not found
 *   ACTA_DB_ERR_SQL    – database failure
 *   ACTA_DB_ERR_ALLOC  – memory allocation failure
 *   ACTA_DB_ERR_INVALID– db is NULL or id <= 0
 *
 * err may be NULL.
 * Free the result with acta_db_model_free().
 */
model_t *acta_db_model_get(db_t *db, int id, int *err);

/*
 * Getter – like acta_db_model_get, but returns NULL for soft-deleted rows
 * (deleted_at IS NOT NULL).  Useful when the caller only wants live models
 * without an extra filter.
 */
model_t *acta_db_model_get_live(db_t *db, int id, int *err);

/*
 * Mutator – full-row update.
 *
 * All fields are written; pass the complete struct.
 * NULL optional fields (description, base_url, configuration) are stored
 * as SQL NULL, overwriting any previous value.  folder_id == 0 is stored
 * as NULL.
 *
 * name, backend, and model_identifier must be non-NULL; passing NULL for
 * any of them is undefined behaviour.
 *
 * To change a single field, first read the row (acta_db_model_get),
 * modify that field, then call this function with the full struct.
 *
 * Returns:
 *   ACTA_DB_OK           – success
 *   ACTA_DB_ERR_INVALID  – db is NULL or m is NULL
 *   ACTA_DB_ERR_NOT_FOUND– no live row matches m->id
 *   ACTA_DB_ERR_SQL      – prepare/step failure
 */
int      acta_db_model_update(db_t *db, const model_t *m);

/*
 * Mutator – soft-delete (set deleted_at = now()).
 *
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_NOT_FOUND if no live row matches id.
 * ACTA_DB_ERR_SQL on failure.
 */
int      acta_db_model_soft_delete(db_t *db, int id);

/*
 * Mutator – restore a soft-deleted model (set deleted_at = NULL).
 *
 * Returns ACTA_DB_OK on success (including when already live – no-op).
 * ACTA_DB_ERR_SQL on failure.
 */
int      acta_db_model_restore(db_t *db, int id);

/*
 * Mutator – move a model to a different folder.
 *
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_NOT_FOUND if the model or target folder does not exist /
 *   is soft-deleted.
 * ACTA_DB_ERR_INVALID if db is NULL.
 * ACTA_DB_ERR_SQL on failure.
 */
int      acta_db_model_move_to_folder(db_t *db, int model_id, int folder_id);

/*
 * Lister – models in a specific folder, ordered by id.
 *
 *   folder_id – the folder to list (0 = root-level only).
 *
 *   offset – zero-based row offset.  Must be >= 0.
 *   limit  – maximum number of rows to return.
 *            <= 0 means no limit (return all matching rows).
 *
 * Returns a heap-allocated array of model_t* (free with
 * acta_db_model_list_free), or NULL on real failure.
 *
 *   *out_count (if non-NULL) – number of items actually returned.
 *   *err       (if non-NULL) – ACTA_DB_OK on success (including empty),
 *                              negative ACTA_DB_ERR_* on failure.
 *
 * Both out_count and err may be NULL.
 */
model_t **acta_db_model_list_in_folder(db_t *db,
                                       int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err);

/*
 * Lister – all models (across all folders), ordered by id.
 *
 *   offset – zero-based row offset.  Must be >= 0.
 *   limit  – maximum number of rows to return.
 *            <= 0 means no limit (return all matching rows).
 *
 * Returns a heap-allocated array of model_t* (free with
 * acta_db_model_list_free), or NULL on real failure.
 *
 *   *out_count (if non-NULL) – number of items actually returned.
 *   *err       (if non-NULL) – ACTA_DB_OK on success (including empty),
 *                              negative ACTA_DB_ERR_* on failure.
 *
 * Both out_count and err may be NULL.
 */
model_t **acta_db_model_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err);

/*
 * Count – total number of model rows matching the given folder filter.
 *
 *   folder_id – restrict to this folder (0 = all folders, i.e. no filter).
 *
 * Returns the row count (>= 0) on success, or -1 on failure.
 * If err is non-NULL it is set to ACTA_DB_OK or a negative ACTA_DB_ERR_*.
 * err may be NULL.
 */
int      acta_db_model_count(db_t *db, int folder_id, int *err);

/* ---------- free ---------- */

/* Free a single model_t and its string fields. Safe with NULL. */
void     acta_db_model_free(model_t *m);

/* Free an array of model_t* returned by a lister, plus the array itself.
 * Safe with NULL. */
void     acta_db_model_list_free(model_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_H */
