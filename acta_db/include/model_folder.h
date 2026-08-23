/* acta_db_model_folder.h */
#ifndef ACTA_DB_MODEL_FOLDER_H
#define ACTA_DB_MODEL_FOLDER_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- model_folder_t ---------- */

typedef struct {
    int     id;
    char   *name;
    int     parent_id;      /* 0 = root */
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;     /* NULL if live */
} model_folder_t;

/*
 * ── Common pagination contract (all listers below) ─────────────────
 *
 *   offset – zero-based row offset (skip this many rows).
 *            Must be >= 0; a negative value yields ACTA_DB_ERR_INVALID.
 *   limit  – maximum number of rows to return.
 *            <= 0 means no limit (return all matching rows).
 *
 *   Return value:
 *     heap-allocated array of model_folder_t*  →  success (possibly empty)
 *     NULL                                      →  real failure
 *
 *   If out_count is non-NULL it receives the number of items returned.
 *   If err is non-NULL it is set to ACTA_DB_OK on success or a
 *   negative ACTA_DB_ERR_* code on failure.
 *   Either out_count or err (or both) may be NULL.
 * ────────────────────────────────────────────────────────────────────
 */

/* --- Mutators ──────────────────────────────────────────────────────── */

/* Create a new folder.
 *
 * @param db         database handle (must not be NULL).
 * @param name       folder name (must not be NULL).
 * @param parent_id  parent folder id, or 0 for root-level.
 * @param out_id     [out] receives the new row id; may be NULL.
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if db or name is
 * NULL, ACTA_DB_ERR_SQL on prepare/step failure. */
int  acta_db_model_folder_create(db_t *db, const char *name, int parent_id,
                                 int *out_id);

/* Rename an existing folder.
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if db, id, or new_name
 * is invalid, ACTA_DB_ERR_SQL on failure. */
int  acta_db_model_folder_rename(db_t *db, int id, const char *new_name);

/* Soft-delete a folder (sets deleted_at).
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if db is NULL,
 * ACTA_DB_ERR_SQL on failure. */
int  acta_db_model_folder_soft_delete(db_t *db, int id);

/* Restore a soft-deleted folder (clears deleted_at).
 *
 * No-op (returns ACTA_DB_OK) if the folder is already live
 * (deleted_at IS NULL).
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if db is NULL,
 * ACTA_DB_ERR_SQL on failure. */
int  acta_db_model_folder_restore(db_t *db, int id);

/* --- Getters ───────────────────────────────────────────────────────── */

/* Fetch a single folder by primary key.
 *
 * Returns a heap-allocated model_folder_t* (free with
 * acta_db_model_folder_free), or NULL when the row is not found.
 *
 * If err is non-NULL it is set to:
 *   ACTA_DB_OK         – row found, or not found (both are "success")
 *   ACTA_DB_ERR_SQL    – database failure
 *   ACTA_DB_ERR_ALLOC  – memory allocation failure
 *   ACTA_DB_ERR_INVALID– db or id is invalid
 *
 * err may be NULL. */
model_folder_t *acta_db_model_folder_get(db_t *db, int id, int *err);

/* --- Listers ───────────────────────────────────────────────────────── */

/* Return a page of child folders of the given parent, ordered by id.
 *
 * If parent_id is 0 returns root-level folders (parent IS NULL in SQL).
 *
 * Pagination: see common contract above. */
model_folder_t **acta_db_model_folder_list_children(db_t *db,
                                                    int parent_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err);

/* Return a page of all folders, ordered by id.
 *
 * Pagination: see common contract above. */
model_folder_t **acta_db_model_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count, int *err);

/* --- Counts ----------------------------------------------------------- */

/* Return the number of direct children of the given folder.
 * Mirrors the WHERE clause of list_children (no pagination).
 *
 * Returns the count (>= 0) on success, or -1 on error (*err set).
 * err may be NULL. */
int acta_db_model_folder_count_children(db_t *db,
                                        int  parent_id,
                                        int *err);

/* Return the total number of folders in the table.
 * Mirrors the WHERE clause of list_all (none – full table).
 *
 * Returns the count (>= 0) on success, or -1 on error (*err set).
 * err may be NULL. */
int acta_db_model_folder_count_all(db_t *db,
                                   int  *err);

/* --- Free ──────────────────────────────────────────────────────────── */

/* Free a single model_folder_t and its string fields. Safe with NULL. */
void acta_db_model_folder_free(model_folder_t *f);

/* Free an array of model_folder_t* pointers returned by a lister.
 * Frees each element and the array itself. Safe with NULL. */
void acta_db_model_folder_list_free(model_folder_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_FOLDER_H */
