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
    char   *deleted_at;     /* NULL if live */
} skill_t;

/* ── Mutators ────────────────────────────────────────────────────── */

/* Insert a skill row.
 *
 * Returns ACTA_DB_OK on success; ACTA_DB_ERR_INVALID if db, s,
 * s->name, or s->prompt_template is NULL; ACTA_DB_ERR_SQL on
 * prepare/step failure.
 *
 * @param out_id  [out] receives the new row id; may be NULL.
 *
 * Optional fields (stored as SQL NULL when absent):
 *   s->folder_id    – 0 means NULL (top-level)
 *   s->description  – NULL pointer → SQL NULL
 *   s->output_schema– NULL pointer → SQL NULL
 */
int acta_db_skill_create(db_t *db, const skill_t *s, int *out_id);

/* Full-row update. All fields are written; pass the complete struct.
 * NULL optional fields overwrite the stored value with SQL NULL.
 * folder_id == 0 is stored as NULL.
 *
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db or s is NULL.
 * ACTA_DB_ERR_NOT_FOUND if no live row matches s->id.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int acta_db_skill_update(db_t *db, const skill_t *s);

int acta_db_skill_soft_delete(db_t *db, int id);
int acta_db_skill_restore(db_t *db, int id);

/* Move a live skill into a folder.
 * folder_id = 0 → move to root (NULL).
 *
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_INVALID if db is NULL.
 * ACTA_DB_ERR_NOT_FOUND if target folder doesn't exist / is deleted,
 *   or if the skill is not live.
 * ACTA_DB_ERR_SQL on prepare/step failure. */
int acta_db_skill_move_to_folder(db_t *db, int skill_id, int folder_id);

/* ── Getters ─────────────────────────────────────────────────────── */

/* Fetch a single skill by id (includes soft-deleted rows).
 * Returns a heap-allocated skill_t, or NULL.
 * *err = ACTA_DB_OK on success or not-found; negative on failure.
 * Free with acta_db_skill_free. */
skill_t *acta_db_skill_get(db_t *db, int id, int *err);

/* Fetch a single live skill (deleted_at IS NULL).
 * Same contract as acta_db_skill_get; returns NULL if deleted. */
skill_t *acta_db_skill_get_live(db_t *db, int id, int *err);

/* ── Listers ─────────────────────────────────────────────────────── */

/* List live skills in a folder, ordered by id.
 *
 * folder_id – 0 for root level only.
 *
 * Pagination:
 *   offset – rows to skip (0-based). Must be >= 0.
 *   limit  – max rows to return. <= 0 means no limit (return all).
 *
 * Returns a heap-allocated array of skill_t*, or NULL.
 * *out_count (nullable) receives the number of items returned.
 * *err (nullable) is ACTA_DB_OK on success/empty, negative on failure.
 * Free with acta_db_skill_list_free. */
skill_t **acta_db_skill_list_in_folder(db_t *db,
                                       int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err);

/* List all live skills, ordered by id.
 *
 * Pagination: same contract as list_in_folder. */
skill_t **acta_db_skill_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err);

/* ── Count ───────────────────────────────────────────────────────── */

/* Return the total number of live skills matching the filter.
 *
 * folder_id – 0 means "all folders" (no filter);
 *             > 0 restricts to that folder (root-level only, not recursive).
 *
 * Returns the count (>= 0) on success, or -1 on error.
 * If err is non-NULL it receives ACTA_DB_OK or a negative code.
 * err may be NULL. */
int acta_db_skill_count(db_t *db,
                        int folder_id,
                        int *err);

/* ── Free ────────────────────────────────────────────────────────── */

/* Free a single skill_t and its string fields. Safe with NULL. */
void acta_db_skill_free(skill_t *s);

/* Free an array of skill_t* pointers returned by a lister,
 * plus the pointer array itself. Safe with NULL. */
void acta_db_skill_list_free(skill_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_H */
