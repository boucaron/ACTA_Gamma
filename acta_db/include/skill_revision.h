#ifndef ACTA_DB_SKILL_REVISION_H
#define ACTA_DB_SKILL_REVISION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * skill_revision_t
 *
 * A new revision row is inserted by a DB trigger every time
 * acta_db_skill_create or acta_db_skill_update succeeds.
 * The `revision` column is an auto-incremented sequence per skill
 * (1, 2, 3, …).
 *
 * Revisions are immutable: no update or delete function is exposed
 * for this table.
 */
typedef struct {
    int     id;
    int     skill_id;
    int     revision;
    int     folder_id;      /* 0 = root */
    char   *name;
    char   *description;
    char   *prompt_template;
    char   *output_schema;
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;
} skill_revision_t;

/* --- Getters ---------------------------------------------------------- */

/*
 * Fetch a single revision by its primary key.
 *
 * Returns a heap-allocated skill_revision_t on success, or NULL.
 * err may be NULL. On success / not-found: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*.
 *
 * Free the result with acta_db_skill_revision_free().
 */
skill_revision_t *acta_db_skill_revision_get(
    db_t *db, int id, int *err);

/*
 * Fetch a specific revision by skill id and revision number.
 *
 * Returns a heap-allocated skill_revision_t on success, or NULL.
 * err may be NULL. On success / not-found: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*.
 *
 * Free the result with acta_db_skill_revision_free().
 */
skill_revision_t *acta_db_skill_revision_get_by_skill_and_rev(
    db_t *db, int skill_id, int revision, int *err);

/*
 * Fetch the latest (highest revision number) for a given skill.
 *
 * Returns a heap-allocated skill_revision_t on success, or NULL.
 * err may be NULL. On success / not-found: *err = ACTA_DB_OK.
 * On real failure: returns NULL, *err = negative ACTA_DB_ERR_*.
 *
 * Free the result with acta_db_skill_revision_free().
 */
skill_revision_t *acta_db_skill_revision_get_latest(
    db_t *db, int skill_id, int *err);

/* --- Lister ----------------------------------------------------------- */

/*
 * Paginated listing of all revisions for a given skill,
 * ordered by revision ascending (oldest first).
 *
 * Pagination:
 *   offset – zero-based row offset (skip this many rows). Must be >= 0.
 *   limit  – maximum number of rows to return.
 *            <= 0 means no limit (return all matching rows).
 *
 * Returns a heap-allocated array of skill_revision_t* on success,
 * or NULL on real failure.
 *
 * If out_count is non-NULL it receives the number of items actually
 * returned (may be less than limit if the result set is exhausted).
 * If err is non-NULL it is set to ACTA_DB_OK on success (including an
 * empty result set) or a negative ACTA_DB_ERR_* code on failure.
 * Either out_count or err (or both) may be NULL.
 *
 * Free the result with acta_db_skill_revision_list_free().
 */
skill_revision_t **acta_db_skill_revision_list_by_skill(
    db_t *db, int skill_id,
    int offset, int limit,
    int *out_count, int *err);

/* --- Count ------------------------------------------------------------ */

/*
 * Return the total number of revision rows for a given skill.
 *
 * Returns the row count (>= 0) on success, or -1 on failure
 * (in which case *err receives a negative ACTA_DB_ERR_* code).
 * err may be NULL if the caller does not need the specific code.
 */
int acta_db_skill_revision_count(db_t *db,
                                 int skill_id,
                                 int *err);

/* --- Free -------------------------------------------------------------- */

/* Free a single revision (and its string fields). Safe with NULL. */
void acta_db_skill_revision_free(skill_revision_t *r);

/* Free an array of skill_revision_t* pointers returned by the lister.
 * Frees each element and the array itself. Safe with NULL. */
void acta_db_skill_revision_list_free(skill_revision_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_REVISION_H */
