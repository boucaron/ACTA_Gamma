/* test_skill.c — Tests for acta_db_skill.h (tests 7.1 – 7.52) */

#include "test_common.h"
#include "skill.h"
#include "skill_folder.h"
#include "skill_revision.h"
#include "db.h"

/* ---------- helpers ---------- */

static int sk_create_skill(db_t *db, int folder_id, const char *name,
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

static int sk_create_folder(db_t *db, const char *name, int parent_id) {
    int id = 0;
    int rc = acta_db_skill_folder_create(db, name, parent_id, &id);
    return (rc == ACTA_DB_OK) ? id : -1;
}

static int sk_count_revisions(db_t *db, int skill_id) {
    int count = 0;
    int err   = 0;
    skill_revision_t **revs = acta_db_skill_revision_list_by_skill(
        db, skill_id, 0, 0, &count, &err);
    if (revs) acta_db_skill_revision_list_free(revs, count);
    return count;
}

static int sk_latest_revision(db_t *db, int skill_id) {
    int count = 0;
    int err   = 0;
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

static int sk_latest_rev_deleted(db_t *db, int skill_id) {
    int count = 0;
    int err   = 0;
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

/* ═══════════════════════════════════════════════════════════════════
 *  7.1 – 7.8: create
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_create_root(void) {
    const char *path = "test/acta_test_sk_create_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RootSkill", "Tell me a joke", "{}");
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

static void test_sk_create_in_folder(void) {
    const char *path = "test/acta_test_sk_create_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "MyFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, folder_id, "ChildSkill", "Template", "{}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, folder_id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_create_initial_revision(void) {
    const char *path = "test/acta_test_sk_create_rev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RevSkill", "Initial prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), 1);
    TEST_ASSERT_EQ_INT(sk_latest_revision(db, id), 1);

    test_db_teardown(db, path);
}

static void test_sk_create_null_prompt(void) {
    const char *path = "test/acta_test_sk_create_null_prompt.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name            = (char *)"NullPrompt";
    s.description     = (char *)"desc";
    s.prompt_template = NULL;
    s.output_schema   = (char *)("{}");
    s.folder_id       = 0;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_sk_create_null_name(void) {
    const char *path = "test/acta_test_sk_create_null_name.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name            = NULL;
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"Template";
    s.output_schema   = (char *)("{}");
    s.folder_id       = 0;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_sk_create_invalid_folder(void) {
    const char *path = "test/acta_test_sk_create_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 99999, "OrphanSkill", "Template", "{}");
    TEST_ASSERT(id < 0);

    test_db_teardown(db, path);
}

static void test_sk_create_dup_root(void) {
    const char *path = "test/acta_test_sk_dup_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "DupName", "Prompt A", "{}");
    TEST_ASSERT(id1 > 0);

    int id2 = sk_create_skill(db, 0, "DupName", "Prompt B", "{}");
    TEST_ASSERT(id2 < 0);

    test_db_teardown(db, path);
}

static void test_sk_create_dup_child(void) {
    const char *path = "test/acta_test_sk_dup_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "DupFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id1 = sk_create_skill(db, folder_id, "ChildDup", "Prompt A", "{}");
    TEST_ASSERT(id1 > 0);

    int id2 = sk_create_skill(db, folder_id, "ChildDup", "Prompt B", "{}");
    TEST_ASSERT(id2 < 0);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.9 – 7.12: get / get_live
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_get_existing(void) {
    const char *path = "test/acta_test_sk_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "GetSkill", "My prompt", "{\"out\":1}");
    TEST_ASSERT(id > 0);

    int err = 0;
    skill_t *s = acta_db_skill_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(s->id, id);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    TEST_ASSERT_EQ_STR(s->name, "GetSkill");
    TEST_ASSERT_EQ_STR(s->prompt_template, "My prompt");
    TEST_ASSERT_EQ_STR(s->output_schema, "{\"out\":1}");
    TEST_ASSERT(s->deleted_at == NULL);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_get_nonexistent(void) {
    const char *path = "test/acta_test_sk_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    skill_t *s = acta_db_skill_get(db, 999999, &err);
    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_sk_get_live_live(void) {
    const char *path = "test/acta_test_sk_live_ok.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "LiveSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get_live(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->id, id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_get_live_deleted(void) {
    const char *path = "test/acta_test_sk_live_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);

    skill_t *s = acta_db_skill_get_live(db, id, NULL);
    TEST_ASSERT_NULL(s);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.13 – 7.16: update
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_update_prompt(void) {
    const char *path = "test/acta_test_sk_upd_prompt.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "UpdPrompt", "Old prompt", "{}");
    TEST_ASSERT(id > 0);

    int rev_before = sk_count_revisions(db, id);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id              = id;
    s.name            = (char *)"UpdPrompt";
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"New prompt";
    s.output_schema   = (char *)("{}");
    s.folder_id       = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before + 1);

    skill_t *fetched = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQ_STR(fetched->prompt_template, "New prompt");
    acta_db_skill_free(fetched);

    test_db_teardown(db, path);
}

static void test_sk_update_schema(void) {
    const char *path = "test/acta_test_sk_upd_schema.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "UpdSchema", "Prompt", "OLD");
    TEST_ASSERT(id > 0);

    int rev_before = sk_count_revisions(db, id);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id              = id;
    s.name            = (char *)"UpdSchema";
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"Prompt";
    s.output_schema   = (char *)"NEW";
    s.folder_id       = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before + 1);

    skill_t *fetched = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQ_STR(fetched->output_schema, "NEW");
    acta_db_skill_free(fetched);

    test_db_teardown(db, path);
}

static void test_sk_update_no_change(void) {
    const char *path = "test/acta_test_sk_upd_nochange.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "NoChange", "Same prompt", "{}");
    TEST_ASSERT(id > 0);

    int rev_before = sk_count_revisions(db, id);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id              = id;
    s.name            = (char *)"NoChange";
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"Same prompt";
    s.output_schema   = (char *)("{}");
    s.folder_id       = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before);

    test_db_teardown(db, path);
}

static void test_sk_update_deleted(void) {
    const char *path = "test/acta_test_sk_upd_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelUpd", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);

    int rev_before = sk_count_revisions(db, id);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id              = id;
    s.name            = (char *)"DelUpd";
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"New prompt after delete";
    s.output_schema   = (char *)("{}");
    s.folder_id       = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.17 – 7.18: soft_delete
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_soft_delete_happy(void) {
    const char *path = "test/acta_test_sk_del_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelHappy", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(s->deleted_at);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_soft_delete_revision(void) {
    const char *path = "test/acta_test_sk_del_rev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelRev", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rev_before = sk_count_revisions(db, id);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before + 1);
    TEST_ASSERT(sk_latest_rev_deleted(db, id));

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.19 – 7.21: listers (functional)
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_list_root(void) {
    const char *path = "test/acta_test_sk_list_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "RootA", "P1", "{}");
    int id2 = sk_create_skill(db, 0, "RootB", "P2", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0);

    int folder_id = sk_create_folder(db, "Sub", 0);
    sk_create_skill(db, folder_id, "ChildC", "P3", "{}");

    int count = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, 0, 0, -1, &count, NULL);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    int found_a = 0, found_b = 0;
    for (int i = 0; i < count; i++) {
        if (items[i]->id == id1) found_a = 1;
        if (items[i]->id == id2) found_b = 1;
    }
    TEST_ASSERT(found_a && found_b);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_specific_folder(void) {
    const char *path = "test/acta_test_sk_list_specific.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder1 = sk_create_folder(db, "Folder1", 0);
    int folder2 = sk_create_folder(db, "Folder2", 0);
    TEST_ASSERT(folder1 > 0 && folder2 > 0);

    int id1 = sk_create_skill(db, folder1, "InFolder1", "P1", "{}");
    int id2 = sk_create_skill(db, folder2, "InFolder2", "P2", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0);

    int count = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder1, 0, -1, &count, NULL);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    acta_db_skill_list_free(items, count);

    count = 0;
    items = acta_db_skill_list_in_folder(db, folder2, 0, -1, &count, NULL);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id2);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_sk_list_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "Alive1", "P1", "{}");
    int id2 = sk_create_skill(db, 0, "Alive2", "P2", "{}");
    int id3 = sk_create_skill(db, 0, "ToDelete", "P3", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0 && id3 > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id3), ACTA_DB_OK);

    int count = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, -1, &count, NULL);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++)
        TEST_ASSERT(items[i]->id != id3);

    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.22: free / list_free
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_free_and_list_free(void) {
    const char *path = "test/acta_test_sk_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    acta_db_skill_free(NULL);
    acta_db_skill_list_free(NULL, 0);

    int id = sk_create_skill(db, 0, "FreeSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    acta_db_skill_free(s);

    int count = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, -1, &count, NULL);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.23 – 7.25: restore
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_restore_happy(void) {
    const char *path = "test/acta_test_sk_restore_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RestoreSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);

    skill_t *deleted = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(deleted);
    TEST_ASSERT_NOT_NULL(deleted->deleted_at);
    acta_db_skill_free(deleted);

    TEST_ASSERT_EQ_INT(acta_db_skill_restore(db, id), ACTA_DB_OK);

    skill_t *restored = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT(restored->deleted_at == NULL);
    acta_db_skill_free(restored);

    skill_t *live = acta_db_skill_get_live(db, id, NULL);
    TEST_ASSERT_NOT_NULL(live);
    acta_db_skill_free(live);

    test_db_teardown(db, path);
}

static void test_sk_restore_already_live(void) {
    const char *path = "test/acta_test_sk_restore_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "AlreadyLive", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(s->deleted_at == NULL);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_restore_nonexistent(void) {
    const char *path = "test/acta_test_sk_restore_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_restore(db, 999999), ACTA_DB_ERR_NOT_FOUND);

    skill_t *s = acta_db_skill_get(db, 999999, NULL);
    TEST_ASSERT_NULL(s);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.26 – 7.30: move_to_folder
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_move_to_folder_happy(void) {
    const char *path = "test/acta_test_sk_move_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "TargetFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, 0, "Movable", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, folder_id), ACTA_DB_OK);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, folder_id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_move_to_root(void) {
    const char *path = "test/acta_test_sk_move_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "OrigFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, folder_id, "MoveToRoot", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 0), ACTA_DB_OK);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_move_invalid_folder(void) {
    const char *path = "test/acta_test_sk_move_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "BadMove", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 99999),
                       ACTA_DB_ERR_NOT_FOUND);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

static void test_sk_move_deleted_skill(void) {
    const char *path = "test/acta_test_sk_move_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "DestFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, 0, "DelMove", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, folder_id),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

static void test_sk_move_to_deleted_folder(void) {
    const char *path = "test/acta_test_sk_move_del_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "WillDelete", 0);
    TEST_ASSERT(folder_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, folder_id), ACTA_DB_OK);

    int id = sk_create_skill(db, 0, "MoveDel", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, folder_id),
                       ACTA_DB_ERR_NOT_FOUND);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.31 – 7.42: pagination
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sk_list_in_folder_limit(void) {
    const char *path = "test/acta_test_sk_list_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int ids[5];
    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Skill%da", i + 1);
        ids[i] = sk_create_skill(db, folder_id, name, "P", "{}");
        TEST_ASSERT(ids[i] > 0);
    }

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 0, 3, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_EQ_STR(items[0]->name, "Skill1a");
    TEST_ASSERT_EQ_STR(items[1]->name, "Skill2a");
    TEST_ASSERT_EQ_STR(items[2]->name, "Skill3a");
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_in_folder_offset_limit(void) {
    const char *path = "test/acta_test_sk_list_offset_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder2", 0);
    TEST_ASSERT(folder_id > 0);

    int ids[5];
    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Skill%da", i + 1);
        ids[i] = sk_create_skill(db, folder_id, name, "P", "{}");
        TEST_ASSERT(ids[i] > 0);
    }

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 2, 3, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[2]);
    TEST_ASSERT_EQ_INT(items[1]->id, ids[3]);
    TEST_ASSERT_EQ_INT(items[2]->id, ids[4]);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_in_folder_offset_beyond(void) {
    const char *path = "test/acta_test_sk_list_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder3", 0);
    TEST_ASSERT(folder_id > 0);

    sk_create_skill(db, folder_id, "Only1", "P", "{}");
    sk_create_skill(db, folder_id, "Only2", "P", "{}");

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 100, 10, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

static void test_sk_list_all_limit(void) {
    const char *path = "test/acta_test_sk_all_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "All%da", i + 1);
        TEST_ASSERT(sk_create_skill(db, 0, name, "P", "{}") > 0);
    }

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "All1a");
    TEST_ASSERT_EQ_STR(items[1]->name, "All2a");
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_all_last_page(void) {
    const char *path = "test/acta_test_sk_all_last_page.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ids[5];
    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "All%da", i + 1);
        ids[i] = sk_create_skill(db, 0, name, "P", "{}");
        TEST_ASSERT(ids[i] > 0);
    }

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 3, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[3]);
    TEST_ASSERT_EQ_INT(items[1]->id, ids[4]);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_all_no_limit(void) {
    const char *path = "test/acta_test_sk_all_nolimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "NL%da", i + 1);
        TEST_ASSERT(sk_create_skill(db, 0, name, "P", "{}") > 0);
    }

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 5);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_all_neg_limit(void) {
    const char *path = "test/acta_test_sk_all_neglimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "NegA", "P", "{}");
    sk_create_skill(db, 0, "NegB", "P", "{}");
    sk_create_skill(db, 0, "NegC", "P", "{}");

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_folder_list_children_limit(void) {
    const char *path = "test/acta_test_sk_folder_children_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = sk_create_folder(db, "Parent", 0);
    TEST_ASSERT(parent_id > 0);

    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%d", i + 1);
        TEST_ASSERT(sk_create_folder(db, name, parent_id) > 0);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(
        db, parent_id, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Child1");
    TEST_ASSERT_EQ_STR(items[1]->name, "Child2");
    acta_db_skill_folder_list_free(items, count);

    count = 0;
    items = acta_db_skill_folder_list_children(db, parent_id, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Child3");
    TEST_ASSERT_EQ_STR(items[1]->name, "Child4");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_folder_list_children_root_offset(void) {
    const char *path = "test/acta_test_sk_folder_root_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_folder(db, "RootA", 0);
    sk_create_folder(db, "RootB", 0);
    sk_create_folder(db, "RootC", 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(
        db, 0, 1, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "RootB");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_sk_list_all_offset_beyond(void) {
    const char *path = "test/acta_test_sk_all_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "Beyond1", "P", "{}");
    sk_create_skill(db, 0, "Beyond2", "P", "{}");

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 50, 10, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

static void test_sk_list_in_folder_limit_exceeds_total(void) {
    const char *path = "test/acta_test_sk_list_limit_exceed.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "SmallFolder", 0);
    TEST_ASSERT(folder_id > 0);

    sk_create_skill(db, folder_id, "X1", "P", "{}");
    sk_create_skill(db, folder_id, "X2", "P", "{}");

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 0, 100, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  7.43 – 7.52: count
 * ═══════════════════════════════════════════════════════════════════ */

/* 7.43: count — all skills (folder_id < 0), mixed root + folders */
static void test_sk_count_all(void) {
    const char *path = "test/acta_test_sk_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* 2 root + 2 in folder1 + 1 in folder2 = 5 total */
    sk_create_skill(db, 0, "Root1", "P", "{}");
    sk_create_skill(db, 0, "Root2", "P", "{}");

    int f1 = sk_create_folder(db, "F1", 0);
    int f2 = sk_create_folder(db, "F2", 0);
    TEST_ASSERT(f1 > 0 && f2 > 0);

    sk_create_skill(db, f1, "InF1a", "P", "{}");
    sk_create_skill(db, f1, "InF1b", "P", "{}");
    sk_create_skill(db, f2, "InF2a", "P", "{}");

    int err = 0;
    int total = acta_db_skill_count(db, -1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 5);

    test_db_teardown(db, path);
}

/* 7.44: count — root level only (folder_id == 0) */
static void test_sk_count_root_only(void) {
    const char *path = "test/acta_test_sk_count_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "RootA", "P", "{}");
    sk_create_skill(db, 0, "RootB", "P", "{}");

    int f1 = sk_create_folder(db, "F1", 0);
    sk_create_skill(db, f1, "ChildA", "P", "{}");

    int err = 0;
    int n = acta_db_skill_count(db, 0, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);  /* only root, not the child */

    test_db_teardown(db, path);
}

/* 7.45: count — specific folder (folder_id > 0) */
static void test_sk_count_specific_folder(void) {
    const char *path = "test/acta_test_sk_count_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int f1 = sk_create_folder(db, "F1", 0);
    int f2 = sk_create_folder(db, "F2", 0);
    TEST_ASSERT(f1 > 0 && f2 > 0);

    sk_create_skill(db, f1, "A1", "P", "{}");
    sk_create_skill(db, f1, "A2", "P", "{}");
    sk_create_skill(db, f1, "A3", "P", "{}");
    sk_create_skill(db, f2, "B1", "P", "{}");

    int err = 0;

    int n1 = acta_db_skill_count(db, f1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n1, 3);

    n1 = acta_db_skill_count(db, f2, NULL);
    TEST_ASSERT_EQ_INT(n1, 1);

    test_db_teardown(db, path);
}

/* 7.46: count — excludes soft-deleted */
static void test_sk_count_excludes_deleted(void) {
    const char *path = "test/acta_test_sk_count_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "Alive1", "P", "{}");
    int id2 = sk_create_skill(db, 0, "Alive2", "P", "{}");
    int id3 = sk_create_skill(db, 0, "Gone", "P", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0 && id3 > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id3), ACTA_DB_OK);

    int err = 0;
    int n = acta_db_skill_count(db, -1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);

    test_db_teardown(db, path);
}

/* 7.47: count — empty database returns 0 */
static void test_sk_count_empty(void) {
    const char *path = "test/acta_test_sk_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, &err), 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, 0, NULL), 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, 999, NULL), 0);

    test_db_teardown(db, path);
}

/* 7.48: count — after soft_delete decreases */
static void test_sk_count_after_delete(void) {
    const char *path = "test/acta_test_sk_count_after_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "A", "P", "{}");
    int id2 = sk_create_skill(db, 0, "B", "P", "{}");
    int id3 = sk_create_skill(db, 0, "C", "P", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0 && id3 > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, NULL), 3);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, NULL), 2);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, NULL), 1);

    test_db_teardown(db, path);
}

/* 7.49: count — after restore increases */
static void test_sk_count_after_restore(void) {
    const char *path = "test/acta_test_sk_count_after_restore.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RestoreMe", "P", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, NULL), 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_restore(db, id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, NULL), 1);

    test_db_teardown(db, path);
}

/* 7.50: count — NULL db returns -1 + ERR_INVALID */
static void test_sk_count_null_db(void) {
    int err = ACTA_DB_OK;
    int n = acta_db_skill_count(NULL, -1, &err);
    TEST_ASSERT_EQ_INT(n, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 7.51: count — err pointer may be NULL (no crash) */
static void test_sk_count_null_err(void) {
    const char *path = "test/acta_test_sk_count_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "NoErr", "P", "{}");

    /* Must not crash; return value is the count. */
    int n = acta_db_skill_count(db, -1, NULL);
    TEST_ASSERT_EQ_INT(n, 1);

    test_db_teardown(db, path);
}

/* 7.52: count — folder_id for non-existent folder returns 0 */
static void test_sk_count_nonexistent_folder(void) {
    const char *path = "test/acta_test_sk_count_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "RootSkill", "P", "{}");

    int err = 0;
    int n = acta_db_skill_count(db, 99999, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Runner
 * ═══════════════════════════════════════════════════════════════════ */

void run_skill_tests(void) {
    fprintf(stderr, "\n=== skill tests ===\n");

    /* create */
    test_sk_create_root();
    test_sk_create_in_folder();
    test_sk_create_initial_revision();
    test_sk_create_null_prompt();
    test_sk_create_null_name();
    test_sk_create_invalid_folder();
    test_sk_create_dup_root();
    test_sk_create_dup_child();

    /* get / get_live */
    test_sk_get_existing();
    test_sk_get_nonexistent();
    test_sk_get_live_live();
    test_sk_get_live_deleted();

    /* update */
    test_sk_update_prompt();
    test_sk_update_schema();
    test_sk_update_no_change();
    test_sk_update_deleted();

    /* soft_delete */
    test_sk_soft_delete_happy();
    test_sk_soft_delete_revision();

    /* listers (functional) */
    test_sk_list_root();
    test_sk_list_specific_folder();
    test_sk_list_all_excludes_deleted();

    /* free */
    test_sk_free_and_list_free();

    /* restore */
    test_sk_restore_happy();
    test_sk_restore_already_live();
    test_sk_restore_nonexistent();

    /* move_to_folder */
    test_sk_move_to_folder_happy();
    test_sk_move_to_root();
    test_sk_move_invalid_folder();
    test_sk_move_deleted_skill();
    test_sk_move_to_deleted_folder();

    /* pagination */
    test_sk_list_in_folder_limit();
    test_sk_list_in_folder_offset_limit();
    test_sk_list_in_folder_offset_beyond();
    test_sk_list_all_limit();
    test_sk_list_all_last_page();
    test_sk_list_all_no_limit();
    test_sk_list_all_neg_limit();
    test_sk_folder_list_children_limit();
    test_sk_folder_list_children_root_offset();
    test_sk_list_all_offset_beyond();
    test_sk_list_in_folder_limit_exceeds_total();

    /* count */
    test_sk_count_all();
    test_sk_count_root_only();
    test_sk_count_specific_folder();
    test_sk_count_excludes_deleted();
    test_sk_count_empty();
    test_sk_count_after_delete();
    test_sk_count_after_restore();
    test_sk_count_null_db();
    test_sk_count_null_err();
    test_sk_count_nonexistent_folder();
}
