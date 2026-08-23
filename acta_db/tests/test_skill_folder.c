/* test_skill_folder.c — Tests for skill_folder.h (tests 6.1 – 6.46) */

#include "test_common.h"
#include "skill_folder.h"

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

/* ---------- 6.6: create — NULL out_id (allowed) ---------- */
static void test_sf_create_null_out_id(void) {
    const char *path = "test/acta_test_sf_create_null_outid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_skill_folder_create(db, "NoOutId", 0, NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 6.7: get — existing ---------- */
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

/* ---------- 6.8: get — non-existent ---------- */
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

/* ---------- 6.9: rename — happy ---------- */
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

/* ---------- 6.10: rename — duplicate ---------- */
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

    rc = acta_db_skill_folder_rename(db, id_b, "SiblingA");
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 6.11: soft_delete — happy ---------- */
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

/* ---------- 6.12: soft_delete — has children ---------- */
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

    rc = acta_db_skill_folder_soft_delete(db, parent_id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 6.13: list_children — with children ---------- */
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
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->parent_id, parent_id);
    }
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.14: list_children — no children ---------- */
static void test_sf_list_children_empty(void) {
    const char *path = "test/acta_test_sf_list_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Lonely", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, id, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    if (items != NULL) {
        acta_db_skill_folder_list_free(items, count);
    }

    test_db_teardown(db, path);
}

/* ---------- 6.15: list_all — excludes deleted ---------- */
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
    skill_folder_t **items = acta_db_skill_folder_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.16: free / list_free ---------- */
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
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.17: list_children — pagination (limit=2, offset=0) ---------- */
static void test_sf_list_children_paged_first(void) {
    const char *path = "test/acta_test_sf_paged_first.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    for (int i = 1; i <= 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%02d", i);
        int child_id = 0;
        rc = acta_db_skill_folder_create(db, name, parent_id, &child_id);
        TEST_ASSERT_EQ_INT(rc, 0);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, 0, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Child01");
    TEST_ASSERT_EQ_STR(items[1]->name, "Child02");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.18: list_children — pagination (limit=2, offset=2) ---------- */
static void test_sf_list_children_paged_second(void) {
    const char *path = "test/acta_test_sf_paged_second.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    for (int i = 1; i <= 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%02d", i);
        int child_id = 0;
        rc = acta_db_skill_folder_create(db, name, parent_id, &child_id);
        TEST_ASSERT_EQ_INT(rc, 0);
    }

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, 2, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Child03");
    TEST_ASSERT_EQ_STR(items[1]->name, "Child04");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.19: list_all — pagination (limit=1, offset=1) ---------- */
static void test_sf_list_all_paged(void) {
    const char *path = "test/acta_test_sf_list_all_paged.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0, id2 = 0, id3 = 0;
    int rc = acta_db_skill_folder_create(db, "Alpha", 0, &id1);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Beta", 0, &id2);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Gamma", 0, &id3);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, 1, 1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Beta");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.20: restore — happy path ---------- */
static void test_sf_restore_happy(void) {
    const char *path = "test/acta_test_sf_restore_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Tempo", 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    rc = acta_db_skill_folder_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int count = 0, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    if (items != NULL) acta_db_skill_folder_list_free(items, count);

    rc = acta_db_skill_folder_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_STR(f->name, "Tempo");
    TEST_ASSERT_NULL(f->deleted_at);
    acta_db_skill_folder_free(f);

    items = acta_db_skill_folder_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Tempo");
    acta_db_skill_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 6.21: restore — nonexistent id ---------- */
static void test_sf_restore_nonexistent(void) {
    const char *path = "test/acta_test_sf_restore_nonexistent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_skill_folder_restore(db, 99999);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 6.22: restore — NULL db ---------- */
static void test_sf_restore_null_db(void) {
    int rc = acta_db_skill_folder_restore(NULL, 1);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ---------- 6.23: restore — already-live (idempotent) ---------- */
static void test_sf_restore_already_live(void) {
    const char *path = "test/acta_test_sf_restore_already_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Alive", 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    rc = acta_db_skill_folder_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_STR(f->name, "Alive");
    TEST_ASSERT_NULL(f->deleted_at);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ---------- 6.24: count_children — with children ---------- */
static void test_sf_count_children_with(void) {
    const char *path = "test/acta_test_sf_count_children.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%d", i);
        int child_id = 0;
        rc = acta_db_skill_folder_create(db, name, parent_id, &child_id);
        TEST_ASSERT_EQ_INT(rc, 0);
    }

    int err = 0;
    int count = acta_db_skill_folder_count_children(db, parent_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 4);

    test_db_teardown(db, path);
}

/* ---------- 6.25: count_children — empty ---------- */
static void test_sf_count_children_empty(void) {
    const char *path = "test/acta_test_sf_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Lonely", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int err = 0;
    int count = acta_db_skill_folder_count_children(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ---------- 6.26: count_children — excludes soft-deleted ---------- */
static void test_sf_count_children_excludes_deleted(void) {
    const char *path = "test/acta_test_sf_count_children_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int c1 = 0, c2 = 0, c3 = 0;
    rc = acta_db_skill_folder_create(db, "A", parent_id, &c1);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "B", parent_id, &c2);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C", parent_id, &c3);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_soft_delete(db, c2);
    TEST_ASSERT_EQ_INT(rc, 0);

    int err = 0;
    int count = acta_db_skill_folder_count_children(db, parent_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    test_db_teardown(db, path);
}

/* ---------- 6.27: count_children — NULL db ---------- */
static void test_sf_count_children_null_db(void) {
    int err = 0;
    int count = acta_db_skill_folder_count_children(NULL, 1, &err);
    TEST_ASSERT_EQ_INT(count, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- 6.28: count_children — NULL err ---------- */
static void test_sf_count_children_null_err(void) {
    const char *path = "test/acta_test_sf_count_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "P", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C", parent_id, &(int){0});
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = acta_db_skill_folder_count_children(db, parent_id, NULL);
    TEST_ASSERT_EQ_INT(count, 1);

    test_db_teardown(db, path);
}

/* ---------- 6.29: count_all — mixed ---------- */
static void test_sf_count_all_mixed(void) {
    const char *path = "test/acta_test_sf_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = 0, id2 = 0, id3 = 0;
    int rc = acta_db_skill_folder_create(db, "One", 0, &id1);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Two", 0, &id2);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Three", 0, &id3);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_soft_delete(db, id2);
    TEST_ASSERT_EQ_INT(rc, 0);

    int err = 0;
    int count = acta_db_skill_folder_count_all(db, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);

    test_db_teardown(db, path);
}

/* ---------- 6.30: count_all — empty ---------- */
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

/* ---------- 6.31: count_all — NULL db ---------- */
static void test_sf_count_all_null_db(void) {
    int err = 0;
    int count = acta_db_skill_folder_count_all(NULL, &err);
    TEST_ASSERT_EQ_INT(count, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- 6.32: count_all — NULL err ---------- */
static void test_sf_count_all_null_err(void) {
    const char *path = "test/acta_test_sf_count_all_null_err.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Solo", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = acta_db_skill_folder_count_all(db, NULL);
    TEST_ASSERT_EQ_INT(count, 1);

    test_db_teardown(db, path);
}

/* ---------- 6.33: list_children — negative offset ---------- */
static void test_sf_list_children_negative_offset(void) {
    const char *path = "test/acta_test_sf_neg_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0;
    int rc = acta_db_skill_folder_create(db, "P", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 99, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_children(db, parent_id, -1, 10, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ---------- 6.34: list_all — negative offset ---------- */
static void test_sf_list_all_negative_offset(void) {
    const char *path = "test/acta_test_sf_list_all_neg_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 99, err = 0;
    skill_folder_t **items = acta_db_skill_folder_list_all(db, -5, 10, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ════════════════════════════════════════════════════════════════════
 *  move
 * ════════════════════════════════════════════════════════════════════ */

/* ---------- 6.35: move — child to root ---------- */
static void test_sf_move_to_root(void) {
    const char *path = "test/acta_test_sf_move_to_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent_id = 0, child_id = 0;
    int rc = acta_db_skill_folder_create(db, "Parent", 0, &parent_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Child", parent_id, &child_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Move child to root */
    rc = acta_db_skill_folder_move(db, child_id, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    /* Verify */
    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, child_id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(f->parent_id, 0);
    acta_db_skill_folder_free(f);

    /* Old parent has 0 children now */
    int count = 0;
    rc = acta_db_skill_folder_count_children(db, parent_id, &err);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ---------- 6.36: move — child to another parent ---------- */
static void test_sf_move_to_new_parent(void) {
    const char *path = "test/acta_test_sf_move_new_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int pa = 0, pb = 0, child = 0;
    int rc = acta_db_skill_folder_create(db, "PA", 0, &pa);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "PB", 0, &pb);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Child", pa, &child);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Move child from PA to PB */
    rc = acta_db_skill_folder_move(db, child, pb);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, child, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(f->parent_id, pb);
    acta_db_skill_folder_free(f);

    /* PA should have 0 children, PB should have 1 */
    int count = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_count_children(db, pa, &err), 0);
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_count_children(db, pb, &err), 1);

    test_db_teardown(db, path);
}

/* ---------- 6.37: move — to self → INVALID ---------- */
static void test_sf_move_to_self(void) {
    const char *path = "test/acta_test_sf_move_self.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "Self", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_move(db, id, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 6.38: move — to descendant → INVALID (cycle) ---------- */
static void test_sf_move_to_descendant(void) {
    const char *path = "test/acta_test_sf_move_descendant.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* A → B → C.  Try to move A under C (would create cycle). */
    int a = 0, b = 0, c = 0;
    int rc = acta_db_skill_folder_create(db, "A", 0, &a);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "B", a, &b);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C", b, &c);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_move(db, a, c);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 6.39: move — to direct child → INVALID (cycle) ---------- */
static void test_sf_move_to_direct_child(void) {
    const char *path = "test/acta_test_sf_move_direct_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int a = 0, b = 0;
    int rc = acta_db_skill_folder_create(db, "A", 0, &a);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "B", a, &b);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Move A under its own child B */
    rc = acta_db_skill_folder_move(db, a, b);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 6.40: move — nonexistent folder → NOT_FOUND ---------- */
static void test_sf_move_nonexistent(void) {
    const char *path = "test/acta_test_sf_move_nonexistent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_skill_folder_move(db, 99999, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 6.41: move — to nonexistent parent → NOT_FOUND ---------- */
static void test_sf_move_to_nonexistent_parent(void) {
    const char *path = "test/acta_test_sf_move_bad_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_skill_folder_create(db, "MoveMe", 0, &id);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_move(db, id, 99999);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 6.42: move — NULL db → INVALID ---------- */
static void test_sf_move_null_db(void) {
    int rc = acta_db_skill_folder_move(NULL, 1, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ---------- 6.43: move — id <= 0 → INVALID ---------- */
static void test_sf_move_invalid_id(void) {
    const char *path = "test/acta_test_sf_move_invalid_id.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_skill_folder_move(db, 0, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 6.44: move — soft-deleted folder → NOT_FOUND ---------- */
static void test_sf_move_deleted_folder(void) {
    const char *path = "test/acta_test_sf_move_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent = 0, child = 0;
    int rc = acta_db_skill_folder_create(db, "P", 0, &parent);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C", parent, &child);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_soft_delete(db, child);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Moving a deleted folder should fail */
    rc = acta_db_skill_folder_move(db, child, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 6.45: move — no-op same parent (still OK) ---------- */
static void test_sf_move_same_parent_noop(void) {
    const char *path = "test/acta_test_sf_move_noop.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int parent = 0, child = 0;
    int rc = acta_db_skill_folder_create(db, "P", 0, &parent);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "C", parent, &child);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Move to the same parent it already has */
    rc = acta_db_skill_folder_move(db, child, parent);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, child, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(f->parent_id, parent);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ---------- 6.46: move — deep chain, valid upward move ---------- */
static void test_sf_move_deep_chain_valid(void) {
    const char *path = "test/acta_test_sf_move_deep.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* root: X, Y
       X → a → b
       Move b up to Y (valid, no cycle) */
    int x = 0, y = 0, a = 0, b = 0;
    int rc = acta_db_skill_folder_create(db, "X", 0, &x);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "Y", 0, &y);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "a", x, &a);
    TEST_ASSERT_EQ_INT(rc, 0);
    rc = acta_db_skill_folder_create(db, "b", a, &b);
    TEST_ASSERT_EQ_INT(rc, 0);

    rc = acta_db_skill_folder_move(db, b, y);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    skill_folder_t *f = acta_db_skill_folder_get(db, b, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(f->parent_id, y);
    acta_db_skill_folder_free(f);

    test_db_teardown(db, path);
}

/* ---------- runner ---------- */
void run_skill_folder_tests(void) {
    fprintf(stderr, "\n=== skill_folder tests ===\n");

    /* create */
    test_sf_create_root();
    test_sf_create_child();
    test_sf_create_duplicate_root();
    test_sf_create_duplicate_child();
    test_sf_create_invalid_parent();
    test_sf_create_null_out_id();

    /* get */
    test_sf_get_existing();
    test_sf_get_nonexistent();

    /* rename */
    test_sf_rename_happy();
    test_sf_rename_duplicate();

    /* soft_delete */
    test_sf_soft_delete_happy();
    test_sf_soft_delete_has_children();

    /* listers */
    test_sf_list_children_with();
    test_sf_list_children_empty();
    test_sf_list_all_excludes_deleted();

    /* free */
    test_sf_free_valid();
    test_sf_free_null();
    test_sf_list_free_valid();

    /* pagination */
    test_sf_list_children_paged_first();
    test_sf_list_children_paged_second();
    test_sf_list_all_paged();

    /* restore */
    test_sf_restore_happy();
    test_sf_restore_nonexistent();
    test_sf_restore_null_db();
    test_sf_restore_already_live();

    /* counts */
    test_sf_count_children_with();
    test_sf_count_children_empty();
    test_sf_count_children_excludes_deleted();
    test_sf_count_children_null_db();
    test_sf_count_children_null_err();
    test_sf_count_all_mixed();
    test_sf_count_all_empty();
    test_sf_count_all_null_db();
    test_sf_count_all_null_err();

    /* offset validation */
    test_sf_list_children_negative_offset();
    test_sf_list_all_negative_offset();

    /* move */
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
}
