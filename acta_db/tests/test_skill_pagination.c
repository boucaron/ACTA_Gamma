/* test_skill_pagination.c — offset/limit boundary tests for skill listers */

#include "test_skill_helpers.h"

/* ── list_in_folder pagination ──────────────────────────────────── */

/* 7.32 */
static void test_list_in_folder_first_page(void) {
    const char *path = "test/acta_test_sk_list_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder", 0);
    TEST_ASSERT(folder_id > 0);

    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Skill%da", i + 1);
        TEST_ASSERT(sk_create_skill(db, folder_id, name, "P", "{}") > 0);
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

/* 7.33 */
static void test_list_in_folder_middle_page(void) {
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

/* 7.34 */
static void test_list_in_folder_offset_beyond(void) {
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

/* 7.42 */
static void test_list_in_folder_limit_exceeds_total(void) {
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

/* ── list_all pagination ────────────────────────────────────────── */

/* 7.35 */
static void test_list_all_first_page(void) {
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

/* 7.36 */
static void test_list_all_last_page(void) {
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

/* 7.37 */
static void test_list_all_limit_zero_no_limit(void) {
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

/* 7.38 */
static void test_list_all_limit_neg1_no_limit(void) {
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

/* 7.41 */
static void test_list_all_offset_beyond(void) {
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

/* ── runner ─────────────────────────────────────────────────────── */

void run_skill_pagination_tests(void) {
    fprintf(stderr, "\n=== skill_pagination tests ===\n");

    test_list_in_folder_first_page();
    test_list_in_folder_middle_page();
    test_list_in_folder_offset_beyond();
    test_list_in_folder_limit_exceeds_total();

    test_list_all_first_page();
    test_list_all_last_page();
    test_list_all_limit_zero_no_limit();
    test_list_all_limit_neg1_no_limit();
    test_list_all_offset_beyond();
}
