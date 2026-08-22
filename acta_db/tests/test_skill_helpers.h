/* test_skill_helpers.h — shared helpers for skill test files */
#ifndef TEST_SKILL_HELPERS_H
#define TEST_SKILL_HELPERS_H

#include "test_common.h"
#include "skill.h"
#include "skill_folder.h"
#include "skill_revision.h"
#include "db.h"

static inline int sk_create_skill(db_t *db, int folder_id, const char *name,
                                   const char *prompt, const char *schema) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name            = (char *)name;
    s.description     = (char *)"desc";
    s.prompt_template = (char *)prompt;
    s.output_schema   = (char *)schema;
    s.folder_id       = folder_id;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    return (rc == ACTA_DB_OK) ? id : -1;
}

static inline int sk_create_folder(db_t *db, const char *name, int parent_id) {
    int id = 0;
    int rc = acta_db_skill_folder_create(db, name, parent_id, &id);
    return (rc == ACTA_DB_OK) ? id : -1;
}

static inline int sk_count_revisions(db_t *db, int skill_id) {
    int count = 0, err = 0;
    skill_revision_t **revs = acta_db_skill_revision_list_by_skill(
        db, skill_id, 0, 0, &count, &err);
    if (revs) acta_db_skill_revision_list_free(revs, count);
    return count;
}

static inline int sk_latest_revision(db_t *db, int skill_id) {
    int count = 0, err = 0;
    skill_revision_t **revs = acta_db_skill_revision_list_by_skill(
        db, skill_id, 0, -1, &count, &err);
    if (!revs || count == 0) {
        if (revs) acta_db_skill_revision_list_free(revs, count);
        return 0;
    }
    int max_rev = 0;
    for (int i = 0; i < count; i++)
        if (revs[i]->revision > max_rev) max_rev = revs[i]->revision;
    acta_db_skill_revision_list_free(revs, count);
    return max_rev;
}

static inline int sk_latest_rev_deleted(db_t *db, int skill_id) {
    int count = 0, err = 0;
    skill_revision_t **revs = acta_db_skill_revision_list_by_skill(
        db, skill_id, 0, -1, &count, &err);
    if (!revs || count == 0) {
        if (revs) acta_db_skill_revision_list_free(revs, count);
        return 0;
    }
    int max_idx = 0;
    for (int i = 1; i < count; i++)
        if (revs[i]->revision > revs[max_idx]->revision) max_idx = i;
    int result = (revs[max_idx]->deleted_at != NULL);
    acta_db_skill_revision_list_free(revs, count);
    return result;
}

#endif /* TEST_SKILL_HELPERS_H */
