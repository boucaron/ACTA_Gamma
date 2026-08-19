/* test_skill_folder.c — Tests for skill.h (tests 6.1 – 6.15) */

#include "test_common.h"
#include "skill.h"

/* ---------- 6.1: create — root ---------- */
static void test_sf_create_root(void) {
    const char *path = "test/acta_test_sf_create_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "RootFolder", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* ---------- 6.2: create — child ---------- */
static void test_sf_create_child(void) {
    const char *path = "test/acta_test_sf_create_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int child_id = 0;
    rc = acta_db_skill_folder_create(db, "Child", parent_id, &child_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(child_id > 0);

    int err = 0;
    skill_folder_t *child = acta_db_skill_folder_get(db, child_id, &err);
    TEST_ASSERT_NOT_NULL(child);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(child->parent_id, parent_id);
    acta_db_skill_folder_free(child);

    test_db_teardown(db, path);
}

/* ---------- 6.3: create — duplicate name (root) ---------- */
static void test_sf_create_duplicate_root(void) {
    const char *path = "test/acta_test_sf_dup_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0;
    int rc = acta_db_skill_folder_create(db, "DupName", 0, &id1);
    TEST_ASSERT_EQ_INT(rc, 0);

    int id2 = 0;
    rc = acta_db_skill_folder_create(db, "DupName", 0, &id2);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 6.4: create — duplicate name (child) ---------- */
static void test_sf_create_duplicate_child(void) {
    const char *path = "test/acta_test_sf_dup_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int child1 = 0;
    rc = acta_db_skill_folder_create(db, "SameChild", parent_id, &child1);
    TEST_ASSERT_EQ_INT(rc, 0);

    int child2 = 0;
    rc = acta_db_skill_folder_create(db, "SameChild", parent_id, &child2);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 6.5: create — invalid parent_id ---------- */
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

/* ---------- 6.6: get — existing ---------- */
static void test_sf_get_existing(void) {
    const char *path = "test/acta_test_sf_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "MyFolder", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

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

/* ---------- 6.7: get — non-existent ---------- */
static void test_sf_get_nonexistent(void) {
    const char *path = "test/acta_test_sf_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, 999999, &err);
    TEST_ASSERT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 6.8: rename — happy ---------- */
static void test_sf_rename_happy(void) {
    const char *path = "test/acta_test_sf_rename.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "OldName", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_rename(db, id, "NewName");
    TEST_ASSERT_EQ_INT(rc, 0);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_STR(f->name, "NewName");
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ---------- 6.9: rename — duplicate ---------- */
static void test_sf_rename_duplicate(void) {
    const char *path = "test/acta_test_sf_rename_dup.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int id_a = 0, id_b = 0;
    rc = acta_db_skill_folder_create(db, "SiblingA", parent_id, &id_a);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "SiblingB", parent_id, &id_b);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Try to rename B to A's name (same parent) — should fail */
    rc = acta_db_skill_folder_rename(db, id_b, "SiblingA");
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 6.10: soft_delete — happy ---------- */
static void test_sf_soft_delete_happy(void) {
    const char *path = "test/acta_test_sf_sdel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "ToDelete", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(f->deleted_at);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ---------- 6.11: soft_delete — has children ---------- */
static void test_sf_soft_delete_has_children(void) {
    const char *path = "test/acta_test_sf_sdel_children.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int child_id = 0;
    rc = acta_db_skill_folder_create(db, "Child", parent_id, &child_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Deleting parent that has children should fail */
    rc = acta_db_skill_folder_soft_delete(db, parent_id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 6.12: list_children — with children ---------- */
static void test_sf_list_children_with(void) {
    const char *path = "test/acta_test_sf_list_with.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int child1 = 0, child2 = 0, child3 = 0;
    rc = acta_db_skill_folder_create(db, "C1", parent_id, &child1);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C2", parent_id, &child2);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C3", parent_id, &child3);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->parent_id, parent_id);
    }
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.13: list_children — no children ---------- */
static void test_sf_list_children_empty(void) {
    const char *path = "test/acta_test_sf_list_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Lonely", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, id, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    if (items != NULL) {
        acta_db_skill_folder_list_free(items, count);
    }

    test_db_teardown(db, path);
}

/* ---------- 6.14: list_all — excludes deleted ---------- */
static void test_sf_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_sf_list_all_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0, id2 = 0;
    int rc = acta_db_skill_folder_create(db, "Keep", 0, &id1);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "DeleteMe", 0, &id2);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_soft_delete(db, id2);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.15: free / list_free ---------- */
static void test_sf_free_valid(void) {
    const char *path = "test/acta_test_sf_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "FreeMe", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

static void test_sf_free_null(void) {
    acta_db_skill_folder_free(NULL);
    TEST_ASSERT(1);
}

static void test_sf_list_free_valid(void) {
    const char *path = "test/acta_test_sf_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int child1 = 0, child2 = 0;
    rc = acta_db_skill_folder_create(db, "C1", parent_id, &child1);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C2", parent_id, &child2);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- runner ---------- */
void run_skill_folder_tests(void) {
    fprintf(stderr, "\n=== skill_folder tests ===\n");
    test_sf_create_root();
    test_sf_create_child();
    test_sf_create_duplicate_root();
    test_sf_create_duplicate_child();
    test_sf_create_invalid_parent();
    test_sf_get_existing();
    test_sf_get_nonexistent();
    test_sf_rename_happy();
    test_sf_rename_duplicate();
    test_sf_soft_delete_happy();
    test_sf_soft_delete_has_children();
    test_sf_list_children_with();
    test_sf_list_children_empty();
    test_sf_list_all_excludes_deleted();
    test_sf_free_valid();
    test_sf_free_null();
    test_sf_list_free_valid();
}
