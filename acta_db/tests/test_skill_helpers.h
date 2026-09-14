/* test_skill_helpers.h — shared helpers for skill test files */
#ifndef TEST_SKILL_HELPERS_H
#define TEST_SKILL_HELPERS_H

#include "test_common.h"
#include "skill.h"
#include "skill_folder.h"
#include "skill_revision.h"
#include "db.h"

/*
 * Create a skill.  Returns the new row id (> 0) on success,
 * or the negative ACTA_DB_ERR_* code on failure.
 */
static inline int sk_create_skill(db_t *db, int folder_id,
                                  char *name,
                                  char *prompt,
                                  char *schema)
{
    skill_t s = {0};
    s.name            = name;
    s.description     = "desc";
    s.prompt_template = prompt;
    s.output_schema   = schema;
    s.folder_id       = folder_id;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    return (rc == ACTA_DB_OK) ? id : rc;
}

/*
 * Create a skill with explicit description (may be NULL).
 */
static inline int sk_create_skill_full(db_t *db, int folder_id,
                                       char *name,
                                       char *desc,
                                       char *prompt,
                                       char *schema)
{
    skill_t s = {0};
    s.name            = name;
    s.description     = desc;
    s.prompt_template = prompt;
    s.output_schema   = schema;
    s.folder_id       = folder_id;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    return (rc == ACTA_DB_OK) ? id : rc;
}

/*
 * Create a skill folder.  Returns the new row id (> 0) on success,
 * or the negative ACTA_DB_ERR_* code on failure.
 */
static inline int sk_create_folder(db_t *db, const char *name, int parent_id)
{
    int id = 0;
    int rc = acta_db_skill_folder_create(db, name, parent_id, &id);
    return (rc == ACTA_DB_OK) ? id : rc;
}

/*
 * Total number of revisions for a skill (SQL COUNT, no row fetch).
 */
static inline int sk_count_revisions(db_t *db, int skill_id)
{
    return acta_db_skill_revision_count(db, skill_id, NULL);
}

/*
 * Highest revision number for a skill (single-row SQL fetch).
 * Returns 0 if no revisions exist.
 */
static inline int sk_latest_revision(db_t *db, int skill_id)
{
    skill_revision_t *r = acta_db_skill_revision_get_latest(db, skill_id, NULL);
    if (!r) return 0;
    int rev = r->revision;
    acta_db_skill_revision_free(r);
    return rev;
}

/*
 * Whether the latest revision is soft-deleted.
 * Returns 0 (false) if no revisions exist.
 */
static inline int sk_latest_rev_deleted(db_t *db, int skill_id)
{
    skill_revision_t *r = acta_db_skill_revision_get_latest(db, skill_id, NULL);
    if (!r) return 0;
    int result = (r->deleted_at != NULL);
    acta_db_skill_revision_free(r);
    return result;
}

#endif /* TEST_SKILL_HELPERS_H */
