/* acta_db_skill_folder.h */
#ifndef ACTA_DB_SKILL_FOLDER_H
#define ACTA_DB_SKILL_FOLDER_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- skill_folder_t ---------- */

typedef struct {
    int     id;
    char   *name;
    int     parent_id;      /* 0 = root */
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;     /* NULL if live */
} skill_folder_t;

/* --- Mutators -------------------------------------------------------- */

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db, name, or out_id is NULL.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int acta_db_skill_folder_create(db_t *db,
                               const char *name,
                               int parent_id,
                               int *out_id);

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL or new_name is empty.
 * ACTA_DB_ERR_NOT_FOUND if no live folder matches id.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int acta_db_skill_folder_rename(db_t *db, int id, const char *new_name);

/**
 * Soft-delete a skill_folder (sets deleted_at = now).
 *
 * Rejects the delete if the folder still has live (non-deleted)
 * children — delete or re-parent them first.
 *
 * Returns:
 *   ACTA_DB_OK           folder was soft-deleted.
 *   ACTA_DB_ERR_INVALID  db is NULL, or the folder has live children.
 *   ACTA_DB_ERR_NOT_FOUND no live row with that id (absent or already
 *                         soft-deleted).
 *   ACTA_DB_ERR_SQL      SQLite prepare/step failure.
 */
int acta_db_skill_folder_soft_delete(db_t *db, int id);

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL.
 * ACTA_DB_ERR_NOT_FOUND if no folder matches id.
 * ACTA_DB_ERR_SQL on prepare/step failure.
 * No-op (returns ACTA_DB_OK) if the folder is already live. */
int acta_db_skill_folder_restore(db_t *db, int id);

/* --- Getters ---------------------------------------------------------- */

/* Fetch a single folder by primary key.
 * Returns heap-allocated skill_folder_t, or NULL if not found / on failure.
 * err may be NULL; on success or not-found *err = ACTA_DB_OK,
 * on real failure *err = negative ACTA_DB_ERR_*. */
skill_folder_t *acta_db_skill_folder_get(db_t *db, int id, int *err);

/* --- Listers ---------------------------------------------------------- */

/* Return a page of child folders (WHERE parent_id = ?) ordered by id.
 *
 * offset – rows to skip (>= 0; negative → ACTA_DB_ERR_INVALID).
 * limit  – max rows to return; <= 0 means no limit (return all).
 *
 * Returns heap-allocated array of skill_folder_t*, or NULL on failure.
 * *out_count (nullable) receives the number of rows returned.
 * *err (nullable) receives ACTA_DB_OK on success/empty, negative on failure. */
skill_folder_t **acta_db_skill_folder_list_children(db_t *db,
                                                    int parent_id,
                                                    int offset, int limit,
                                                    int *out_count,
                                                    int *err);

/* Return a page of all folders ordered by id.
 *
 * offset – rows to skip (>= 0; negative → ACTA_DB_ERR_INVALID).
 * limit  – max rows to return; <= 0 means no limit (return all).
 *
 * Returns heap-allocated array of skill_folder_t*, or NULL on failure.
 * *out_count (nullable) receives the number of rows returned.
 * *err (nullable) receives ACTA_DB_OK on success/empty, negative on failure. */
skill_folder_t **acta_db_skill_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count,
                                               int *err);

/* --- Counts ----------------------------------------------------------- */

/* Return the total number of child folders for the given parent.
 * Mirrors the WHERE clause of list_children (no pagination).
 *
 * Returns the count (>= 0) on success, or -1 on error (*err set).
 * err may be NULL. */
int acta_db_skill_folder_count_children(db_t *db,
                                        int parent_id,
                                        int *err);

/* Return the total number of all folders.
 * Mirrors the WHERE clause of list_all (none – full table).
 *
 * Returns the count (>= 0) on success, or -1 on error (*err set).
 * err may be NULL. */
int acta_db_skill_folder_count_all(db_t *db,
                                   int *err);

/* --- Free ------------------------------------------------------------- */

/* Free a single folder struct (and its string fields). Safe with NULL. */
void acta_db_skill_folder_free(skill_folder_t *f);

/* Free an array returned by a lister: frees each element and the array.
 * Safe with NULL. */
void acta_db_skill_folder_list_free(skill_folder_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_FOLDER_H */
