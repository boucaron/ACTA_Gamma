/* test_skill_placement.c
 * move_to_folder, list_in_folder, list_all, folder children */

#include "test_skill_helpers.h"

/* ── list_in_folder ─────────────────────────────────────────────── */

/* 7.19 */
static void test_list_root(void) {
    const char *path = "test/acta_test_sk_list_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "RootA", "P1", "{}");
    int id2 = sk_create_skill(db, 0, "RootB", "P2", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0);

    int folder_id = sk_create_folder(db, "Sub", 0);
    TEST_ASSERT(folder_id > 0);
    int child_id = sk_create_skill(db, folder_id, "ChildC", "P3", "{}");
    TEST_ASSERT(child_id > 0);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_in_folder(db, 0, 0, -1,
                                                   &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
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

/* 7.20 */
static void test_list_specific_folder(void) {
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

    int count = 0, err = 0;
    skill_t **items;
    items = acta_db_skill_list_in_folder(db, folder1, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    acta_db_skill_list_free(items, count);

    count = 0;
    items = acta_db_skill_list_in_folder(db, folder2, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id2);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.21 */
static void test_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_sk_list_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = sk_create_skill(db, 0, "Alive1", "P1", "{}");
    int id2 = sk_create_skill(db, 0, "Alive2", "P2", "{}");
    int id3 = sk_create_skill(db, 0, "ToDelete", "P3", "{}");
    TEST_ASSERT(id1 > 0 && id2 > 0 && id3 > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, id3), ACTA_DB_OK);

    int count = 0, err = 0;
    skill_t **items = acta_db_skill_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    int found1 = 0, found2 = 0;
    for (int i = 0; i < count; i++) {
        TEST_ASSERT(items[i]->id != id3);
        if (items[i]->id == id1) found1 = 1;
        if (items[i]->id == id2) found2 = 1;
    }
    TEST_ASSERT(found1 && found2);
    acta_db_skill_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.22 — new: negative offset is rejected */
static void test_list_negative_offset(void) {
    const char *path = "test/acta_test_sk_list_negoffset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    int count = 0;

    skill_t **items = acta_db_skill_list_all(db, -1, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);

    items = acta_db_skill_list_in_folder(db, 0, -5, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);

    test_db_teardown(db, path);
}

/* ── move_to_folder ─────────────────────────────────────────────── */

/* 7.26 */
static void test_move_to_folder_happy(void) {
    const char *path = "test/acta_test_sk_move_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "TargetFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, 0, "Movable", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, folder_id),
                       ACTA_DB_OK);

    int err = 0;
    skill_t *s = acta_db_skill_get(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, folder_id);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* 7.27 */
static void test_move_to_root(void) {
    const char *path = "test/acta_test_sk_move_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "OrigFolder", 0);
    TEST_ASSERT(folder_id > 0);

    int id = sk_create_skill(db, folder_id, "MoveToRoot", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 0), ACTA_DB_OK);

    int err = 0;
    skill_t *s = acta_db_skill_get(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* 7.28 */
static void test_move_invalid_folder(void) {
    const char *path = "test/acta_test_sk_move_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = sk_create_skill(db, 0, "BadMove", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, 99999),
                       ACTA_DB_ERR_NOT_FOUND);

    int err = 0;
    skill_t *s = acta_db_skill_get(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* 7.29 */
static void test_move_deleted_skill(void) {
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

/* 7.30 */
static void test_move_to_deleted_folder(void) {
    const char *path = "test/acta_test_sk_move_del_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = sk_create_folder(db, "WillDelete", 0);
    TEST_ASSERT(folder_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, folder_id),
                       ACTA_DB_OK);

    int id = sk_create_skill(db, 0, "MoveDel", "Prompt", "{}");
    TEST_ASSERT(id > 0);

    TEST_ASSERT_EQ_INT(acta_db_skill_move_to_folder(db, id, folder_id),
                       ACTA_DB_ERR_NOT_FOUND);

    int err = 0;
    skill_t *s = acta_db_skill_get(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ_INT(s->folder_id, 0);
    acta_db_skill_free(s);

    test_db_teardown(db, path);
}

/* ── folder children (pagination on skill_folder lister) ────────── */

/* 7.39 */
static void test_folder_children_limit(void) {
    const char *path = "test/acta_test_sk_folder_children_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = sk_create_folder(db, "Parent", 0);
    TEST_ASSERT(parent_id > 0);

    int child_ids[4];
    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%d", i + 1);
        child_ids[i] = sk_create_folder(db, name, parent_id);
        TEST_ASSERT(child_ids[i] > 0);
    }

    int count = 0, err = 0;

    skill_folder_t **items = acta_db_skill_folder_list_children(
        db, parent_id, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->id, child_ids[0]);
    TEST_ASSERT_EQ_INT(items[1]->id, child_ids[1]);
    acta_db_skill_folder_list_free(items, count);

    count = 0;
    items = acta_db_skill_folder_list_children(db, parent_id, 2, 2,
                                               &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->id, child_ids[2]);
    TEST_ASSERT_EQ_INT(items[1]->id, child_ids[3]);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 7.40 */
static void test_folder_children_root_offset(void) {
    const char *path = "test/acta_test_sk_folder_root_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int a = sk_create_folder(db, "RootA", 0);
    int b = sk_create_folder(db, "RootB", 0);
    int c = sk_create_folder(db, "RootC", 0);
    TEST_ASSERT(a > 0 && b > 0 && c > 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(
        db, 0, 1, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, b);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ── runner ─────────────────────────────────────────────────────── */

int run_skill_placement_tests(void) {
    fprintf(stderr, "\n=== skill_placement tests ===\n");

    test_list_root();
    test_list_specific_folder();
    test_list_all_excludes_deleted();
    test_list_negative_offset();

    test_move_to_folder_happy();
    test_move_to_root();
    test_move_invalid_folder();
    test_move_deleted_skill();
    test_move_to_deleted_folder();

    test_folder_children_limit();
    test_folder_children_root_offset();

    return test_failures;
}
