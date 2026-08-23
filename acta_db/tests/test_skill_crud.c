/* test_skill_crud.c — create, get, update, soft_delete, restore, free */

#include "test_skill_helpers.h"

/* ── create ─────────────────────────────────────────────────────── */

/* 7.1 */
static void test_create_root(void) {
    const char *path = "test/acta_test_sk_create_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RootSkill", "Tell me a joke", "{}");
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* 7.2 */
static void test_create_in_folder(void) {
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

/* 7.3 */
static void test_create_initial_revision(void) {
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

/* 7.4 */
static void test_create_null_prompt(void) {
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
    TEST_ASSERT_EQ_INT(acta_db_skill_create(db, &s, &id), ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 7.5 */
static void test_create_null_name(void) {
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
    TEST_ASSERT_EQ_INT(acta_db_skill_create(db, &s, &id), ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 7.6 */
static void test_create_invalid_folder(void) {
    const char *path = "test/acta_test_sk_create_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 99999, "OrphanSkill", "Template", "{}");
    TEST_ASSERT(id < 0);

    test_db_teardown(db, path);
}

/* 7.7 */
static void test_create_dup_root(void) {
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

/* 7.8 */
static void test_create_dup_child(void) {
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

/* ── get / get_live ─────────────────────────────────────────────── */

/* 7.9 */
static void test_get_existing(void) {
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

/* 7.10 */
static void test_get_nonexistent(void) {
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

/* 7.11 */
static void test_get_live_live(void) {
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

/* 7.12 */
static void test_get_live_deleted(void) {
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

/* ── update ─────────────────────────────────────────────────────── */

/* 7.13 */
static void test_update_prompt(void) {
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

    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before + 1);

    skill_t *fetched = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQ_STR(fetched->prompt_template, "New prompt");
    acta_db_skill_free(fetched);

    test_db_teardown(db, path);
}

/* 7.14 */
static void test_update_schema(void) {
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

    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before + 1);

    skill_t *fetched = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(fetched);
    TEST_ASSERT_EQ_STR(fetched->output_schema, "NEW");
    acta_db_skill_free(fetched);

    test_db_teardown(db, path);
}

/* 7.15 */
static void test_update_no_change(void) {
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

    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before);

    test_db_teardown(db, path);
}

/* 7.16 */
static void test_update_deleted(void) {
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

    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s), ACTA_DB_ERR_NOT_FOUND);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before);

    test_db_teardown(db, path);
}

/* ── soft_delete ────────────────────────────────────────────────── */

/* 7.17 */
static void test_soft_delete_happy(void) {
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

/* 7.18 */
static void test_soft_delete_revision(void) {
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

/* ── restore ────────────────────────────────────────────────────── */

/* 7.23 */
static void test_restore_happy(void) {
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

/* 7.24 */
static void test_restore_already_live(void) {
    const char *path = "test/acta_test_sk_restore_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "AlreadyLive", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_restore(db, id), ACTA_DB_OK);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(s->deleted_at == NULL);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}


/* 7.25 */
static void test_restore_nonexistent(void) {
    const char *path = "test/acta_test_sk_restore_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_restore(db, 999999), ACTA_DB_ERR_NOT_FOUND);
    TEST_ASSERT_NULL(acta_db_skill_get(db, 999999, NULL));

    test_db_teardown(db, path);
}

/* ── free ───────────────────────────────────────────────────────── */

/* 7.22 */
static void test_free_and_list_free(void) {
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

/* ── runner ─────────────────────────────────────────────────────── */

void run_skill_crud_tests(void) {
    fprintf(stderr, "\n=== skill_crud tests ===\n");

    test_create_root();
    test_create_in_folder();
    test_create_initial_revision();
    test_create_null_prompt();
    test_create_null_name();
    test_create_invalid_folder();
    test_create_dup_root();
    test_create_dup_child();

    test_get_existing();
    test_get_nonexistent();
    test_get_live_live();
    test_get_live_deleted();

    test_update_prompt();
    test_update_schema();
    test_update_no_change();
    test_update_deleted();

    test_soft_delete_happy();
    test_soft_delete_revision();

    test_restore_happy();
    test_restore_already_live();
    test_restore_nonexistent();

    test_free_and_list_free();
}
