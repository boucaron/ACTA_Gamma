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

/* ── Pagination: see the common pagination contract in db.h ────────────
 *    (offset >= 0; limit clamped to ACTA_DB_MAX_PAGE; out_count/err
 *    nullable; empty page is success with *out_count == 0.) */

/* --- Mutators ──────────────────────────────────────────────────────── */

/* Create a new folder.
 *
 * @param db         database handle (must not be NULL).
 * @param name       folder name (must not be NULL or empty).
 * @param parent_id  parent folder id, or 0 for root-level.
 * @param out_id     [out] receives the new row id; may be NULL.
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if db is NULL
 * or name is NULL/empty, ACTA_DB_ERR_SQL on prepare/step failure. */
int  acta_db_model_folder_create(db_t *db, const char *name, int parent_id,
                                 int *out_id);

/* Rename an existing folder.
 *
 * Returns ACTA_DB_OK on success,
 * ACTA_DB_ERR_INVALID if db is NULL, id is invalid, or new_name is NULL/empty,
 * ACTA_DB_ERR_NOT_FOUND if no live folder matches id,
 * ACTA_DB_ERR_SQL on failure. */
int  acta_db_model_folder_rename(db_t *db, int id, const char *new_name);

/**
 * Soft-delete a model_folder (sets deleted_at = now).
 *
 * Rejects the delete if the folder still has live (non-deleted)
 * children — delete or re-parent them first.
 *
 * Invariant shared with acta_db_skill_folder_soft_delete:
 *   live children → ACTA_DB_ERR_INVALID.
 *
 * Returns:
 *   ACTA_DB_OK           folder was soft-deleted.
 *   ACTA_DB_ERR_INVALID  db is NULL, or the folder has live children.
 *   ACTA_DB_ERR_NOT_FOUND no live row with that id (absent or already
 *                         soft-deleted).
 *   ACTA_DB_ERR_SQL      SQLite prepare/step failure.
 */
int acta_db_model_folder_soft_delete(db_t *db, int id);


/* Restore a soft-deleted folder (clears deleted_at).
 *
 * No-op (returns ACTA_DB_OK) if the folder is already live
 * (deleted_at IS NULL).
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID if db is NULL,
 * ACTA_DB_ERR_NOT_FOUND if no folder with that id exists (live or
 * deleted), ACTA_DB_ERR_SQL on failure. */
int  acta_db_model_folder_restore(db_t *db, int id);

/*
 * Reparent a live folder to a different parent.
 *
 *   folder_id     – the folder to move (must be live).
 *   new_parent_id – target parent, or 0 for root-level.
 *
 * Validation (in order, first failure wins):
 *   db is NULL                         → ACTA_DB_ERR_INVALID
 *   folder not found / soft-deleted   → ACTA_DB_ERR_NOT_FOUND
 *   new_parent_id > 0 but that folder
 *     does not exist / is deleted     → ACTA_DB_ERR_NOT_FOUND
 *   new_parent_id is a descendant of
 *     folder_id (cycle)              → ACTA_DB_ERR_INVALID
 *   folder is already under
 *     new_parent_id (no-op)          → ACTA_DB_OK
 *
 * On success the folder's parent_id and updated_at are updated.
 *
 * Returns ACTA_DB_OK on success, or a negative ACTA_DB_ERR_* code.
 */
int  acta_db_model_folder_move_to(db_t *db, int folder_id, int new_parent_id);

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

/* Return a page of live child folders of the given parent, ordered by id.
 *
 * If parent_id is 0 returns root-level folders (parent IS NULL in SQL).
 * Soft-deleted children are excluded.
 *
 * Pagination: see common contract above. */
model_folder_t **acta_db_model_folder_list_children(db_t *db,
                                                    int parent_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err);

/* Return a page of all live folders, ordered by id.
 *
 * Soft-deleted rows are excluded.
 *
 * Pagination: see common contract above. */
model_folder_t **acta_db_model_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count, int *err);

/* --- Counts ----------------------------------------------------------- */

/* Return the number of live direct children of the given folder.
 * Mirrors the WHERE clause of list_children (no pagination).
 *
 * Returns the count (>= 0) on success, or -1 on error (*err set).
 * err may be NULL. */
int acta_db_model_folder_count_children(db_t *db,
                                        int  parent_id,
                                        int *err);

/* Return the total number of live folders.
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
