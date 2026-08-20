#include "test_common.h"
#include "db.h"

/* ---------- 3.1: create — root ---------- */
static void test_mf_create_root(void) {
    const char *path = "test/acta_test_mf_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id = 0;
    int rc = acta_db_model_folder_create(db, "Root Folder", 0, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT(id > 0);
    test_db_teardown(db, path);
}

/* ---------- 3.2: create — child ---------- */
static void test_mf_create_child(void) {
    const char *path = "test/acta_test_mf_child.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id = 0, child_id = 0;
    acta_db_model_folder_create(db, "Parent", 0, &parent_id);
    int rc = acta_db_model_folder_create(db, "Child", parent_id, &child_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT(child_id > 0);
    test_db_teardown(db, path);
}

/* ---------- 3.3: create — duplicate name (root) ---------- */
static void test_mf_create_dup_root(void) {
    const char *path = "test/acta_test_mf_duproot.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id1, id2;
    acta_db_model_folder_create(db, "SameName", 0, &id1);
    int rc = acta_db_model_folder_create(db, "SameName", 0, &id2);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);   /* UNIQUE constraint */
    test_db_teardown(db, path);
}

/* ---------- 3.4: create — duplicate name (child) ---------- */
static void test_mf_create_dup_child(void) {
    const char *path = "test/acta_test_mf_dupchild.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id;
    acta_db_model_folder_create(db, "P", 0, &parent_id);
    int id1, id2;
    acta_db_model_folder_create(db, "Dup", parent_id, &id1);
    int rc = acta_db_model_folder_create(db, "Dup", parent_id, &id2);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);   /* UNIQUE constraint */
    test_db_teardown(db, path);
}

/* ---------- 3.5: create — same name different parent ---------- */
static void test_mf_create_same_name_diff_parent(void) {
    const char *path = "test/acta_test_mf_samename.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int p1, p2;
    acta_db_model_folder_create(db, "A", 0, &p1);
    acta_db_model_folder_create(db, "B", 0, &p2);
    int c1, c2;
    int rc1 = acta_db_model_folder_create(db, "X", p1, &c1);
    int rc2 = acta_db_model_folder_create(db, "X", p2, &c2);
    TEST_ASSERT_EQ_INT(rc1, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rc2, ACTA_DB_OK);
    test_db_teardown(db, path);
}

/* ---------- 3.6: create — invalid parent_id ---------- */
static void test_mf_create_invalid_parent(void) {
    const char *path = "test/acta_test_mf_badparent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    int rc = acta_db_model_folder_create(db, "Orphan", 999999, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);   /* FK constraint */
    test_db_teardown(db, path);
}

/* ---------- 3.7: get — existing ---------- */
static void test_mf_get_existing(void) {
    const char *path = "test/acta_test_mf_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    acta_db_model_folder_create(db, "MyFolder", 0, &id);
    int err;
    model_folder_t *f = acta_db_model_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ_INT(f->id, id);
    TEST_ASSERT_EQ_STR(f->name, "MyFolder");
    acta_db_model_folder_free(f);
    test_db_teardown(db, path);
}

/* ---------- 3.8: get — non-existent ---------- */
static void test_mf_get_nonexistent(void) {
    const char *path = "test/acta_test_mf_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int err;
    model_folder_t *f = acta_db_model_folder_get(db, 999999, &err);
    TEST_ASSERT_NULL(f);
    test_db_teardown(db, path);
}

/* ---------- 3.9: rename — happy ---------- */
static void test_mf_rename_happy(void) {
    const char *path = "test/acta_test_mf_rename.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    acta_db_model_folder_create(db, "Old", 0, &id);
    int rc = acta_db_model_folder_rename(db, id, "New");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    int err;
    model_folder_t *f = acta_db_model_folder_get(db, id, &err);
    TEST_ASSERT_EQ_STR(f->name, "New");
    acta_db_model_folder_free(f);
    test_db_teardown(db, path);
}

/* ---------- 3.10: rename — duplicate name ---------- */
static void test_mf_rename_dup(void) {
    const char *path = "test/acta_test_mf_renamedup.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id1, id2;
    acta_db_model_folder_create(db, "A", 0, &id1);
    acta_db_model_folder_create(db, "B", 0, &id2);
    int rc = acta_db_model_folder_rename(db, id2, "A");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);   /* UNIQUE constraint */
    test_db_teardown(db, path);
}

/* ---------- 3.11: soft_delete — happy ---------- */
static void test_mf_soft_delete_happy(void) {
    const char *path = "test/acta_test_mf_sd.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    acta_db_model_folder_create(db, "ToDelete", 0, &id);
    int rc = acta_db_model_folder_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    int err;
    model_folder_t *f = acta_db_model_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(f->deleted_at);
    acta_db_model_folder_free(f);
    test_db_teardown(db, path);
}

/* ---------- 3.12: soft_delete — already deleted ---------- */
static void test_mf_soft_delete_twice(void) {
    const char *path = "test/acta_test_mf_sd2.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    acta_db_model_folder_create(db, "X", 0, &id);
    acta_db_model_folder_soft_delete(db, id);
    int rc = acta_db_model_folder_soft_delete(db, id);
    /* WHERE deleted_at IS NULL matches 0 rows;
     * sqlite3_step still returns SQLITE_DONE → ACTA_DB_OK */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    int err;
    model_folder_t *f = acta_db_model_folder_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(f->deleted_at);
    acta_db_model_folder_free(f);
    test_db_teardown(db, path);
}

/* ---------- 3.13: soft_delete — has children ---------- */
static void test_mf_soft_delete_has_children(void) {
    const char *path = "test/acta_test_mf_sdchildren.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id, child_id;
    acta_db_model_folder_create(db, "P", 0, &parent_id);
    acta_db_model_folder_create(db, "C", parent_id, &child_id);
    int rc = acta_db_model_folder_soft_delete(db, parent_id);
    /* Soft-delete is a timestamp UPDATE; FK still satisfied. */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    test_db_teardown(db, path);
}

/* ---------- 3.14: list_children — with children ---------- */
static void test_mf_list_children_with(void) {
    const char *path = "test/acta_test_mf_lc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id;
    acta_db_model_folder_create(db, "P", 0, &parent_id);
    acta_db_model_folder_create(db, "C1", parent_id, &(int){0});
    acta_db_model_folder_create(db, "C2", parent_id, &(int){0});
    acta_db_model_folder_create(db, "C3", parent_id, &(int){0});
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_children(db, parent_id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.15: list_children — no children ---------- */
static void test_mf_list_children_none(void) {
    const char *path = "test/acta_test_mf_lc0.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    acta_db_model_folder_create(db, "Leaf", 0, &id);
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_children(db, id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(count, 0);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.16: list_all — mixed ---------- */
static void test_mf_list_all_mixed(void) {
    const char *path = "test/acta_test_mf_la.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int root1, root2, child1;
    acta_db_model_folder_create(db, "R1", 0, &root1);
    acta_db_model_folder_create(db, "R2", 0, &root2);
    acta_db_model_folder_create(db, "C1", root1, &child1);
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.17: list_all — excludes deleted ---------- */
static void test_mf_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_mf_la_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id1, id2, id3;
    acta_db_model_folder_create(db, "A", 0, &id1);
    acta_db_model_folder_create(db, "B", 0, &id2);
    acta_db_model_folder_create(db, "C", 0, &id3);
    acta_db_model_folder_soft_delete(db, id2);
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.18-3.20: free / list_free ---------- */
static void test_mf_free_valid(void) {
    const char *path = "test/acta_test_mf_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id;
    acta_db_model_folder_create(db, "F", 0, &id);
    int err;
    model_folder_t *f = acta_db_model_folder_get(db, id, &err);
    acta_db_model_folder_free(f);
    test_db_teardown(db, path);
}

static void test_mf_free_null(void) {
    acta_db_model_folder_free(NULL);
    TEST_ASSERT(1);
}

static void test_mf_list_free_valid(void) {
    const char *path = "test/acta_test_mf_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Folder%d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 0, &count, &err);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.21: list_all — pagination page 1 ---------- */
static void test_mf_list_all_page1(void) {
    const char *path = "test/acta_test_mf_page1.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    /* Verify ordering: F00, F01 (alphabetical) */
    TEST_ASSERT_EQ_STR(items[0]->name, "F00");
    TEST_ASSERT_EQ_STR(items[1]->name, "F01");
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.22: list_all — pagination page 2 ---------- */
static void test_mf_list_all_page2(void) {
    const char *path = "test/acta_test_mf_page2.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "F02");
    TEST_ASSERT_EQ_STR(items[1]->name, "F03");
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.23: list_all — pagination last partial page ---------- */
static void test_mf_list_all_last_page(void) {
    const char *path = "test/acta_test_mf_page_last.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 4, 2, &count, &err);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "F04");
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.24: list_all — offset beyond range ---------- */
static void test_mf_list_all_offset_beyond(void) {
    const char *path = "test/acta_test_mf_page_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 10, 5, &count, &err);
    TEST_ASSERT_EQ_INT(count, 0);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.25: list_all — limit 0 means no limit ---------- */
static void test_mf_list_all_no_limit(void) {
    const char *path = "test/acta_test_mf_nolimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(count, 7);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.26: list_all — limit larger than total ---------- */
static void test_mf_list_all_limit_exceeds(void) {
    const char *path = "test/acta_test_mf_limitbig.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 100, &count, &err);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.27: list_children — pagination page 1 ---------- */
static void test_mf_list_children_page1(void) {
    const char *path = "test/acta_test_mf_lc_page1.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id;
    acta_db_model_folder_create(db, "P", 0, &parent_id);
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "C%02d", i);
        acta_db_model_folder_create(db, name, parent_id, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_children(db, parent_id, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "C00");
    TEST_ASSERT_EQ_STR(items[1]->name, "C01");
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.28: list_children — pagination page 2 ---------- */
static void test_mf_list_children_page2(void) {
    const char *path = "test/acta_test_mf_lc_page2.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id;
    acta_db_model_folder_create(db, "P", 0, &parent_id);
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "C%02d", i);
        acta_db_model_folder_create(db, name, parent_id, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_children(db, parent_id, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "C02");
    TEST_ASSERT_EQ_STR(items[1]->name, "C03");
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.29: list_children — pagination excludes deleted ---------- */
static void test_mf_list_children_paged_excludes_deleted(void) {
    const char *path = "test/acta_test_mf_lc_pagedel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int parent_id, id1, id2, id3;
    acta_db_model_folder_create(db, "P", 0, &parent_id);
    acta_db_model_folder_create(db, "A", parent_id, &id1);
    acta_db_model_folder_create(db, "B", parent_id, &id2);
    acta_db_model_folder_create(db, "C", parent_id, &id3);
    acta_db_model_folder_soft_delete(db, id2);
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_children(db, parent_id, 0, 10, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 3.30: list_all — pagination excludes deleted ---------- */
static void test_mf_list_all_paged_excludes_deleted(void) {
    const char *path = "test/acta_test_mf_la_pagedel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int id1, id2, id3, id4;
    acta_db_model_folder_create(db, "A", 0, &id1);
    acta_db_model_folder_create(db, "B", 0, &id2);
    acta_db_model_folder_create(db, "C", 0, &id3);
    acta_db_model_folder_create(db, "D", 0, &id4);
    acta_db_model_folder_soft_delete(db, id2);
    /* 3 live folders, page size 2 → page 1 has 2, page 2 has 1 */
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_folder_list_free(items, count);

    int count2 = 0;
    model_folder_t **items2 = acta_db_model_folder_list_all(db, 2, 2, &count2, &err);
    TEST_ASSERT_EQ_INT(count2, 1);
    acta_db_model_folder_list_free(items2, count2);
    test_db_teardown(db, path);
}

/* ---------- 3.31: list_all — offset 0 with limit ---------- */
static void test_mf_list_all_offset_zero(void) {
    const char *path = "test/acta_test_mf_off0.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "F%02d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }
    int count = 0, err;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 1, &count, &err);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "F00");
    acta_db_model_folder_list_free(items, count);
    test_db_teardown(db, path);
}

void run_model_folder_tests(void) {
    fprintf(stderr, "\n=== model_folder tests ===\n");
    test_mf_create_root();
    test_mf_create_child();
    test_mf_create_dup_root();
    test_mf_create_dup_child();
    test_mf_create_same_name_diff_parent();
    test_mf_create_invalid_parent();
    test_mf_get_existing();
    test_mf_get_nonexistent();
    test_mf_rename_happy();
    test_mf_rename_dup();
    test_mf_soft_delete_happy();
    test_mf_soft_delete_twice();
    test_mf_soft_delete_has_children();
    test_mf_list_children_with();
    test_mf_list_children_none();
    test_mf_list_all_mixed();
    test_mf_list_all_excludes_deleted();
    test_mf_free_valid();
    test_mf_free_null();
    test_mf_list_free_valid();
    /* pagination */
    test_mf_list_all_page1();
    test_mf_list_all_page2();
    test_mf_list_all_last_page();
    test_mf_list_all_offset_beyond();
    test_mf_list_all_no_limit();
    test_mf_list_all_limit_exceeds();
    test_mf_list_children_page1();
    test_mf_list_children_page2();
    test_mf_list_children_paged_excludes_deleted();
    test_mf_list_all_paged_excludes_deleted();
    test_mf_list_all_offset_zero();
}
