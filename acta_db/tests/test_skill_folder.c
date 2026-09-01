/* test_skill_folder.c — Tests for skill_folder.h (tests 6.1 – 6.64) */

#include "test_common.h"
#include "skill_folder.h"

/* ════════════════════════════════════════════════════════════════════
 *  create
 * ════════════════════════════════════════════════════════════════════ */

/* 6.1 */
static void test_sf_create_root(void) {
    const char *path = "test/acta_test_sf_create_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "RootFolder", 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* 6.2 */
static void test_sf_create_child(void) {
    const char *path = "test/acta_test_sf_create_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    int child_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Child", parent_id, &child_id),
                       ACTA_DB_OK);
    TEST_ASSERT(child_id > 0);

    int err = 0;
    skill_folder_t *child = acta_db_skill_folder_get(db, child_id, &err);
    TEST_ASSERT_NOT_NULL(child);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(child->parent_id, parent_id);
    TEST_ASSERT_EQ_STR(child->name, "Child");
    acta_db_skill_folder_free(child);

    test_db_teardown(db, path);
}

/* 6.3 */
static void test_sf_create_duplicate_root(void) {
    const char *path = "test/acta_test_sf_dup_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "DupName", 0, &id1),
                       ACTA_DB_OK);

    int id2 = 0;
    int rc = acta_db_skill_folder_create(db, "DupName", 0, &id2);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* 6.4 */
static void test_sf_create_duplicate_child(void) {
    const char *path = "test/acta_test_sf_dup_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    int child1 = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "SameChild", parent_id, &child1),
                       ACTA_DB_OK);

    int child2 = 0;
    int rc = acta_db_skill_folder_create(db, "SameChild", parent_id, &child2);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* 6.5 */
static void test_sf_create_invalid_parent(void) {
    const char *path = "test/acta_test_sf_bad_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Orphan", 999999, &id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* 6.6 */
static void test_sf_create_null_out_id(void) {
    const char *path = "test/acta_test_sf_create_null_outid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "NoOutId", 0, NULL),
                       ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* 6.7 */
static void test_sf_create_null_db(void) {
    int id = 0;
    int rc = acta_db_skill_folder_create(NULL, "X", 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* 6.8 */
static void test_sf_create_null_name(void) {
    const char *path = "test/acta_test_sf_create_null_name.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, NULL, 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 6.9 */
static void test_sf_create_empty_name(void) {
    const char *path = "test/acta_test_sf_create_empty_name.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "", 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  get
 * ════════════════════════════════════════════════════════════════════ */

/* 6.10 */
static void test_sf_get_existing(void) {
    const char *path = "test/acta_test_sf_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "MyFolder", 0, &id),
                       ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(f->id, id);
    TEST_ASSERT_EQ_STR(f->name, "MyFolder");
    TEST_ASSERT_EQ_INT(f->parent_id, 0);
    TEST_ASSERT(f->deleted_at == NULL);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* 6.11 */
static void test_sf_get_nonexistent(void) {
    const char *path = "test/acta_test_sf_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, 999999, &err);
    TEST_ASSERT(f == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* 6.12 */
static void test_sf_get_null_db(void) {
    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(NULL, 1, &err);
    TEST_ASSERT(f == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 6.13 */
static void test_sf_get_invalid_id(void) {
    const char *path = "test/acta_test_sf_get_bad_id.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, 0, &err);
    TEST_ASSERT(f == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  rename
 * ════════════════════════════════════════════════════════════════════ */

/* 6.14 */
static void test_sf_rename_happy(void) {
    const char *path = "test/acta_test_sf_rename.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "OldName", 0, &id),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_rename(db, id, "NewName"),
                       ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_STR(f->name, "NewName");
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* 6.15 */
static void test_sf_rename_duplicate(void) {
    const char *path = "test/acta_test_sf_rename_dup.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    int id_a = 0, id_b = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "SiblingA", parent_id, &id_a),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "SiblingB", parent_id, &id_b),
                       ACTA_DB_OK);

    int rc = acta_db_skill_folder_rename(db, id_b, "SiblingA");
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* 6.16 */
static void test_sf_rename_nonexistent(void) {
    const char *path = "test/acta_test_sf_rename_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_skill_folder_rename(db, 99999, "Whatever");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 6.17 */
static void test_sf_rename_null_db(void) {
    int rc = acta_db_skill_folder_rename(NULL, 1, "X");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ════════════════════════════════════════════════════════════════════
 *  soft_delete
 * ════════════════════════════════════════════════════════════════════ */

/* 6.18 */
static void test_sf_soft_delete_happy(void) {
    const char *path = "test/acta_test_sf_sdel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "ToDelete", 0, &id),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, id), ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT(f->deleted_at != NULL);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* 6.19 */
static void test_sf_soft_delete_has_children(void) {
    const char *path = "test/acta_test_sf_sdel_children.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);
    int child_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Child", parent_id, &child_id),
                       ACTA_DB_OK);

    int rc = acta_db_skill_folder_soft_delete(db, parent_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 6.20 */
static void test_sf_soft_delete_nonexistent(void) {
    const char *path = "test/acta_test_sf_sdel_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_skill_folder_soft_delete(db, 99999);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 6.21 */
static void test_sf_soft_delete_null_db(void) {
    int rc = acta_db_skill_folder_soft_delete(NULL, 1);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* 6.22 */
static void test_sf_soft_delete_already_deleted(void) {
    const char *path = "test/acta_test_sf_sdel_twice.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "X", 0, &id),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, id), ACTA_DB_OK);

    /* Second delete: no live row → NOT_FOUND */
    int rc = acta_db_skill_folder_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  restore
 * ════════════════════════════════════════════════════════════════════ */

/* 6.23 */
static void test_sf_restore_happy(void) {
    const char *path = "test/acta_test_sf_restore_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Tempo", 0, &id),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, id), ACTA_DB_OK);

    /* Not visible in list while deleted */
    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    if (items) acta_db_skill_folder_list_free(items, count);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_restore(db, id), ACTA_DB_OK);

    err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_STR(f->name, "Tempo");
    TEST_ASSERT(f->deleted_at == NULL);
    acta_db_skill_folder_free(f);

    count = 0;
    err = 0;
    items = acta_db_skill_folder_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Tempo");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.24 */
static void test_sf_restore_nonexistent(void) {
    const char *path = "test/acta_test_sf_restore_nonexistent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_restore(db, 99999),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 6.25 */
static void test_sf_restore_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_restore(NULL, 1),
                       ACTA_DB_ERR_INVALID);
}

/* 6.26 */
static void test_sf_restore_already_live(void) {
    const char *path = "test/acta_test_sf_restore_already_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Alive", 0, &id),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_restore(db, id), ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_STR(f->name, "Alive");
    TEST_ASSERT(f->deleted_at == NULL);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  list_children
 * ════════════════════════════════════════════════════════════════════ */

/* 6.27 */
static void test_sf_list_children_with(void) {
    const char *path = "test/acta_test_sf_list_with.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    int child_ids[3] = {0};
    const char *names[] = {"C1", "C2", "C3"};
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ_INT(
            acta_db_skill_folder_create(db, names[i], parent_id, &child_ids[i]),
            ACTA_DB_OK);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id,
                                                                0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->parent_id, parent_id);
    }
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.28 */
static void test_sf_list_children_empty(void) {
    const char *path = "test/acta_test_sf_list_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Lonely", 0, &id),
                       ACTA_DB_OK);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, id,
                                                                0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    if (items) acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.29 */
static void test_sf_list_children_null_db(void) {
    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(NULL, 1,
                                                                0, -1, &count, &err);
    TEST_ASSERT(items == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 6.30 */
static void test_sf_list_children_paged_first(void) {
    const char *path = "test/acta_test_sf_paged_first.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    for (int i = 1; i <= 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%02d", i);
        int child_id = 0;
        TEST_ASSERT_EQ_INT(
            acta_db_skill_folder_create(db, name, parent_id, &child_id),
            ACTA_DB_OK);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id,
                                                                0, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Child01");
    TEST_ASSERT_EQ_STR(items[1]->name, "Child02");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.31 */
static void test_sf_list_children_paged_second(void) {
    const char *path = "test/acta_test_sf_paged_second.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    for (int i = 1; i <= 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%02d", i);
        int child_id = 0;
        TEST_ASSERT_EQ_INT(
            acta_db_skill_folder_create(db, name, parent_id, &child_id),
            ACTA_DB_OK);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id,
                                                                2, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Child03");
    TEST_ASSERT_EQ_STR(items[1]->name, "Child04");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.32 */
static void test_sf_list_children_negative_offset(void) {
    const char *path = "test/acta_test_sf_neg_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "P", 0, &parent_id),
                       ACTA_DB_OK);

    int count = -1, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id,
                                                                -1, 10, &count, &err);
    TEST_ASSERT(items == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  list_all
 * ════════════════════════════════════════════════════════════════════ */

/* 6.33 */
static void test_sf_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_sf_list_all_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0, id2 = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Keep", 0, &id1),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "DeleteMe", 0, &id2),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, id2), ACTA_DB_OK);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.34 */
static void test_sf_list_all_paged(void) {
    const char *path = "test/acta_test_sf_list_all_paged.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    const char *names[] = {"Alpha", "Beta", "Gamma"};
    for (int i = 0; i < 3; i++) {
        int id = 0;
        TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, names[i], 0, &id),
                           ACTA_DB_OK);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, 1, 1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Beta");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.35 */
static void test_sf_list_all_negative_offset(void) {
    const char *path = "test/acta_test_sf_list_all_neg_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = -1, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, -5, 10, &count, &err);
    TEST_ASSERT(items == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  list_all_with_deleted
 * ════════════════════════════════════════════════════════════════════ */

/* 6.59 */
static void test_sf_list_all_with_deleted_includes_deleted(void) {
    const char *path = "test/acta_test_sf_list_allwd_includes.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id_live = 0, id_deleted = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Keep", 0, &id_live),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Gone", 0, &id_deleted),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, id_deleted),
                       ACTA_DB_OK);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all_with_deleted(
        db, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++) {
        if (items[i]->id == id_live) {
            TEST_ASSERT_EQ_STR(items[i]->name, "Keep");
            TEST_ASSERT(items[i]->deleted_at == NULL);
        } else if (items[i]->id == id_deleted) {
            TEST_ASSERT_EQ_STR(items[i]->name, "Gone");
            TEST_ASSERT(items[i]->deleted_at != NULL);
        } else {
            TEST_ASSERT(0); /* unexpected row */
        }
    }
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.60 */
static void test_sf_list_all_with_deleted_empty(void) {
    const char *path = "test/acta_test_sf_list_allwd_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all_with_deleted(
        db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    if (items) acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.61 */
static void test_sf_list_all_with_deleted_null_db(void) {
    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all_with_deleted(
        NULL, 0, -1, &count, &err);
    TEST_ASSERT(items == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 6.62 */
static void test_sf_list_all_with_deleted_negative_offset(void) {
    const char *path = "test/acta_test_sf_list_allwd_neg_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = -1, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all_with_deleted(
        db, -5, 10, &count, &err);
    TEST_ASSERT(items == NULL);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* 6.63 */
static void test_sf_list_all_with_deleted_paged(void) {
    const char *path = "test/acta_test_sf_list_allwd_paged.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    const char *names[] = {"F1", "F2", "F3", "F4"};
    int ids[4] = {0};
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, names[i], 0, &ids[i]),
                           ACTA_DB_OK);
    }
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, ids[1]), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, ids[3]), ACTA_DB_OK);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all_with_deleted(
        db, 0, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "F1");
    TEST_ASSERT(items[0]->deleted_at == NULL);
    TEST_ASSERT_EQ_STR(items[1]->name, "F2");
    TEST_ASSERT(items[1]->deleted_at != NULL);
    acta_db_skill_folder_list_free(items, count);

    count = 0;
    err = 0;
    items = acta_db_skill_folder_list_all_with_deleted(db, 2, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "F3");
    TEST_ASSERT(items[0]->deleted_at == NULL);
    TEST_ASSERT_EQ_STR(items[1]->name, "F4");
    TEST_ASSERT(items[1]->deleted_at != NULL);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* 6.64 */
static void test_sf_list_all_with_deleted_nested(void) {
    const char *path = "test/acta_test_sf_list_allwd_nested.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0, child_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Child", parent_id, &child_id),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, child_id),
                       ACTA_DB_OK);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all_with_deleted(
        db, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++) {
        if (items[i]->id == child_id) {
            /* Soft-deleted child still reports its (live) parent. */
            TEST_ASSERT_EQ_INT(items[i]->parent_id, parent_id);
            TEST_ASSERT(items[i]->deleted_at != NULL);
        } else if (items[i]->id == parent_id) {
            TEST_ASSERT_EQ_INT(items[i]->parent_id, 0);
            TEST_ASSERT(items[i]->deleted_at == NULL);
        } else {
            TEST_ASSERT(0); /* unexpected row */
        }
    }
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  count_children
 * ════════════════════════════════════════════════════════════════════ */

/* 6.36 */
static void test_sf_count_children_with(void) {
    const char *path = "test/acta_test_sf_count_children.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%d", i);
        int child_id = 0;
        TEST_ASSERT_EQ_INT(
            acta_db_skill_folder_create(db, name, parent_id, &child_id),
            ACTA_DB_OK);
    }

    int err = 0;
    int count = acta_db_skill_folder_count_children(db, parent_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 4);

    test_db_teardown(db, path);
}

/* 6.37 */
static void test_sf_count_children_empty(void) {
    const char *path = "test/acta_test_sf_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Lonely", 0, &id),
                       ACTA_DB_OK);

    int err = 0;
    int count = acta_db_skill_folder_count_children(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* 6.38 */
static void test_sf_count_children_excludes_deleted(void) {
    const char *path = "test/acta_test_sf_count_children_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);

    int c1 = 0, c2 = 0, c3 = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "A", parent_id, &c1),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "B", parent_id, &c2),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "C", parent_id, &c3),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, c2), ACTA_DB_OK);

    int err = 0;
    int count = acta_db_skill_folder_count_children(db, parent_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    test_db_teardown(db, path);
}

/* 6.39 */
static void test_sf_count_children_null_db(void) {
    int err = 0;
    int count = acta_db_skill_folder_count_children(NULL, 1, &err);
    TEST_ASSERT_EQ_INT(count, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 6.40 */
static void test_sf_count_children_null_err(void) {
    const char *path = "test/acta_test_sf_count_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "P", 0, &parent_id),
                       ACTA_DB_OK);
    int child_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "C", parent_id, &child_id),
                       ACTA_DB_OK);

    int count = acta_db_skill_folder_count_children(db, parent_id, NULL);
    TEST_ASSERT_EQ_INT(count, 1);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  count_all
 * ════════════════════════════════════════════════════════════════════ */

/* 6.41 */
static void test_sf_count_all_mixed(void) {
    const char *path = "test/acta_test_sf_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0, id2 = 0, id3 = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "One", 0, &id1),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Two", 0, &id2),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Three", 0, &id3),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, id2), ACTA_DB_OK);

    int err = 0;
    int count = acta_db_skill_folder_count_all(db, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    test_db_teardown(db, path);
}

/* 6.42 */
static void test_sf_count_all_empty(void) {
    const char *path = "test/acta_test_sf_count_all_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    int count = acta_db_skill_folder_count_all(db, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* 6.43 */
static void test_sf_count_all_null_db(void) {
    int err = 0;
    int count = acta_db_skill_folder_count_all(NULL, &err);
    TEST_ASSERT_EQ_INT(count, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* 6.44 */
static void test_sf_count_all_null_err(void) {
    const char *path = "test/acta_test_sf_count_all_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Solo", 0, &id),
                       ACTA_DB_OK);

    int count = acta_db_skill_folder_count_all(db, NULL);
    TEST_ASSERT_EQ_INT(count, 1);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  move_to
 * ════════════════════════════════════════════════════════════════════ */

/* 6.45 */
static void test_sf_move_to_root(void) {
    const char *path = "test/acta_test_sf_move_to_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0, child_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Parent", 0, &parent_id),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Child", parent_id, &child_id),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, child_id, 0),
                       ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, child_id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(f->parent_id, 0);
    acta_db_skill_folder_free(f);

    int count = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_count_children(db, parent_id, &err), 0);

    test_db_teardown(db, path);
}

/* 6.46 */
static void test_sf_move_to_new_parent(void) {
    const char *path = "test/acta_test_sf_move_new_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int pa = 0, pb = 0, child = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "PA", 0, &pa),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "PB", 0, &pb),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Child", pa, &child),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, child, pb),
                       ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, child, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(f->parent_id, pb);
    acta_db_skill_folder_free(f);

    int c1 = acta_db_skill_folder_count_children(db, pa, &err);
    int c2 = acta_db_skill_folder_count_children(db, pb, &err);
    TEST_ASSERT_EQ_INT(c1, 0);
    TEST_ASSERT_EQ_INT(c2, 1);

    test_db_teardown(db, path);
}

/* 6.47 */
static void test_sf_move_to_self(void) {
    const char *path = "test/acta_test_sf_move_self.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Self", 0, &id),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, id, id),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 6.48 */
static void test_sf_move_to_descendant(void) {
    const char *path = "test/acta_test_sf_move_descendant.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* A → B → C.  Moving A under C would create a cycle. */
    int a = 0, b = 0, c = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "A", 0, &a),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "B", a, &b),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "C", b, &c),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, a, c),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 6.49 */
static void test_sf_move_to_direct_child(void) {
    const char *path = "test/acta_test_sf_move_direct_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int a = 0, b = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "A", 0, &a),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "B", a, &b),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, a, b),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 6.50 */
static void test_sf_move_nonexistent(void) {
    const char *path = "test/acta_test_sf_move_nonexistent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, 99999, 0),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 6.51 */
static void test_sf_move_to_nonexistent_parent(void) {
    const char *path = "test/acta_test_sf_move_bad_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "MoveMe", 0, &id),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, id, 99999),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 6.52 */
static void test_sf_move_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(NULL, 1, 0),
                       ACTA_DB_ERR_INVALID);
}

/* 6.53 */
static void test_sf_move_invalid_id(void) {
    const char *path = "test/acta_test_sf_move_invalid_id.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, 0, 0),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* 6.54 */
static void test_sf_move_deleted_folder(void) {
    const char *path = "test/acta_test_sf_move_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent = 0, child = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "P", 0, &parent),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "C", parent, &child),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_soft_delete(db, child),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, child, 0),
                       ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* 6.55 */
static void test_sf_move_same_parent_noop(void) {
    const char *path = "test/acta_test_sf_move_noop.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent = 0, child = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "P", 0, &parent),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "C", parent, &child),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, child, parent),
                       ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, child, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(f->parent_id, parent);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* 6.56 */
static void test_sf_move_deep_chain_valid(void) {
    const char *path = "test/acta_test_sf_move_deep.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* root: X, Y
       X → a → b
       Move b under Y (valid, no cycle). */
    int x = 0, y = 0, a = 0, b = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "X", 0, &x),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "Y", 0, &y),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "a", x, &a),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "b", a, &b),
                       ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_skill_folder_move_to(db, b, y),
                       ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, b, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(f->parent_id, y);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  free
 * ════════════════════════════════════════════════════════════════════ */

/* 6.57 */
static void test_sf_free_null(void) {
    acta_db_skill_folder_free(NULL);
}

/* 6.58 */
static void test_sf_list_free_null(void) {
    acta_db_skill_folder_list_free(NULL, 0);
}

/* ════════════════════════════════════════════════════════════════════
 *  runner
 * ════════════════════════════════════════════════════════════════════ */

void run_skill_folder_tests(void) {
    fprintf(stderr, "\n=== skill_folder tests ===\n");

    /* create */
    test_sf_create_root();
    test_sf_create_child();
    test_sf_create_duplicate_root();
    test_sf_create_duplicate_child();
    test_sf_create_invalid_parent();
    test_sf_create_null_out_id();
    test_sf_create_null_db();
    test_sf_create_null_name();
    test_sf_create_empty_name();

    /* get */
    test_sf_get_existing();
    test_sf_get_nonexistent();
    test_sf_get_null_db();
    test_sf_get_invalid_id();

    /* rename */
    test_sf_rename_happy();
    test_sf_rename_duplicate();
    test_sf_rename_nonexistent();
    test_sf_rename_null_db();

    /* soft_delete */
    test_sf_soft_delete_happy();
    test_sf_soft_delete_has_children();
    test_sf_soft_delete_nonexistent();
    test_sf_soft_delete_null_db();
    test_sf_soft_delete_already_deleted();

    /* restore */
    test_sf_restore_happy();
    test_sf_restore_nonexistent();
    test_sf_restore_null_db();
    test_sf_restore_already_live();

    /* list_children */
    test_sf_list_children_with();
    test_sf_list_children_empty();
    test_sf_list_children_null_db();
    test_sf_list_children_paged_first();
    test_sf_list_children_paged_second();
    test_sf_list_children_negative_offset();

    /* list_all */
    test_sf_list_all_excludes_deleted();
    test_sf_list_all_paged();
    test_sf_list_all_negative_offset();

    /* list_all_with_deleted */
    test_sf_list_all_with_deleted_includes_deleted();
    test_sf_list_all_with_deleted_empty();
    test_sf_list_all_with_deleted_null_db();
    test_sf_list_all_with_deleted_negative_offset();
    test_sf_list_all_with_deleted_paged();
    test_sf_list_all_with_deleted_nested();

    /* count_children */
    test_sf_count_children_with();
    test_sf_count_children_empty();
    test_sf_count_children_excludes_deleted();
    test_sf_count_children_null_db();
    test_sf_count_children_null_err();

    /* count_all */
    test_sf_count_all_mixed();
    test_sf_count_all_empty();
    test_sf_count_all_null_db();
    test_sf_count_all_null_err();

    /* move_to */
    test_sf_move_to_root();
    test_sf_move_to_new_parent();
    test_sf_move_to_self();
    test_sf_move_to_descendant();
    test_sf_move_to_direct_child();
    test_sf_move_nonexistent();
    test_sf_move_to_nonexistent_parent();
    test_sf_move_null_db();
    test_sf_move_invalid_id();
    test_sf_move_deleted_folder();
    test_sf_move_same_parent_noop();
    test_sf_move_deep_chain_valid();

    /* free */
    test_sf_free_null();
    test_sf_list_free_null();
}
