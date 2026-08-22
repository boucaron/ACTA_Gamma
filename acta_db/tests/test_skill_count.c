/* test_skill_count.c — acta_db_skill_count */

#include "test_skill_helpers.h"

/* 7.43 */
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
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, &err), 5);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* 7.44 */
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
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, 0, &err), 2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* 7.45 */
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
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, f1, &err), 3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, f2, NULL), 1);

    test_db_teardown(db, path);
}

/* 7.46 */
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
    TEST_ASSERT_EQ_INT(acta_db_skill_count(db, -1, &err), 2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* 7.47 */
static void test_count_empty(void) {
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

/* 7.48 */
static void test_count_after_delete(void) {
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

/* 7.49 */
static void test_count_after_restore(void) {
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

/* 7.50 */
static void test_count_null_db(void) {
    int err = ACTA_DB_OK;
    int n = acta_db_skill_count(NULL, -1, &err);
    TEST_ASSERT_EQ_INT(n, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 7.51 */
static void test_count_null_err(void) {
    const char *path = "test/acta_test_sk_count_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    sk_create_skill(db, 0, "NoErr", "P", "{}");

    int n = acta_db_skill_count(db, -1, NULL);
    TEST_ASSERT_EQ_INT(n, 1);

    test_db_teardown(db, path);
}

/* 7.52 */
static void test_count_nonexistent_folder(void) {
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
    test_count_null_db();
    test_count_null_err();
    test_count_nonexistent_folder();
}
