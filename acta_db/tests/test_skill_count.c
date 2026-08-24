/* test_skill_count.c — acta_db_skill_count_in_folder / acta_db_skill_count_all */

#include "test_skill_helpers.h"

static void test_count_all(void) {
    const char *path = "test/acta_test_sk_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "Root1", "P", "{}");
    sk_create_skill(db, 0, "Root2", "P", "{}");

    int f1 = sk_create_folder(db, "F1", 0);
    int f2 = sk_create_folder(db, "F2", 0);
    TEST_ASSERT(f1 > 0 && f2 > 0);

    sk_create_skill(db, f1, "InF1a", "P", "{}");
    sk_create_skill(db, f1, "InF1b", "P", "{}");
    sk_create_skill(db, f2, "InF2a", "P", "{}");

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, &err), 5);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_count_root_only(void) {
    const char *path = "test/acta_test_sk_count_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "RootA", "P", "{}");
    sk_create_skill(db, 0, "RootB", "P", "{}");

    int f1 = sk_create_folder(db, "F1", 0);
    sk_create_skill(db, f1, "ChildA", "P", "{}");

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 0, &err), 2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_count_specific_folder(void) {
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
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f1, &err), 3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f2, &err), 1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_count_excludes_deleted(void) {
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
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, &err), 2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_count_empty(void) {
    const char *path = "test/acta_test_sk_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, &err), 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 0, &err), 0);
    /* Folder that has never had any skills. */
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 999, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_count_after_delete(void) {
    const char *path = "test/acta_test_sk_count_after_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "A", "P", "{}");
    int id2 = sk_create_skill(db, 0, "B", "P", "{}");
    int id3 = sk_create_skill(db, 0, "C", "P", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0 && id3 > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 3);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 2);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 1);

    test_db_teardown(db, path);
}

static void test_count_after_restore(void) {
    const char *path = "test/acta_test_sk_count_after_restore.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "RestoreMe", "P", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_restore(db, id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 1);

    test_db_teardown(db, path);
}

static void test_count_move_shifts_folders(void) {
    const char *path = "test/acta_test_sk_count_move.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int f1 = sk_create_folder(db, "F1", 0);
    int f2 = sk_create_folder(db, "F2", 0);
    TEST_ASSERT(f1 > 0 && f2 > 0);

    sk_create_skill(db, f1, "InF1a", "P", "{}");
    sk_create_skill(db, f1, "InF1b", "P", "{}");
    sk_create_skill(db, f2, "InF2a", "P", "{}");

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f1, &err), 2);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f2, &err), 1);

    /* Move one skill from f1 → f2. */
    int n = 0;
    skill_t **s = acta_db_skill_list_in_folder(db, f1, 0, 1, &n, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, s[0]->id, f2), ACTA_DB_OK);
    acta_db_skill_list_free(s, n);

    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f1, NULL), 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, f2, NULL), 2);
    /* Total unchanged. */
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 3);

    test_db_teardown(db, path);
}


static void test_count_null_db(void) {
    int err = ACTA_DB_OK;

    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(NULL, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(NULL, 0, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_count_null_err(void) {
    const char *path = "test/acta_test_sk_count_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "NoErr", "P", "{}");

    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, NULL), 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 0, NULL), 1);

    test_db_teardown(db, path);
}

static void test_count_nonexistent_folder(void) {
    const char *path = "test/acta_test_sk_count_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "RootSkill", "P", "{}");

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 99999, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ── runner ─────────────────────────────────────────────────────── */

void run_skill_count_tests(void) {
    fprintf(stderr, "\n=== skill_count tests ===\n");

    test_count_all();
    test_count_root_only();
    test_count_specific_folder();
    test_count_excludes_deleted();
    test_count_empty();
    test_count_after_delete();
    test_count_after_restore();
    test_count_move_shifts_folders();
    test_count_null_db();
    test_count_null_err();
    test_count_nonexistent_folder();
}
