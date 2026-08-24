/* test_skill_crud.c — create, get, update, soft_delete, restore, free */

#include "test_skill_helpers.h"

/* ── helper: build a full skill_t for update calls ──────────────── */

static skill_t mk_skill(int id, int folder_id,
                        const char *name, const char *desc,
                        const char *prompt, const char *schema)
{
    skill_t s = {0};
    s.id              = id;
    s.folder_id       = folder_id;
    s.name            = name;
    s.description     = desc;
    s.prompt_template = prompt;
    s.output_schema   = schema;
    return s;
}

/* ═══════════════════════════════════════════════════════════════════
 *  create
 * ═══════════════════════════════════════════════════════════════════ */

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

    skill_t s = {0};
    s.name            = "NullPrompt";
    s.prompt_template = NULL;

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

    skill_t s = {0};
    s.name            = NULL;
    s.prompt_template = "Template";

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
    TEST_ASSERT_EQ_INT(id, ACTA_DB_ERR_NOT_FOUND);

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

/* ═══════════════════════════════════════════════════════════════════
 *  get / get_live
 * ═══════════════════════════════════════════════════════════════════ */

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

/* 7.26 (new) */
static void test_get_invalid_id(void) {
    const char *path = "test/acta_test_sk_get_badid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_NULL(acta_db_skill_get(db, 0, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(acta_db_skill_get(db, -1, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 7.27 (new) */
static void test_get_live_invalid_id(void) {
    const char *path = "test/acta_test_sk_live_badid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_NULL(acta_db_skill_get_live(db, 0, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  update
 * ═══════════════════════════════════════════════════════════════════ */

/* 7.13 */
static void test_update_prompt(void) {
    const char *path = "test/acta_test_sk_upd_prompt.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "UpdPrompt", "Old prompt", "{}");
    TEST_ASSERT(id > 0);

    int rev_before = sk_count_revisions(db, id);

    skill_t s = mk_skill(id, 0, "UpdPrompt", "desc", "New prompt", "{}");
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

    skill_t s = mk_skill(id, 0, "UpdSchema", "desc", "Prompt", "NEW");
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

    skill_t s = mk_skill(id, 0, "NoChange", "desc", "Same prompt", "{}");
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

    skill_t s = mk_skill(id, 0, "DelUpd", "desc", "New prompt after delete", "{}");
    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s), ACTA_DB_ERR_NOT_FOUND);
    TEST_ASSERT_EQ_INT(sk_count_revisions(db, id), rev_before);

    test_db_teardown(db, path);
}

/* 7.28 (new) */
static void test_update_null_args(void) {
    const char *path = "test/acta_test_sk_upd_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    skill_t s = mk_skill(1, 0, "X", "d", "p", "{}");
    TEST_ASSERT_EQ_INT(acta_db_skill_update(NULL, &s), ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, NULL), ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  soft_delete
 * ═══════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════
 *  move_to_folder
 * ═══════════════════════════════════════════════════════════════════ */

/* 7.29 (new) */
static void test_move_to_folder_happy(void) {
    const char *path = "test/acta_test_sk_move_ok.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "Target", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, 0, "Mover", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, folder_id),
                       ACTA_DB_OK);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, folder_id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* 7.30 (new) */
static void test_move_to_root(void) {
    const char *path = "test/acta_test_sk_move_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "Src", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, folder_id, "ToRoot", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 0), ACTA_DB_OK);

    skill_t *s = acta_db_skill_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* 7.31 (new) */
static void test_move_deleted_skill(void) {
    const char *path = "test/acta_test_sk_move_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "MoveDel", "Prompt", "{}");
    TEST_ASSERT(id > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 0),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 7.32 (new) */
static void test_move_bad_folder(void) {
    const char *path = "test/acta_test_sk_move_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "MoveBad", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 99999),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  restore
 * ═══════════════════════════════════════════════════════════════════ */

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

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  listers
 * ═══════════════════════════════════════════════════════════════════ */

/* 7.33 (new) */
static void test_list_negative_offset(void) {
    const char *path = "test/acta_test_sk_list_negoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "A", "p", "{}");

    int err = 0;
    TEST_ASSERT_NULL(acta_db_skill_list_in_folder(db, 0, -1, 10, NULL, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    TEST_ASSERT_NULL(acta_db_skill_list_all(db, -1, 10, NULL, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 7.34 (new) */
static void test_list_pagination(void) {
    const char *path = "test/acta_test_sk_list_page.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Skill%d", i);
        TEST_ASSERT(sk_create_skill(db, 0, name, "p", "{}") > 0);
    }

    int count = 0, err = 0;
    skill_t **page1 = acta_db_skill_list_all(db, 0, 3, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);

    skill_t **page2 = acta_db_skill_list_all(db, 3, 3, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    /* no overlap: last of page1 != first of page2 */
    TEST_ASSERT(page1[2]->id != page2[0]->id);

    acta_db_skill_list_free(page1, 3);
    acta_db_skill_list_free(page2, 2);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  count
 * ═══════════════════════════════════════════════════════════════════ */

/* 7.35 (new) */
static void test_count_all(void) {
    const char *path = "test/acta_test_sk_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 0);

    TEST_ASSERT(sk_create_skill(db, 0, "A", "p", "{}") > 0);
    TEST_ASSERT(sk_create_skill(db, 0, "B", "p", "{}") > 0);
    TEST_ASSERT(sk_create_skill(db, 0, "C", "p", "{}") > 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, &err), 3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* soft-deleted should not be counted */
    skill_t **items = acta_db_skill_list_all(db, 0, -1, NULL, NULL);
    if (items) acta_db_skill_soft_delete(db, items[0]->id);

    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 2);

    test_db_teardown(db, path);
}

/* 7.36 (new) */
static void test_count_in_folder(void) {
    const char *path = "test/acta_test_sk_count_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int f1 = sk_create_folder(db, "F1", 0);
    int f2 = sk_create_folder(db, "F2", 0);

    TEST_ASSERT(sk_create_skill(db, 0,  "Root", "p", "{}") > 0);
    TEST_ASSERT(sk_create_skill(db, f1, "InF1", "p", "{}") > 0);
    TEST_ASSERT(sk_create_skill(db, f2, "InF2", "p", "{}") > 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 0,  &err), 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f1, &err), 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f2, &err), 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, &err), 3);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  free
 * ═══════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════
 *  runner
 * ═══════════════════════════════════════════════════════════════════ */

void run_skill_crud_tests(void) {
    fprintf(stderr, "\n=== skill_crud tests ===\n");

    /* create */
    test_create_root();
    test_create_in_folder();
    test_create_initial_revision();
    test_create_null_prompt();
    test_create_null_name();
    test_create_invalid_folder();
    test_create_dup_root();
    test_create_dup_child();

    /* get */
    test_get_existing();
    test_get_nonexistent();
    test_get_live_live();
    test_get_live_deleted();
    test_get_invalid_id();
    test_get_live_invalid_id();

    /* update */
    test_update_prompt();
    test_update_schema();
    test_update_no_change();
    test_update_deleted();
    test_update_null_args();

    /* soft_delete */
    test_soft_delete_happy();
    test_soft_delete_revision();

    /* move_to_folder */
    test_move_to_folder_happy();
    test_move_to_root();
    test_move_deleted_skill();
    test_move_bad_folder();

    /* restore */
    test_restore_happy();
    test_restore_already_live();
    test_restore_nonexistent();

    /* listers */
    test_list_negative_offset();
    test_list_pagination();

    /* count */
    test_count_all();
    test_count_in_folder();

    /* free */
    test_free_and_list_free();
}
