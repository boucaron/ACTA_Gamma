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
 * Returns ACTA_DB_OK on success;
 * ACTA_DB_ERR_INVALID if db, s, s->name, or s->prompt_template is NULL;
 * ACTA_DB_ERR_DUPLICATE if the name is already used in the same folder;
 * ACTA_DB_ERR_FK if s->folder_id does not reference a live folder;
 * ACTA_DB_ERR_SQL on other prepare/step failure.
 *
 * @param out_id  [out] receives the new row id; may be NULL.
 *
 * Optional fields (stored as SQL NULL when absent):
 *   s->folder_id    – 0 means NULL (top-level)
 *   s->description  – NULL pointer → SQL NULL
 *   s->output_schema– NULL pointer → SQL NULL
 */
int acta_db_skill_create(db_t *db, const skill_t *s, int *out_id);

/*
 * Mutator – full-row update (replace, not merge).
 *
 * Every column in the SET clause is written from the corresponding
 * struct field.  There is no partial / "only non-NULL" variant.
 *
 *   • Optional fields (description, output_schema):
 *       NULL in struct  →  SQL NULL in column, overwriting prior value.
 *   • folder_id == 0  →  SQL NULL (sentinel for "no folder").
 *   • Required fields (name, prompt_template):
 *       NULL  →  call rejected, returns ACTA_DB_ERR_INVALID.
 *
 * An AFTER UPDATE trigger snapshots the prior row into skill_revision,
 * so an accidental NULL-out is recoverable from the journal — but that
 * is a safety net, not a workflow.  Populate the full struct.
 *
 * To change a single field:
 *   1. acta_db_skill_get(db, id, &s);
 *   2. modify the one field;
 *   3. call acta_db_skill_update(db, &s) with the fully-populated struct.
 *
 * Returns:
 *   ACTA_DB_OK           – success
 *   ACTA_DB_ERR_INVALID  – db/s is NULL, s->id <= 0, or a required
 *                          field (name, prompt_template) is NULL
 *   ACTA_DB_ERR_NOT_FOUND– no live row matches s->id
 *   ACTA_DB_ERR_DUPLICATE– (name, folder_id) conflicts with a live row
 *   ACTA_DB_ERR_FK       – s->folder_id does not reference a live folder
 *   ACTA_DB_ERR_SQL      – prepare/step failure
 */
int acta_db_skill_update(db_t *db, const skill_t *s);

/* Soft-delete a live skill (set deleted_at = now()).
 * Returns ACTA_DB_OK on success.
 * ACTA_DB_ERR_NOT_FOUND if no live row matches id.
 * ACTA_DB_ERR_SQL on failure. */
int acta_db_skill_soft_delete(db_t *db, int id);

/* Restore a soft-deleted skill (set deleted_at = NULL).
 * Returns ACTA_DB_OK on success (including when already live – no-op).
 * ACTA_DB_ERR_NOT_FOUND if no row matches id.
 * ACTA_DB_ERR_SQL on failure. */
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
 *   limit  – max rows to return. <= 0 or > ACTA_DB_MAX_PAGE → clamped
 *            to ACTA_DB_MAX_PAGE (see the common pagination contract in db.h).
 *
 * Returns a heap-allocated array of skill_t*, or NULL.
 * *out_count (nullable) receives the number of items returned.
 * *err (nullable) is ACTA_DB_OK on success/empty, negative on failure.
 * Free with acta_db_skill_list_free. */
skill_t **acta_db_skill_list_in_folder(db_t *db,
                                       int folder_id,
                                       int offset, int limit,
                                       int *out_count, int *err);

/* Lister – like acta_db_skill_list_in_folder, but includes soft-deleted
 * skills (deleted_at IS NOT NULL).  Callers must check deleted_at to
 * distinguish live rows from deleted ones.
 *
 *   folder_id – the folder to list (0 = root-level only).
 *
 *   offset / limit – same pagination contract as list_in_folder.
 *
 *   *out_count / *err – same out-parameters as list_in_folder;
 *   both may be NULL.
 */
skill_t **acta_db_skill_list_in_folder_with_deleted(db_t *db,
                                                    int folder_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err);

/* List all live skills, ordered by id.
 *
 * Pagination: same contract as list_in_folder. */
skill_t **acta_db_skill_list_all(db_t *db,
                                 int offset, int limit,
                                 int *out_count, int *err);

/* Lister – like acta_db_skill_list_all, but includes soft-deleted skills
 * (deleted_at IS NOT NULL).  Callers must check deleted_at to distinguish
 * live rows from deleted ones.
 *
 *   offset / limit – same pagination contract as list_all.
 *
 *   *out_count / *err – same out-parameters as list_all; both may be NULL.
 */
skill_t **acta_db_skill_list_all_with_deleted(db_t *db,
                                              int offset, int limit,
                                              int *out_count, int *err);

/* ── Count ───────────────────────────────────────────────────────── */

/* Count live skills in a specific folder.
 *
 *   folder_id – the folder to count (0 = root-level only).
 *
 * Returns the count (>= 0) on success, or -1 on error.
 * If err is non-NULL it receives ACTA_DB_OK or a negative code.
 * err may be NULL.
 *
 * Mirrors acta_db_skill_list_in_folder: the count equals the number of
 * rows you would get from list_in_folder(db, folder_id, 0, -1, …). */
int acta_db_skill_count_in_folder(db_t *db, int folder_id, int *err);

/* Count – like acta_db_skill_count_in_folder, but includes soft-deleted
 * skills (deleted_at IS NOT NULL).
 */
int acta_db_skill_count_in_folder_with_deleted(db_t *db, int folder_id,
                                               int *err);

/* Count all live skills (across all folders).
 *
 * Returns the count (>= 0) on success, or -1 on error.
 * If err is non-NULL it receives ACTA_DB_OK or a negative code.
 * err may be NULL.
 *
 * Mirrors acta_db_skill_list_all: the count equals the number of rows
 * you would get from list_all(db, 0, -1, …). */
int acta_db_skill_count_all(db_t *db, int *err);

/*
 * Count – like acta_db_skill_count_all, but includes soft-deleted skills
 * (deleted_at IS NOT NULL).
 */
int acta_db_skill_count_all_with_deleted(db_t *db, int *err);

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
