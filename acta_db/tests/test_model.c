#include "test_common.h"

/* ---------- 4.1: model_create — root ---------- */
static void test_model_create_root(void) {
    const char *path = "test/acta_test_m_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_t m = {
        .name = "GPT-4",
        .backend = "openai",
        .model_identifier = "gpt-4",
        .folder_id = 0,
    };
    int id = 0;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(id > 0);
    test_db_teardown(db, path);
}

/* ---------- 4.2: model_create — in folder ---------- */
static void test_model_create_in_folder(void) {
    const char *path = "test/acta_test_m_folder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int folder_id;
    acta_db_model_folder_create(db, "MyFolder", 0, &folder_id);

    model_t m = {
        .name = "Local LLM",
        .backend = "ollama",
        .model_identifier = "llama3",
        .folder_id = folder_id,
    };
    int id = 0;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    test_db_teardown(db, path);
}

/* ---------- 4.3: model_create — initial revision auto-created ---------- */
static void test_model_create_initial_revision(void) {
    const char *path = "test/acta_test_m_rev1.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int model_id = 0;
    acta_db_model_create(db, &m, &model_id);

    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, model_id, &count);
    TEST_ASSERT_EQ_INT(count, 1);
    if (count > 0) {
        TEST_ASSERT_EQ_INT(revs[0].revision, 1);
    }
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.4: model_create — current_revision set to 1 ---------- */
static void test_model_create_current_rev(void) {
    const char *path = "test/acta_test_m_currev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id = 0;
    acta_db_model_create(db, &m, &id);
    model_t *got = acta_db_model_get(db, id);
    TEST_ASSERT_EQ_INT(got->current_revision, 1);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.5: model_create — NULL name ---------- */
static void test_model_create_null_name(void) {
    const char *path = "test/acta_test_m_nullname.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = NULL, .backend = "b", .model_identifier = "mid" };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 4.6: model_create — NULL backend ---------- */
static void test_model_create_null_backend(void) {
    const char *path = "test/acta_test_m_nullbe.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "N", .backend = NULL, .model_identifier = "mid" };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 4.7: model_create — NULL model_identifier ---------- */
static void test_model_create_null_mid(void) {
    const char *path = "test/acta_test_m_nullmid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "N", .backend = "b", .model_identifier = NULL };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 4.8: model_create — invalid folder_id ---------- */
static void test_model_create_invalid_folder(void) {
    const char *path = "test/acta_test_m_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "N", .backend = "b", .model_identifier = "mid", .folder_id = 999999 };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 4.9: model_create — duplicate name (root) ---------- */
static void test_model_create_dup_root(void) {
    const char *path = "test/acta_test_m_duproot.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "Dup", .backend = "b", .model_identifier = "mid" };
    int id1;
    acta_db_model_create(db, &m, &id1);
    int id2;
    int rc = acta_db_model_create(db, &m, &id2);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 4.10: model_create — duplicate name (child) ---------- */
static void test_model_create_dup_child(void) {
    const char *path = "test/acta_test_m_dupchild.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int folder_id;
    acta_db_model_folder_create(db, "F", 0, &folder_id);
    model_t m = { .name = "Dup", .backend = "b", .model_identifier = "mid", .folder_id = folder_id };
    int id1;
    acta_db_model_create(db, &m, &id1);
    int id2;
    int rc = acta_db_model_create(db, &m, &id2);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 4.11: model_get — existing ---------- */
static void test_model_get_existing(void) {
    const char *path = "test/acta_test_m_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "Test", .backend = "openai", .model_identifier = "gpt-4", .description = "desc" };
    int id;
    acta_db_model_create(db, &m, &id);
    model_t *got = acta_db_model_get(db, id);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, id);
    TEST_ASSERT_EQ_STR(got->name, "Test");
    TEST_ASSERT_EQ_STR(got->backend, "openai");
    TEST_ASSERT_EQ_STR(got->model_identifier, "gpt-4");
    TEST_ASSERT_EQ_STR(got->description, "desc");
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.12: model_get — non-existent ---------- */
static void test_model_get_nonexistent(void) {
    const char *path = "test/acta_test_m_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t *got = acta_db_model_get(db, 999999);
    TEST_ASSERT_NULL(got);
    test_db_teardown(db, path);
}

/* ---------- 4.13: model_get_live — live ---------- */
static void test_model_get_live_live(void) {
    const char *path = "test/acta_test_m_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "L", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    model_t *got = acta_db_model_get_live(db, id);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_NULL(got->deleted_at);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.14: model_get_live — soft-deleted ---------- */
static void test_model_get_live_deleted(void) {
    const char *path = "test/acta_test_m_livedel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "L", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);
    model_t *got = acta_db_model_get_live(db, id);
    TEST_ASSERT_NULL(got);
    test_db_teardown(db, path);
}

/* ---------- 4.15: model_update — change name ---------- */
static void test_model_update_name(void) {
    const char *path = "test/acta_test_m_upname.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "Old", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);

    model_t update = { .id = id, .name = "New", .backend = "b", .model_identifier = "mid" };
    int rc = acta_db_model_update(db, &update);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, id, &count);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.16: model_update — change backend ---------- */
static void test_model_update_backend(void) {
    const char *path = "test/acta_test_m_upbe.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "openai", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);

    model_t update = { .id = id, .name = "M", .backend = "anthropic", .model_identifier = "mid" };
    int rc = acta_db_model_update(db, &update);
    TEST_ASSERT_EQ_INT(rc, 0);
    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, id, &count);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.17: model_update — change configuration ---------- */
static void test_model_update_config(void) {
    const char *path = "test/acta_test_m_upconf.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid", .configuration = "{}" };
    int id;
    acta_db_model_create(db, &m, &id);

    model_t update = { .id = id, .name = "M", .backend = "b", .model_identifier = "mid", .configuration = "{\"temp\":0.5}" };
    int rc = acta_db_model_update(db, &update);
    TEST_ASSERT_EQ_INT(rc, 0);
    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, id, &count);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.18: model_update — no actual change ---------- */
static void test_model_update_no_change(void) {
    const char *path = "test/acta_test_m_upnochange.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);

    model_t update = { .id = id, .name = "M", .backend = "b", .model_identifier = "mid" };
    int rc = acta_db_model_update(db, &update);
    /* Should succeed but NOT create a new revision */
    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, id, &count);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.19: model_update — current_revision updated ---------- */
static void test_model_update_current_rev(void) {
    const char *path = "test/acta_test_m_upcurrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);

    model_t update = { .id = id, .name = "Changed", .backend = "b", .model_identifier = "mid" };
    acta_db_model_update(db, &update);

    model_t *got = acta_db_model_get(db, id);
    TEST_ASSERT_EQ_INT(got->current_revision, 2);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.20: model_update — on soft-deleted model ---------- */
static void test_model_update_deleted(void) {
    const char *path = "test/acta_test_m_updel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);

    model_t update = { .id = id, .name = "X", .backend = "b", .model_identifier = "mid" };
    acta_db_model_update(db, &update);

    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, id, &count);
    /* Should NOT have created a new live revision */
    /* count should still be 2 (initial + delete revision) */
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.21: model_soft_delete — happy ---------- */
static void test_model_soft_delete_happy(void) {
    const char *path = "test/acta_test_m_sd.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    int rc = acta_db_model_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, 0);
    model_t *got = acta_db_model_get(db, id);
    TEST_ASSERT_NOT_NULL(got->deleted_at);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.22: model_soft_delete — revision with deleted_at ---------- */
static void test_model_soft_delete_revision(void) {
    const char *path = "test/acta_test_m_sdrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);

    int count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, id, &count);
    TEST_ASSERT_EQ_INT(count, 2);
    /* Last revision should have deleted_at set */
    if (count >= 2) {
        TEST_ASSERT_NOT_NULL(revs[1].deleted_at);
    }
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.23: model_soft_delete — current_revision NOT updated ---------- */
static void test_model_soft_delete_current_rev(void) {
    const char *path = "test/acta_test_m_sdcurrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);

    model_t *got = acta_db_model_get(db, id);
    /* current_revision should still be 1 (the last live revision) */
    TEST_ASSERT_EQ_INT(got->current_revision, 1);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.24: model_list_in_folder — root ---------- */
static void test_model_list_in_folder_root(void) {
    const char *path = "test/acta_test_m_listroot.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "RootModel", .backend = "b", .model_identifier = "mid" };
    acta_db_model_create(db, &m, &(int){0});
    int count = 0;
    model_t *items = acta_db_model_list_in_folder(db, 0, &count);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.25: model_list_in_folder — specific ---------- */
static void test_model_list_in_folder_specific(void) {
    const char *path = "test/acta_test_m_listspec.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int f1, f2;
    acta_db_model_folder_create(db, "A", 0, &f1);
    acta_db_model_folder_create(db, "B", 0, &f2);
    model_t ma = { .name = "InA", .backend = "b", .model_identifier = "mid", .folder_id = f1 };
    model_t mb = { .name = "InB", .backend = "b", .model_identifier = "mid", .folder_id = f2 };
    acta_db_model_create(db, &ma, &(int){0});
    acta_db_model_create(db, &mb, &(int){0});

    int count = 0;
    model_t *items = acta_db_model_list_in_folder(db, f1, &count);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.26: model_list_in_folder — empty ---------- */
static void test_model_list_in_folder_empty(void) {
    const char *path = "test/acta_test_m_listempty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int fid;
    acta_db_model_folder_create(db, "Empty", 0, &fid);
    int count = 0;
    model_t *items = acta_db_model_list_in_folder(db, fid, &count);
    TEST_ASSERT_EQ_INT(count, 0);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.27: model_list_all — excludes deleted ---------- */
static void test_model_list_all_excludes_deleted(void) {
    const char *path = "test/acta_test_m_ladel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m1 = { .name = "A", .backend = "b", .model_identifier = "mid" };
    model_t m2 = { .name = "B", .backend = "b", .model_identifier = "mid" };
    model_t m3 = { .name = "C", .backend = "b", .model_identifier = "mid" };
    int id1, id2, id3;
    acta_db_model_create(db, &m1, &id1);
    acta_db_model_create(db, &m2, &id2);
    acta_db_model_create(db, &m3, &id3);
    acta_db_model_soft_delete(db, id2);
    int count = 0;
    model_t *items = acta_db_model_list_all(db, &count);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.28-4.30: free / list_free ---------- */
static void test_model_free_valid(void) {
    const char *path = "test/acta_test_m_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    model_t *got = acta_db_model_get(db, id);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

static void test_model_free_null(void) {
    acta_db_model_free(NULL);
    TEST_ASSERT(1);
}

static void test_model_list_free_valid(void) {
    const char *path = "test/acta_test_m_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Model%d", i);
        model_t m = { .name = name, .backend = "b", .model_identifier = "mid" };
        acta_db_model_create(db, &m, &(int){0});
    }
    int count = 0;
    model_t *items = acta_db_model_list_all(db, &count);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

void run_model_tests(void) {
    fprintf(stderr, "\n=== model tests ===\n");
    test_model_create_root();
    test_model_create_in_folder();
    test_model_create_initial_revision();
    test_model_create_current_rev();
    test_model_create_null_name();
    test_model_create_null_backend();
    test_model_create_null_mid();
    test_model_create_invalid_folder();
    test_model_create_dup_root();
    test_model_create_dup_child();
    test_model_get_existing();
    test_model_get_nonexistent();
    test_model_get_live_live();
    test_model_get_live_deleted();
    test_model_update_name();
    test_model_update_backend();
    test_model_update_config();
    test_model_update_no_change();
    test_model_update_current_rev();
    test_model_update_deleted();
    test_model_soft_delete_happy();
    test_model_soft_delete_revision();
    test_model_soft_delete_current_rev();
    test_model_list_in_folder_root();
    test_model_list_in_folder_specific();
    test_model_list_in_folder_empty();
    test_model_list_all_excludes_deleted();
    test_model_free_valid();
    test_model_free_null();
    test_model_list_free_valid();
}
