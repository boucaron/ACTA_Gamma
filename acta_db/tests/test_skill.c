/* test_skill.c — Tests for skill.h (tests 7.1 – 7.23) */

#include "test_common.h"
#include "skill.h"
#include "skill_revision.h"

/* ---------- helpers ---------- */

static int sk_create_skill(db_t *db, int folder_id, const char *name,
                            const char *prompt, const char *schema) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name = (char *)name;
    s.description = (char *)"desc";
    s.prompt_template = (char *)prompt;
    s.output_schema = (char *)schema;
    s.folder_id = folder_id;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    return (rc == 0) ? id : -1;
}

static int sk_create_folder(db_t *db, const char *name, int parent_id) {
    int id = 0;
    int rc = acta_db_skill_folder_create(db, name, parent_id, &id);
    return (rc == 0) ? id : -1;
}

/* Count skill revisions for a given skill_id */
static int sk_count_revisions(db_t *db, int skill_id) {
    int count = 0;
    skill_revision_t *revs = acta_db_skill_revision_list_by_skill(db, skill_id, &count);
    if (revs) {
        acta_db_skill_revision_list_free(revs, count);
    }
    return count;
}

/* Get the latest revision number for a skill */
static int sk_latest_revision(db_t *db, int skill_id) {
    int count = 0;
    skill_revision_t *revs = acta_db_skill_revision_list_by_skill(db, skill_id, &count);
    if (!revs || count == 0) {
        return 0;
    }
    int max_rev = 0;
    for (int i = 0; i < count; i++) {
        if (revs[i].revision > max_rev) {
            max_rev = revs[i].revision;
        }
    }
    acta_db_skill_revision_list_free(revs, count);
    return max_rev;
}

/* Check if the latest revision has deleted_at set */
static int sk_latest_rev_deleted(db_t *db, int skill_id) {
    int count = 0;
    skill_revision_t *revs = acta_db_skill_revision_list_by_skill(db, skill_id, &count);
    if (!revs || count == 0) {
        acta_db_skill_revision_list_free(revs, count);
        return 0;
    }
    int max_idx = 0;
    for (int i = 1; i < count; i++) {
        if (revs[i].revision > revs[max_idx].revision) {
            max_idx = i;
        }
    }
    int result = (revs[max_idx].deleted_at != NULL);
    acta_db_skill_revision_list_free(revs, count);
    return result;
}

/* ---------- 7.1: create — root ---------- */
static void test_sk_create_root(void) {
    const char *path = "test/acta_test_sk_create_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RootSkill", "Tell me a joke", "{}");
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* ---------- 7.2: create — in folder ---------- */
static void test_sk_create_in_folder(void) {
    const char *path = "test/acta_test_sk_create_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "MyFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, folder_id, "ChildSkill", "Template", "{}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, folder_id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.3: create — initial revision auto-created ---------- */
static void test_sk_create_initial_revision(void) {
    const char *path = "test/acta_test_sk_create_rev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RevSkill", "Initial prompt", "{}");
    TEST_ASSERT(id > 0);

    int rev_count = sk_count_revisions(db, id);
    TEST_ASSERT_EQ_INT(rev_count, 1);

    int latest = sk_latest_revision(db, id);
    TEST_ASSERT_EQ_INT(latest, 1);

    test_db_teardown(db, path);
}

/* ---------- 7.4: create — NULL prompt_template ---------- */
static void test_sk_create_null_prompt(void) {
    const char *path = "test/acta_test_sk_create_null_prompt.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name = (char *)"NullPrompt";
    s.description = (char *)"desc";
    s.prompt_template = NULL;
    s.output_schema = (char *)("{}");
    s.folder_id = 0;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 7.5: create — NULL name ---------- */
static void test_sk_create_null_name(void) {
    const char *path = "test/acta_test_sk_create_null_name.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name = NULL;
    s.description = (char *)"desc";
    s.prompt_template = (char *)"Template";
    s.output_schema = (char *)("{}");
    s.folder_id = 0;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 7.6: create — invalid folder_id ---------- */
static void test_sk_create_invalid_folder(void) {
    const char *path = "test/acta_test_sk_create_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 99999, "OrphanSkill", "Template", "{}");
    TEST_ASSERT(id < 0);

    test_db_teardown(db, path);
}

/* ---------- 7.7: create — duplicate name (root) ---------- */
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

/* ---------- 7.8: create — duplicate name (child) ---------- */
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

/* ---------- 7.9: get — existing ---------- */
static void test_sk_get_existing(void) {
    const char *path = "test/acta_test_sk_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "GetSkill", "My prompt", "{\"out\":1}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->id, id);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    TEST_ASSERT_EQ_STR(s->name, "GetSkill");
    TEST_ASSERT_EQ_STR(s->prompt_template, "My prompt");
    TEST_ASSERT_EQ_STR(s->output_schema, "{\"out\":1}");
    TEST_ASSERT(s->deleted_at == NULL);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.10: get — non-existent ---------- */
static void test_sk_get_nonexistent(void) {
    const char *path = "test/acta_test_sk_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    skill_t *s = acta_db_skill_get(db, 999999);
    TEST_ASSERT_NULL(s);

    test_db_teardown(db, path);
}

/* ---------- 7.11: get_live — live ---------- */
static void test_sk_get_live_live(void) {
    const char *path = "test/acta_test_sk_live_ok.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "LiveSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get_live(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->id, id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.12: get_live — deleted ---------- */
static void test_sk_get_live_deleted(void) {
    const char *path = "test/acta_test_sk_live_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *s = acta_db_skill_get_live(db, id);
    TEST_ASSERT_NULL(s);

    test_db_teardown(db, path);
}

/* ---------- 7.13: update — change prompt_template ---------- */
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
    s.id = id;
    s.name = (char *)"UpdPrompt";
    s.description = (char *)"desc";
    s.prompt_template = (char *)"New prompt";
    s.output_schema = (char *)("{}");
    s.folder_id = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, 0);

    int rev_after = sk_count_revisions(db, id);
    TEST_ASSERT_EQ_INT(rev_after, rev_before + 1);

    /* Verify the new prompt is stored */
    skill_t *fetched = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQ_STR(fetched->prompt_template, "New prompt");
    acta_db_skill_free(fetched);

    test_db_teardown(db, path);
}

/* ---------- 7.14: update — change output_schema ---------- */
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
    s.id = id;
    s.name = (char *)"UpdSchema";
    s.description = (char *)"desc";
    s.prompt_template = (char *)"Prompt";
    s.output_schema = (char *)"NEW";
    s.folder_id = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, 0);

    int rev_after = sk_count_revisions(db, id);
    TEST_ASSERT_EQ_INT(rev_after, rev_before + 1);

    skill_t *fetched = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQ_STR(fetched->output_schema, "NEW");
    acta_db_skill_free(fetched);

    test_db_teardown(db, path);
}

/* ---------- 7.15: update — no change ---------- */
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
    s.id = id;
    s.name = (char *)"NoChange";
    s.description = (char *)"desc";
    s.prompt_template = (char *)"Same prompt";
    s.output_schema = (char *)("{}");
    s.folder_id = 0;

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, 0);

    int rev_after = sk_count_revisions(db, id);
    TEST_ASSERT_EQ_INT(rev_after, rev_before);

    test_db_teardown(db, path);
}

/* ---------- 7.16: update — on deleted skill ---------- */
static void test_sk_update_deleted(void) {
    const char *path = "test/acta_test_sk_upd_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelUpd", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int rev_before = sk_count_revisions(db, id);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id = id;
    s.name = (char *)"DelUpd";
    s.description = (char *)"desc";
    s.prompt_template = (char *)"New prompt after delete";
    s.output_schema = (char *)("{}");
    s.folder_id = 0;

    rc = acta_db_skill_update(db, &s);
    /* Should either fail or not create a new revision */
    if (rc == 0) {
        int rev_after = sk_count_revisions(db, id);
        TEST_ASSERT_EQ_INT(rev_after, rev_before);
    }

    test_db_teardown(db, path);
}

/* ---------- 7.17: soft_delete — happy ---------- */
static void test_sk_soft_delete_happy(void) {
    const char *path = "test/acta_test_sk_del_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelHappy", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(s->deleted_at);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.18: soft_delete — deleted revision created ---------- */
static void test_sk_soft_delete_revision(void) {
    const char *path = "test/acta_test_sk_del_rev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "DelRev", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rev_before = sk_count_revisions(db, id);

    int rc = acta_db_skill_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int rev_after = sk_count_revisions(db, id);
    TEST_ASSERT_EQ_INT(rev_after, rev_before + 1);

    int latest_del = sk_latest_rev_deleted(db, id);
    TEST_ASSERT(latest_del);

    test_db_teardown(db, path);
}

/* ---------- 7.20: list_in_folder — root ---------- */
static void test_sk_list_root(void) {
    const char *path = "test/acta_test_sk_list_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "RootA", "P1", "{}");
    int id2 = sk_create_skill(db, 0, "RootB", "P2", "{}");
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);

    /* Create a skill in a subfolder — should NOT appear in root list */
    int folder_id = sk_create_folder(db, "Sub", 0);
    sk_create_skill(db, folder_id, "ChildC", "P3", "{}");

    int count = 0;
    skill_t *items = acta_db_skill_list_in_folder(db, 0, &count);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    /* Verify both root skills are present */
    int found_a = 0, found_b = 0;
    for (int i = 0; i < count; i++) {
        if (items[i].id == id1) found_a = 1;
        if (items[i].id == id2) found_b = 1;
    }
    TEST_ASSERT(found_a);
    TEST_ASSERT(found_b);

    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 7.21: list_in_folder — specific ---------- */
static void test_sk_list_specific_folder(void) {
    const char *path = "test/acta_test_sk_list_specific.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder1 = sk_create_folder(db, "Folder1", 0);
    int folder2 = sk_create_folder(db, "Folder2", 0);
    TEST_ASSERT(folder1 > 0);
    TEST_ASSERT(folder2 > 0);

    int id1 = sk_create_skill(db, folder1, "InFolder1", "P1", "{}");
    int id2 = sk_create_skill(db, folder2, "InFolder2", "P2", "{}");
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);

    /* List folder1 — should only contain id1 */
    int count = 0;
    skill_t *items = acta_db_skill_list_in_folder(db, folder1, &count);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0].id, id1);
    TEST_ASSERT_EQ_INT(items[0].folder_id, folder1);
    acta_db_skill_list_free(items, count);

    /* List folder2 — should only contain id2 */
    count = 0;
    items = acta_db_skill_list_in_folder(db, folder2, &count);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0].id, id2);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 7.22: list_all — excludes deleted ---------- */
static void test_sk_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_sk_list_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "Alive1", "P1", "{}");
    int id2 = sk_create_skill(db, 0, "Alive2", "P2", "{}");
    int id3 = sk_create_skill(db, 0, "ToDelete", "P3", "{}");
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);
    TEST_ASSERT(id3 > 0);

    int rc = acta_db_skill_soft_delete(db, id3);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0;
    skill_t *items = acta_db_skill_list_all(db, &count);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    /* Ensure id3 is NOT in the list */
    for (int i = 0; i < count; i++) {
        TEST_ASSERT(items[i].id != id3);
    }

    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 7.23: free / list_free ---------- */
static void test_sk_free_and_list_free(void) {
    const char *path = "test/acta_test_sk_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* free(NULL) — no crash */
    acta_db_skill_free(NULL);
    TEST_ASSERT(1);

    /* list_free(NULL, 0) — no crash */
    acta_db_skill_list_free(NULL, 0);
    TEST_ASSERT(1);

    /* Valid free */
    int id = sk_create_skill(db, 0, "FreeSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    acta_db_skill_free(s);

    /* Valid list_free */
    int count = 0;
    skill_t *items = acta_db_skill_list_all(db, &count);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 7.24: restore — happy path ---------- */
static void test_sk_restore_happy(void) {
    const char *path = "test/acta_test_sk_restore_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RestoreSkill", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    /* Soft delete first */
    int rc = acta_db_skill_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *deleted = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(deleted);
    TEST_ASSERT_NOT_NULL(deleted->deleted_at);
    acta_db_skill_free(deleted);

    /* Now restore */
    rc = acta_db_skill_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *restored = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT(restored->deleted_at == NULL);
    acta_db_skill_free(restored);

    /* get_live should find it again */
    skill_t *live = acta_db_skill_get_live(db, id);
    TEST_ASSERT_NOT_NULL(live);
    acta_db_skill_free(live);

    test_db_teardown(db, path);
}

/* ---------- 7.25: restore — already live (no-op) ---------- */
static void test_sk_restore_already_live(void) {
    const char *path = "test/acta_test_sk_restore_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "AlreadyLive", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    /* Should succeed as a no-op (WHERE deleted_at IS NOT NULL matches nothing) */
    int rc = acta_db_skill_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(s->deleted_at == NULL);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.26: restore — non-existent id ---------- */
static void test_sk_restore_nonexistent(void) {
    const char *path = "test/acta_test_sk_restore_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* No row with id 999999 — UPDATE affects 0 rows, but rc is still SQLITE_DONE */
    /* So this returns 0 (success) but effectively a no-op */
    int rc = acta_db_skill_restore(db, 999999);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *s = acta_db_skill_get(db, 999999);
    TEST_ASSERT_NULL(s);

    test_db_teardown(db, path);
}

/* ---------- 7.27: move — to existing folder ---------- */
static void test_sk_move_to_folder_happy(void) {
    const char *path = "test/acta_test_sk_move_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "TargetFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, 0, "Movable", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_move_to_folder(db, id, folder_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, folder_id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.28: move — to root (folder_id = 0) ---------- */
static void test_sk_move_to_root(void) {
    const char *path = "test/acta_test_sk_move_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "OrigFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, folder_id, "MoveToRoot", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_move_to_folder(db, id, 0);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.29: move — invalid folder_id ---------- */
static void test_sk_move_invalid_folder(void) {
    const char *path = "test/acta_test_sk_move_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "BadMove", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_move_to_folder(db, id, 99999);
    TEST_ASSERT(rc < 0);

    /* Skill unchanged */
    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ---------- 7.30: move — deleted skill ---------- */
static void test_sk_move_deleted_skill(void) {
    const char *path = "test/acta_test_sk_move_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "DestFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, 0, "DelMove", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    int rc = acta_db_skill_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_move_to_folder(db, id, folder_id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 7.31: move — to deleted folder ---------- */
static void test_sk_move_to_deleted_folder(void) {
    const char *path = "test/acta_test_sk_move_del_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "WillDelete", 0);
    TEST_ASSERT(folder_id > 0);

    int rc = acta_db_skill_folder_soft_delete(db, folder_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int id = sk_create_skill(db, 0, "MoveDel", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    rc = acta_db_skill_move_to_folder(db, id, folder_id);
    TEST_ASSERT(rc < 0);

    skill_t *s = acta_db_skill_get(db, id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}


/* ---------- runner ---------- */
void run_skill_tests(void) {
    fprintf(stderr, "\n=== skill tests ===\n");
    test_sk_create_root();
    test_sk_create_in_folder();
    test_sk_create_initial_revision();
    test_sk_create_null_prompt();
    test_sk_create_null_name();
    test_sk_create_invalid_folder();
    test_sk_create_dup_root();
    test_sk_create_dup_child();
    test_sk_get_existing();
    test_sk_get_nonexistent();
    test_sk_get_live_live();
    test_sk_get_live_deleted();
    test_sk_update_prompt();
    test_sk_update_schema();
    test_sk_update_no_change();
    test_sk_update_deleted();
    test_sk_soft_delete_happy();
    test_sk_soft_delete_revision();
    test_sk_list_root();
    test_sk_list_specific_folder();
    test_sk_list_all_excludes_deleted();
    test_sk_free_and_list_free();
    test_sk_restore_happy();
    test_sk_restore_already_live();
    test_sk_restore_nonexistent();
    test_sk_move_to_folder_happy();
    test_sk_move_to_root();
    test_sk_move_invalid_folder();
    test_sk_move_deleted_skill();
    test_sk_move_to_deleted_folder();
}

