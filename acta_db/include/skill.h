#ifndef ACTA_DB_SKILL_H
#define ACTA_DB_SKILL_H

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
    char   *deleted_at;
} skill_folder_t;

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db/name/out_id is NULL or folder has live children.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int           acta_db_skill_folder_create(db_t *db, const char *name, int parent_id, int *out_id);

/* Returns a heap-allocated skill_folder_t, or NULL if not found / error. */
skill_folder_t *acta_db_skill_folder_get(db_t *db, int id);

/* Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on failure. */
int           acta_db_skill_folder_rename(db_t *db, int id, const char *new_name);

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL or folder has live children.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int           acta_db_skill_folder_soft_delete(db_t *db, int id);

/* Returns a heap-allocated array of skill_folder_t, or NULL on error.
 * *out_count receives the number of items. */
skill_folder_t *acta_db_skill_folder_list_children(db_t *db, int parent_id, int *out_count);
skill_folder_t *acta_db_skill_folder_list_all(db_t *db, int *out_count);

void          acta_db_skill_folder_free(skill_folder_t *f);
void          acta_db_skill_folder_list_free(skill_folder_t *items, int count);

/* ---------- skill_t ---------- */

typedef struct {
    int     id;
    int     folder_id;      /* 0 = root */
    char   *name;
    char   *description;
    char   *prompt_template;
    char   *output_schema;
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;
} skill_t;

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db/s/name/prompt_template/out_id is NULL.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int      acta_db_skill_create(db_t *db, const skill_t *s, int *out_id);

/* Returns a heap-allocated skill_t, or NULL if not found / error. */
skill_t *acta_db_skill_get(db_t *db, int id);

/* Returns a heap-allocated skill_t if live (deleted_at IS NULL), or NULL. */
skill_t *acta_db_skill_get_live(db_t *db, int id);

/* Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on failure. */
int      acta_db_skill_update(db_t *db, const skill_t *s);

/* Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on failure. */
int      acta_db_skill_soft_delete(db_t *db, int id);

/* Returns a heap-allocated array of skill_t, or NULL on error.
 * *out_count receives the number of items. */
skill_t *acta_db_skill_list_in_folder(db_t *db, int folder_id, int *out_count);
skill_t *acta_db_skill_list_all(db_t *db, int *out_count);

void     acta_db_skill_free(skill_t *s);
void     acta_db_skill_list_free(skill_t *items, int count);

/* Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on failure. */
int  acta_db_skill_restore(db_t *db, int id);

/* Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL.
 * ACTA_DB_ERR_NOT_FOUND if target folder doesn't exist / is deleted,
 *   or if the skill is not live.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int  acta_db_skill_move_to_folder(db_t *db, int skill_id, int folder_id);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_H */
