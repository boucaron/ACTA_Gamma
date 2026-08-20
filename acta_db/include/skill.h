/* acta_db_skill.h */
#ifndef ACTA_DB_SKILL_H
#define ACTA_DB_SKILL_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* Inserts a skill row into the "skills" table.
 *
 * Returns ACTA_DB_OK on success.
 * Returns ACTA_DB_ERR_INVALID if db, s, s->name, or s->prompt_template is NULL.
 * Returns ACTA_DB_ERR_SQL on prepare or step failure.
 *
 * @param out_id  [out] receives the new row id; may be NULL if not needed.
 *
 * Optional fields (bound as NULL when absent):
 *   s->folder_id      — 0 means NULL (top-level)
 *   s->description    — NULL if pointer is NULL
 *   s->output_schema  — NULL if pointer is NULL
 */
int acta_db_skill_create(db_t *db, const skill_t *s, int *out_id);

/* Returns a heap-allocated skill_t on success, or NULL.
 * err may be NULL. On success / not-found: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_t *acta_db_skill_get(db_t *db, int id, int *err);

/* Returns a heap-allocated skill_t if live (deleted_at IS NULL), or NULL.
 * err may be NULL. On success / not-found: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_t *acta_db_skill_get_live(db_t *db, int id, int *err);

/**
 * Update an existing skill (full-row write).
 *
 * All fields are written; pass the full struct.
 * NULL optional fields (description, output_schema) are stored as SQL NULL,
 * overwriting any previous value.  folder_id == 0 is stored as NULL.
 *
 * To change a single field, first read the row (acta_db_skill_get),
 * modify that field, then call this function with the complete struct.
 *
 * Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on
 * failure.  Returns ACTA_DB_ERR_NOT_FOUND if no live row matches s->id.
 */
int      acta_db_skill_update(db_t *db, const skill_t *s);

/* Returns ACTA_DB_OK on success, ACTA_DB_ERR_INVALID or ACTA_DB_ERR_SQL on failure. */
int      acta_db_skill_soft_delete(db_t *db, int id);

/* Returns a heap-allocated array of skill_t* on success, or NULL.
 * err may be NULL. *out_count receives the item count.
 * offset is the row offset (0-based); limit is max rows (-1 or 0 = no limit).
 * On success / empty: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_t **acta_db_skill_list_in_folder(db_t *db,
                                        int folder_id,
                                        int offset, int limit,
                                        int *out_count, int *err);

/* Returns a heap-allocated array of skill_t* on success, or NULL.
 * err may be NULL. *out_count receives the item count.
 * offset is the row offset (0-based); limit is max rows (-1 or 0 = no limit).
 * On success / empty: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*. */
skill_t **acta_db_skill_list_all(db_t *db,
                                  int offset, int limit,
                                  int *out_count, int *err);

void     acta_db_skill_free(skill_t *s);
void     acta_db_skill_list_free(skill_t **items, int count);

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
