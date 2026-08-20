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

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db/name/out_id is NULL
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int            acta_db_skill_folder_create(db_t *db, const char *name, int parent_id, int *out_id);

/* Returns a heap-allocated skill_folder_t on success, or NULL.
 * err may be NULL. On success / not-found: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_folder_t *acta_db_skill_folder_get(db_t *db, int id, int *err);

/* Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on failure. */
int            acta_db_skill_folder_rename(db_t *db, int id, const char *new_name);

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL or folder has live children.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int            acta_db_skill_folder_soft_delete(db_t *db, int id);

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL.
 * ACTA_DB_ERR_SQL on prepare/step failure.
 * No-op (returns ACTA_DB_OK) if the folder is already live (deleted_at IS NULL). */
int            acta_db_skill_folder_restore(db_t *db, int id);

/* Returns a heap-allocated array of skill_folder_t* on success, or NULL.
 * err may be NULL. *out_count receives the item count.
 * offset is the row offset (0-based); limit is max rows (-1 or 0 = no limit).
 * On success / empty: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_folder_t **acta_db_skill_folder_list_children(db_t *db,
                                                    int parent_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err);

/* Returns a heap-allocated array of skill_folder_t* on success, or NULL.
 * err may be NULL. *out_count receives the item count.
 * offset is the row offset (0-based); limit is max rows (-1 or 0 = no limit).
 * On success / empty: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_folder_t **acta_db_skill_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count, int *err);

void            acta_db_skill_folder_free(skill_folder_t *f);
void            acta_db_skill_folder_list_free(skill_folder_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_FOLDER_H */
