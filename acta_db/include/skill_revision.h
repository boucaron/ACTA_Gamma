#ifndef ACTA_DB_SKILL_REVISION_H
#define ACTA_DB_SKILL_REVISION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

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

skill_revision_t *acta_db_skill_revision_get(
    db_t *db, int id, int *err);

skill_revision_t *acta_db_skill_revision_get_by_skill_and_rev(
    db_t *db, int skill_id, int revision, int *err);

skill_revision_t *acta_db_skill_revision_get_latest(
    db_t *db, int skill_id, int *err);

/* --- Lister ----------------------------------------------------------- */

skill_revision_t **acta_db_skill_revision_list_by_skill(
    db_t *db, int skill_id,
    int offset, int limit,
    int *out_count, int *err);

/* --- Free -------------------------------------------------------------- */

void acta_db_skill_revision_free(skill_revision_t *r);
void acta_db_skill_revision_list_free(skill_revision_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_REVISION_H */
