/* test_skill_pagination.c — offset/limit boundary tests for skill listers */

#include "test_skill_helpers.h"

/* ── tiny local helpers ─────────────────────────────────────────── */

/* Create `n` skills named "Pfx1", "Pfx2", … in folder_id.
 * Returns the first row id (caller may verify ordering). */
static int create_n_skills(db_t *db, int folder_id,
                           const char *prefix, int n)
{
    int first_id = -1;
    char buf[32];
    for (int i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "%s%d", prefix, i + 1);
        int id = sk_create_skill(db, folder_id, buf, "P", "{}");
        TEST_ASSERT(id > 0);
        if (i == 0) first_id = id;
    }
    return first_id;
}

/* ── list_in_folder pagination ──────────────────────────────────── */

/* 7.32 – first page: offset=0, limit < total */
static void test_list_in_folder_first_page(void) {
    const char *path = "test/acta_test_sk_list_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder", 0);
    TEST_ASSERT(folder_id > 0);
    create_n_skills(db, folder_id, "Skill", 5);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 0, 3,
                                                   &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_EQ_STR(items[0]->name, "Skill1");
    TEST_ASSERT_EQ_STR(items[1]->name, "Skill2");
    TEST_ASSERT_EQ_STR(items[2]->name, "Skill3");
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.33 – middle page: offset lands in the interior */
static void test_list_in_folder_middle_page(void) {
    const char *path = "test/acta_test_sk_list_offset_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder2", 0);
    TEST_ASSERT(folder_id > 0);
    int first_id = create_n_skills(db, folder_id, "Skill", 5);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 2, 3,
                                                   &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    /* ids are sequential from first_id */
    TEST_ASSERT_EQ_INT(items[0]->id, first_id + 2);
    TEST_ASSERT_EQ_INT(items[1]->id, first_id + 3);
    TEST_ASSERT_EQ_INT(items[2]->id, first_id + 4);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.34 – offset well beyond total: empty result, not an error */
static void test_list_in_folder_offset_beyond(void) {
    const char *path = "test/acta_test_sk_list_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "PagFolder3", 0);
    TEST_ASSERT(folder_id > 0);
    create_n_skills(db, folder_id, "Only", 2);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 100, 10,
                                                   &count, &err);
    /* empty is success: OK + NULL + count 0 */
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* 7.42 – limit exceeds total: return all, no error */
static void test_list_in_folder_limit_exceeds_total(void) {
    const char *path = "test/acta_test_sk_list_limit_exceed.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "SmallFolder", 0);
    TEST_ASSERT(folder_id > 0);
    create_n_skills(db, folder_id, "X", 2);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 0, 100,
                                                   &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.43 – exact boundary: offset == total_rows → empty */
static void test_list_in_folder_offset_equals_total(void) {
    const char *path = "test/acta_test_sk_list_offset_eq_total.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "BoundaryFolder", 0);
    TEST_ASSERT(folder_id > 0);
    create_n_skills(db, folder_id, "B", 3);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, 3, 10,
                                                   &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* 7.44 – limit == 1 (minimum page), walk all rows one at a time */
static void test_list_in_folder_limit_one(void) {
    const char *path = "test/acta_test_sk_list_limit1.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "Limit1Folder", 0);
    TEST_ASSERT(folder_id > 0);
    int first_id = create_n_skills(db, folder_id, "L", 5);

    for (int page = 0; page < 5; page++) {
        int count = 0, err = 0;
        skill_t **items = acta_db_skill_list_in_folder(db, folder_id,
                                                       page, 1,
                                                       &count, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        TEST_ASSERT_NOT_NULL(items);
        TEST_ASSERT_EQ_INT(count, 1);
        TEST_ASSERT_EQ_INT(items[0]->id, first_id + page);
        acta_db_skill_list_free(items, count);
    }

    /* one past the end → empty */
    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id,
                                                   5, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}


/* 7.45 – negative offset → ACTA_DB_ERR_INVALID */
static void test_list_in_folder_negative_offset(void) {
    const char *path = "test/acta_test_sk_list_negoffset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "NegOffFolder", 0);
    TEST_ASSERT(folder_id > 0);
    create_n_skills(db, folder_id, "N", 3);

    int count = -1, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, folder_id, -1, 10,
                                                   &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);

    test_db_teardown(db, path);
}

/* ── list_all pagination ────────────────────────────────────────── */

/* 7.35 – first page */
static void test_list_all_first_page(void) {
    const char *path = "test/acta_test_sk_all_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    create_n_skills(db, 0, "All", 5);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "All1");
    TEST_ASSERT_EQ_STR(items[1]->name, "All2");
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.36 – last page (offset near the end) */
static void test_list_all_last_page(void) {
    const char *path = "test/acta_test_sk_all_last_page.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int first_id = create_n_skills(db, 0, "All", 5);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 3, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->id, first_id + 3);
    TEST_ASSERT_EQ_INT(items[1]->id, first_id + 4);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.37 – limit == 0 means "no limit" */
static void test_list_all_limit_zero_no_limit(void) {
    const char *path = "test/acta_test_sk_all_nolimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    create_n_skills(db, 0, "NL", 5);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 5);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.38 – limit == -1 also means "no limit" */
static void test_list_all_limit_neg1_no_limit(void) {
    const char *path = "test/acta_test_sk_all_neglimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    create_n_skills(db, 0, "Neg", 3);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.41 – offset well beyond: empty */
static void test_list_all_offset_beyond(void) {
    const char *path = "test/acta_test_sk_all_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    create_n_skills(db, 0, "Beyond", 2);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 50, 10, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* 7.46 – negative offset → ACTA_DB_ERR_INVALID */
static void test_list_all_negative_offset(void) {
    const char *path = "test/acta_test_sk_all_negoffset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    create_n_skills(db, 0, "NO", 3);

    int count = -1, err = 0;
    skill_t **items = acta_db_skill_list_all(db, -5, 10, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);

    test_db_teardown(db, path);
}

/* 7.47 – out_count and err both NULL (nullable out-params) */
static void test_list_all_null_out_params(void) {
    const char *path = "test/acta_test_sk_all_nullparams.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    create_n_skills(db, 0, "NP", 3);

    /* no out_count, no err – should still work */
    skill_t **items = acta_db_skill_list_all(db, 0, 10, NULL, NULL);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_skill_list_free(items, 3);

    test_db_teardown(db, path);
}

/* ── runner ─────────────────────────────────────────────────────── */

void run_skill_pagination_tests(void) {
    fprintf(stderr, "\n=== skill_pagination tests ===\n");

    test_list_in_folder_first_page();
    test_list_in_folder_middle_page();
    test_list_in_folder_offset_beyond();
    test_list_in_folder_limit_exceeds_total();
    test_list_in_folder_offset_equals_total();
    test_list_in_folder_limit_one();
    test_list_in_folder_negative_offset();

    test_list_all_first_page();
    test_list_all_last_page();
    test_list_all_limit_zero_no_limit();
    test_list_all_limit_neg1_no_limit();
    test_list_all_offset_beyond();
    test_list_all_negative_offset();
    test_list_all_null_out_params();
}
