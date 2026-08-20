#include "test_common.h"
#include "db.h"

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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
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
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, model_id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 1);
    if (count > 0) {
        TEST_ASSERT_EQ_INT(revs[0]->revision, 1);
    }
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.4: model_create — NULL name ---------- */
static void test_model_create_null_name(void) {
    const char *path = "test/acta_test_m_nullname.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = NULL, .backend = "b", .model_identifier = "mid" };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
    test_db_teardown(db, path);
}

/* ---------- 4.5: model_create — NULL backend ---------- */
static void test_model_create_null_backend(void) {
    const char *path = "test/acta_test_m_nullbe.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "N", .backend = NULL, .model_identifier = "mid" };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
    test_db_teardown(db, path);
}

/* ---------- 4.6: model_create — NULL model_identifier ---------- */
static void test_model_create_null_mid(void) {
    const char *path = "test/acta_test_m_nullmid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "N", .backend = "b", .model_identifier = NULL };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
    test_db_teardown(db, path);
}

/* ---------- 4.7: model_create — invalid folder_id ---------- */
static void test_model_create_invalid_folder(void) {
    const char *path = "test/acta_test_m_badfolder.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "N", .backend = "b", .model_identifier = "mid", .folder_id = 999999 };
    int id;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);
    test_db_teardown(db, path);
}

/* ---------- 4.8: model_create — duplicate name (root) ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);
    test_db_teardown(db, path);
}

/* ---------- 4.9: model_create — duplicate name (child) ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);
    test_db_teardown(db, path);
}

/* ---------- 4.10: model_get — existing ---------- */
static void test_model_get_existing(void) {
    const char *path = "test/acta_test_m_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "Test", .backend = "openai", .model_identifier = "gpt-4", .description = "desc" };
    int id;
    acta_db_model_create(db, &m, &id);

    int err = 0;
    model_t *got = acta_db_model_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(got->id, id);
    TEST_ASSERT_EQ_STR(got->name, "Test");
    TEST_ASSERT_EQ_STR(got->backend, "openai");
    TEST_ASSERT_EQ_STR(got->model_identifier, "gpt-4");
    TEST_ASSERT_EQ_STR(got->description, "desc");
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.11: model_get — non-existent ---------- */
static void test_model_get_nonexistent(void) {
    const char *path = "test/acta_test_m_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = -1;
    model_t *got = acta_db_model_get(db, 999999, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);  /* not-found is still "ok" */
    test_db_teardown(db, path);
}

/* ---------- 4.12: model_get_live — live ---------- */
static void test_model_get_live_live(void) {
    const char *path = "test/acta_test_m_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "L", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);

    int err = 0;
    model_t *got = acta_db_model_get_live(db, id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(got->deleted_at);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.13: model_get_live — soft-deleted ---------- */
static void test_model_get_live_deleted(void) {
    const char *path = "test/acta_test_m_livedel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "L", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);

    int err = -1;
    model_t *got = acta_db_model_get_live(db, id, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);  /* not-found (deleted) is still "ok" */
    test_db_teardown(db, path);
}

/* ---------- 4.14: model_update — change name ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int count = 0;
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.15: model_update — change backend ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    int count = 0;
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.16: model_update — change configuration ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    int count = 0;
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.17: model_update — no actual change ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    /* No new revision created */
    int count = 0;
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.18: model_update — on soft-deleted model ---------- */
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
    int rc = acta_db_model_update(db, &update);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);

    int count = 0;
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.19: model_soft_delete — happy ---------- */
static void test_model_soft_delete_happy(void) {
    const char *path = "test/acta_test_m_sd.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    int rc = acta_db_model_soft_delete(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    model_t *got = acta_db_model_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got->deleted_at);
    acta_db_model_free(got);
    test_db_teardown(db, path);
}

/* ---------- 4.20: model_soft_delete — revision with deleted_at ---------- */
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
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, id, &count, NULL);
    TEST_ASSERT_EQ_INT(count, 2);
    if (count >= 2) {
        TEST_ASSERT_NOT_NULL(revs[1]->deleted_at);
    }
    acta_db_model_revision_list_free(revs, count);
    test_db_teardown(db, path);
}

/* ---------- 4.21: model_list_in_folder — root ---------- */
static void test_model_list_in_folder_root(void) {
    const char *path = "test/acta_test_m_listroot.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "RootModel", .backend = "b", .model_identifier = "mid" };
    acta_db_model_create(db, &m, &(int){0});
    int count = 0;
    int err = 0;
    model_t **items = acta_db_model_list_in_folder(db, 0, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.22: model_list_in_folder — specific ---------- */
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
    int err = 0;
    model_t **items = acta_db_model_list_in_folder(db, f1, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.23: model_list_in_folder — empty ---------- */
static void test_model_list_in_folder_empty(void) {
    const char *path = "test/acta_test_m_listempty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int fid;
    acta_db_model_folder_create(db, "Empty", 0, &fid);
    int count = 0;
    int err = 0;
    model_t **items = acta_db_model_list_in_folder(db, fid, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.24: model_list_all — excludes deleted ---------- */
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
    int err = 0;
    model_t **items = acta_db_model_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.25-4.27: free / list_free ---------- */
static void test_model_free_valid(void) {
    const char *path = "test/acta_test_m_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    model_t *got = acta_db_model_get(db, id, NULL);
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
    int err = 0;
    model_t **items = acta_db_model_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    acta_db_model_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 4.28: model_restore — happy path ---------- */
static void test_model_restore_happy(void) {
    const char *path = "test/acta_test_m_restore.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);

    int rc = acta_db_model_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    model_t *got = acta_db_model_get_live(db, id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(got->deleted_at);
    acta_db_model_free(got);

    test_db_teardown(db, path);
}

/* ---------- 4.29: model_restore — already live ---------- */
static void test_model_restore_already_live(void) {
    const char *path = "test/acta_test_m_restore_live.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);

    /* WHERE deleted_at IS NOT NULL matches 0 rows → NOT_FOUND */
    int rc = acta_db_model_restore(db, id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 4.30: model_restore — non-existent ---------- */
static void test_model_restore_nonexistent(void) {
    const char *path = "test/acta_test_m_restore_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_model_restore(db, 999999);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 4.31: model_restore — appears in list_all after restore ---------- */
static void test_model_restore_in_list(void) {
    const char *path = "test/acta_test_m_restore_list.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_t m1 = { .name = "A", .backend = "b", .model_identifier = "mid" };
    model_t m2 = { .name = "B", .backend = "b", .model_identifier = "mid" };
    int id1, id2;
    acta_db_model_create(db, &m1, &id1);
    acta_db_model_create(db, &m2, &id2);

    acta_db_model_soft_delete(db, id2);

    int count = 0;
    int err = 0;
    model_t **items = acta_db_model_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_list_free(items, count);

    acta_db_model_restore(db, id2);

    count = 0;
    err = 0;
    items = acta_db_model_list_all(db, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 4.32: model_move_to_folder — to specific folder ---------- */
static void test_model_move_to_folder_specific(void) {
    const char *path = "test/acta_test_m_move_spec.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int f1, f2;
    acta_db_model_folder_create(db, "FolderA", 0, &f1);
    acta_db_model_folder_create(db, "FolderB", 0, &f2);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid", .folder_id = f1 };
    int id;
    acta_db_model_create(db, &m, &id);

    int rc = acta_db_model_move_to_folder(db, id, f2);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    int count1 = 0;
    model_t **items1 = acta_db_model_list_in_folder(db, f1, 0, -1, &count1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count1, 0);
    acta_db_model_list_free(items1, count1);

    count1 = 0;
    err = 0;
    model_t **items2 = acta_db_model_list_in_folder(db, f2, 0, -1, &count1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count1, 1);
    TEST_ASSERT_EQ_INT(items2[0]->id, id);
    acta_db_model_list_free(items2, count1);

    test_db_teardown(db, path);
}

/* ---------- 4.33: model_move_to_folder — to root (folder_id = 0) ---------- */
static void test_model_move_to_folder_root(void) {
    const char *path = "test/acta_test_m_move_root.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int fid;
    acta_db_model_folder_create(db, "F", 0, &fid);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid", .folder_id = fid };
    int id;
    acta_db_model_create(db, &m, &id);

    int rc = acta_db_model_move_to_folder(db, id, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    int count = 0;
    model_t **items = acta_db_model_list_in_folder(db, 0, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id);
    acta_db_model_list_free(items, count);

    count = 0;
    err = 0;
    model_t **old = acta_db_model_list_in_folder(db, fid, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    acta_db_model_list_free(old, count);

    test_db_teardown(db, path);
}

/* ---------- 4.34: model_move_to_folder — soft-deleted model ---------- */
static void test_model_move_deleted(void) {
    const char *path = "test/acta_test_m_move_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int fid;
    acta_db_model_folder_create(db, "F", 0, &fid);

    model_t m = { .name = "M", .backend = "b", .model_identifier = "mid" };
    int id;
    acta_db_model_create(db, &m, &id);
    acta_db_model_soft_delete(db, id);

    int rc = acta_db_model_move_to_folder(db, id, fid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 4.35: model_move_to_folder — non-existent model ---------- */
static void test_model_move_nonexistent(void) {
    const char *path = "test/acta_test_m_move_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_model_move_to_folder(db, 999999, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 4.36: model_list_in_folder — pagination (offset) ---------- */
static void test_model_list_in_folder_pagination_offset(void) {
    const char *path = "test/acta_test_m_pagen_off.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create 3 models in root */
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Pag%d", i);
        model_t m = { .name = name, .backend = "b", .model_identifier = "mid" };
        acta_db_model_create(db, &m, &(int){0});
    }

    /* offset=1, limit=1 → should return only the 2nd model */
    int count = 0;
    int err = 0;
    model_t **items = acta_db_model_list_in_folder(db, 0, 1, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Pag1");
    acta_db_model_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 4.37: model_list_in_folder — pagination (limit) ---------- */
static void test_model_list_in_folder_pagination_limit(void) {
    const char *path = "test/acta_test_m_pagen_lim.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create 5 models in root */
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Lim%d", i);
        model_t m = { .name = name, .backend = "b", .model_identifier = "mid" };
        acta_db_model_create(db, &m, &(int){0});
    }

    /* offset=0, limit=2 → first 2 only */
    int count = 0;
    int err = 0;
    model_t **items = acta_db_model_list_in_folder(db, 0, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Lim0");
    TEST_ASSERT_EQ_STR(items[1]->name, "Lim1");
    acta_db_model_list_free(items, count);

    /* offset=2, limit=2 → items 2 and 3 */
    count = 0;
    err = 0;
    items = acta_db_model_list_in_folder(db, 0, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_STR(items[0]->name, "Lim2");
    TEST_ASSERT_EQ_STR(items[1]->name, "Lim3");
    acta_db_model_list_free(items, count);

    /* offset=4, limit=2 → only 1 remaining */
    count = 0;
    err = 0;
    items = acta_db_model_list_in_folder(db, 0, 4, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_STR(items[0]->name, "Lim4");
    acta_db_model_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 4.38: model_list_all — pagination ---------- */
static void test_model_list_all_pagination(void) {
    const char *path = "test/acta_test_m_pagen_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create 4 models in root */
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "All%d", i);
        model_t m = { .name = name, .backend = "b", .model_identifier = "mid" };
        acta_db_model_create(db, &m, &(int){0});
    }

    /* offset=0, limit=2 */
    int count = 0;
    int err = 0;
    model_t **items = acta_db_model_list_all(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_list_free(items, count);

    /* offset=2, limit=-1 (remaining) */
    count = 0;
    err = 0;
    items = acta_db_model_list_all(db, 2, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 4.39: model_folder_list_children — pagination ---------- */
static void test_model_folder_list_children_pagination(void) {
    const char *path = "test/acta_test_mf_pagen.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create 3 child folders under root */
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Child%d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }

    /* offset=0, limit=2 → first 2 children */
    int count = 0;
    int err = 0;
    model_folder_t **items = acta_db_model_folder_list_children(db, 0, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_model_folder_list_free(items, count);

    /* offset=2, limit=-1 → remaining 1 */
    count = 0;
    err = 0;
    items = acta_db_model_folder_list_children(db, 0, 2, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_folder_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 4.40: model_folder_list_all — pagination ---------- */
static void test_model_folder_list_all_pagination(void) {
    const char *path = "test/acta_test_mf_pagen_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create 4 folders at root */
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Folder%d", i);
        acta_db_model_folder_create(db, name, 0, &(int){0});
    }

    /* offset=0, limit=3 */
    int count = 0;
    int err = 0;
    model_folder_t **items = acta_db_model_folder_list_all(db, 0, 3, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_folder_list_free(items, count);

    /* offset=3, limit=-1 → last 1 */
    count = 0;
    err = 0;
    items = acta_db_model_folder_list_all(db, 3, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_model_folder_list_free(items, count);

    test_db_teardown(db, path);
}

void run_model_tests(void) {
    fprintf(stderr, "\n=== model tests ===\n");
    test_model_create_root();
    test_model_create_in_folder();
    test_model_create_initial_revision();
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
    test_model_update_deleted();
    test_model_soft_delete_happy();
    test_model_soft_delete_revision();
    test_model_list_in_folder_root();
    test_model_list_in_folder_specific();
    test_model_list_in_folder_empty();
    test_model_list_all_excludes_deleted();
    test_model_free_valid();
    test_model_free_null();
    test_model_list_free_valid();
    /* restore */
    test_model_restore_happy();
    test_model_restore_already_live();
    test_model_restore_nonexistent();
    test_model_restore_in_list();
    /* move_to_folder */
    test_model_move_to_folder_specific();
    test_model_move_to_folder_root();
    test_model_move_deleted();
    test_model_move_nonexistent();
    /* pagination */
    test_model_list_in_folder_pagination_offset();
    test_model_list_in_folder_pagination_limit();
    test_model_list_all_pagination();
    test_model_folder_list_children_pagination();
    test_model_folder_list_all_pagination();
}
