/* test_skill_deleted.c — skill listers/counters that include soft-deleted rows:
 *
 *   acta_db_skill_list_in_folder_with_deleted
 *   acta_db_skill_list_all_with_deleted
 *   acta_db_skill_count_in_folder_with_deleted
 *   acta_db_skill_count_all_with_deleted
 *
 * Mirrors the with_deleted test sections in test_model.c.
 */

#include "test_skill_helpers.h"

/* ---------- list_in_folder_with_deleted — includes soft-deleted (root) ---------- */
static void test_list_in_folder_with_deleted_root(void) {
    const char *path = "test/acta_test_sk_lwfd_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "A", "P", "{}");
    int id2 = sk_create_skill(db, 0, "B", "P", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id1), ACTA_DB_OK);

    /* Live-only lister still excludes the deleted row */
    int err = 0;
    int count = 0;
    skill_t **live = acta_db_skill_list_in_folder(db, 0, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(live[0]->name, "B");
    acta_db_skill_list_free(live, count);

    /* With-deleted lister returns both, in id order, with deleted_at set */
    count = 0;
    err = 0;
    skill_t **items = acta_db_skill_list_in_folder_with_deleted(db, 0, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "A");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_STR(items[1]->name, "B");
    TEST_ASSERT_NULL(items[1]->deleted_at);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- list_in_folder_with_deleted — specific folder ---------- */
static void test_list_in_folder_with_deleted_folder(void) {
    const char *path = "test/acta_test_sk_lwfd_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int fid = sk_create_folder(db, "F", 0);
    TEST_ASSERT(fid > 0);

    int id1 = sk_create_skill(db, fid, "InF", "P", "{}");
    int id2 = sk_create_skill(db, 0, "Root", "P", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id1), ACTA_DB_OK);

    /* Folder view: deleted folder skill is the only row, visible here */
    int err = 0;
    int count = 0;
    skill_t **items = acta_db_skill_list_in_folder_with_deleted(db, fid, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "InF");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    acta_db_skill_list_free(items, count);

    /* Root view: live root skill only (the deleted one lives in the folder) */
    count = 0;
    err = 0;
    items = acta_db_skill_list_in_folder_with_deleted(db, 0, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Root");
    TEST_ASSERT_NULL(items[0]->deleted_at);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- list_in_folder_with_deleted — empty ---------- */
static void test_list_in_folder_with_deleted_empty(void) {
    const char *path = "test/acta_test_sk_lwfd_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    int err = 0;
    skill_t **items = acta_db_skill_list_in_folder_with_deleted(db, 0, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- list_in_folder_with_deleted — invalid args ---------- */
static void test_list_in_folder_with_deleted_invalid_args(void) {
    const char *path = "test/acta_test_sk_lwfd_args.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    int count = 0;

    /* NULL db */
    skill_t **items = acta_db_skill_list_in_folder_with_deleted(NULL, 0, 0, -1, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    /* Negative offset */
    err = 0;
    items = acta_db_skill_list_in_folder_with_deleted(db, 0, -1, -1, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- list_in_folder_with_deleted — pagination over mixed rows ---------- */
static void test_list_in_folder_with_deleted_pagination(void) {
    const char *path = "test/acta_test_sk_lwfd_page.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "S%d", i);
        int id = sk_create_skill(db, 0, name, "P", "{}");
        TEST_ASSERT(id > 0);
        if (i % 2 == 0)
            TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK); /* delete S0, S2 */
    }

    int err = 0;
    int count = 0;

    /* Page 1: S0 (deleted), S1 (live) */
    skill_t **items = acta_db_skill_list_in_folder_with_deleted(db, 0, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "S0");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_STR(items[1]->name, "S1");
    TEST_ASSERT_NULL(items[1]->deleted_at);
    acta_db_skill_list_free(items, count);

    /* Page 2: S2 (deleted), S3 (live) */
    count = 0;
    err = 0;
    items = acta_db_skill_list_in_folder_with_deleted(db, 0, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "S2");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_STR(items[1]->name, "S3");
    TEST_ASSERT_NULL(items[1]->deleted_at);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- list_in_folder_with_deleted — NULL out-params ---------- */
static void test_list_in_folder_with_deleted_null_outs(void) {
    const char *path = "test/acta_test_sk_lwfd_nullouts.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "S", "P", "{}");
    TEST_ASSERT(id > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK);

    /* Both out_count and err NULL: must not crash */
    skill_t **items = acta_db_skill_list_in_folder_with_deleted(db, 0, 0, -1, NULL, NULL);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_skill_list_free(items, 1);

    test_db_teardown(db, path);
}

/* ---------- list_all_with_deleted — includes soft-deleted from all folders ---------- */
static void test_list_all_with_deleted_all_folders(void) {
    const char *path = "test/acta_test_sk_lawd_allfolders.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int fid = sk_create_folder(db, "F", 0);
    TEST_ASSERT(fid > 0);

    int idA = sk_create_skill(db, 0, "A", "P", "{}");
    int idB = sk_create_skill(db, fid, "B", "P", "{}");
    int idC = sk_create_skill(db, 0, "C", "P", "{}");
    TEST_ASSERT(idA > 0 && idB > 0 && idC > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, idA), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, idB), ACTA_DB_OK);

    /* Live-only lister still excludes both deleted rows */
    int err = 0;
    int count = 0;
    skill_t **live = acta_db_skill_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(live[0]->name, "C");
    acta_db_skill_list_free(live, count);

    /* With-deleted lister returns all three, in id order */
    count = 0;
    err = 0;
    skill_t **items = acta_db_skill_list_all_with_deleted(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_EQ_STR(items[0]->name, "A");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_STR(items[1]->name, "B");
    TEST_ASSERT_NOT_NULL(items[1]->deleted_at);
    TEST_ASSERT_EQ_STR(items[2]->name, "C");
    TEST_ASSERT_NULL(items[2]->deleted_at);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- list_all_with_deleted — invalid args ---------- */
static void test_list_all_with_deleted_invalid_args(void) {
    const char *path = "test/acta_test_sk_lawd_args.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    int count = 0;

    /* NULL db */
    skill_t **items = acta_db_skill_list_all_with_deleted(NULL, 0, -1, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    /* Negative offset */
    err = 0;
    items = acta_db_skill_list_all_with_deleted(db, -1, -1, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- list_all_with_deleted — pagination over mixed rows ---------- */
static void test_list_all_with_deleted_pagination(void) {
    const char *path = "test/acta_test_sk_lawd_page.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "S%d", i);
        int id = sk_create_skill(db, 0, name, "P", "{}");
        TEST_ASSERT(id > 0);
        if (i % 2 == 0)
            TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK); /* delete S0, S2 */
    }

    int err = 0;
    int count = 0;

    /* Page 1: S0 (deleted), S1 (live) */
    skill_t **items = acta_db_skill_list_all_with_deleted(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "S0");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_STR(items[1]->name, "S1");
    TEST_ASSERT_NULL(items[1]->deleted_at);
    acta_db_skill_list_free(items, count);

    /* Page 2: S2 (deleted), S3 (live) */
    count = 0;
    err = 0;
    items = acta_db_skill_list_all_with_deleted(db, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "S2");
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_STR(items[1]->name, "S3");
    TEST_ASSERT_NULL(items[1]->deleted_at);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- count_in_folder_with_deleted ---------- */
static void test_count_in_folder_with_deleted(void) {
    const char *path = "test/acta_test_sk_cifwd.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int fid = sk_create_folder(db, "F", 0);
    TEST_ASSERT(fid > 0);

    /* Folder: 1 live + 1 deleted; root: 1 live + 1 deleted */
    int idF1 = sk_create_skill(db, fid, "F1", "P", "{}");
    int idF2 = sk_create_skill(db, fid, "F2", "P", "{}");
    int idR1 = sk_create_skill(db, 0, "R1", "P", "{}");
    int idR2 = sk_create_skill(db, 0, "R2", "P", "{}");
    TEST_ASSERT(idF1 > 0 && idF2 > 0 && idR1 > 0 && idR2 > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, idF2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, idR2), ACTA_DB_OK);

    int err = 0;

    /* Folder: live-only vs with-deleted */
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, fid, &err), 1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder_with_deleted(db, fid, &err), 2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* Root (folder_id == 0): live-only vs with-deleted */
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder(db, 0, &err), 1);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder_with_deleted(db, 0, &err), 2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* NULL db */
    TEST_ASSERT_EQ_INT(acta_db_skill_count_in_folder_with_deleted(NULL, fid, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- count_all_with_deleted ---------- */
static void test_count_all_with_deleted(void) {
    const char *path = "test/acta_test_sk_cawd.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "C%d", i);
        int id = sk_create_skill(db, 0, name, "P", "{}");
        TEST_ASSERT(id > 0);
        if (i % 2 == 0)
            TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id), ACTA_DB_OK); /* delete C0, C2 */
    }

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all(db, &err), 2);
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all_with_deleted(db, &err), 4);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* NULL db */
    TEST_ASSERT_EQ_INT(acta_db_skill_count_all_with_deleted(NULL, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ── runner ───────────────────────────────────────────────────────── */

void run_skill_deleted_tests(void) {
    fprintf(stderr, "\n=== skill_deleted tests ===\n");

    /* list_in_folder_with_deleted */
    test_list_in_folder_with_deleted_root();
    test_list_in_folder_with_deleted_folder();
    test_list_in_folder_with_deleted_empty();
    test_list_in_folder_with_deleted_invalid_args();
    test_list_in_folder_with_deleted_pagination();
    test_list_in_folder_with_deleted_null_outs();

    /* list_all_with_deleted */
    test_list_all_with_deleted_all_folders();
    test_list_all_with_deleted_invalid_args();
    test_list_all_with_deleted_pagination();

    /* counts */
    test_count_in_folder_with_deleted();
    test_count_all_with_deleted();
}
