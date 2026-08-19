/* test_model_revision.c — Tests for model_revision.h (tests 5.1 – 5.15) */

#include "test_common.h"
#include "model_revision.h"

/* Short helper to keep test bodies readable (model create has many params) */
static int mr_create_model(db_t *db, const char *name) {
    model_t m;
    memset(&m, 0, sizeof(m));
    m.name = (char *)name;
    m.description = (char *)"desc";
    m.backend = (char *)"openai";
    m.base_url = (char *)"http://localhost:8080";
    m.model_identifier = (char *)"model-id";
    m.configuration = (char *)"{ }";
    m.folder_id = 0;

    int id = 0;
    int rc = acta_db_model_create(db, &m, &id);
    return (rc == 0) ? id : -1;
}


static int mr_update_model(db_t *db, int model_id, const char *name) {
    model_t m;
    memset(&m, 0, sizeof(m));
    m.id = model_id;
    m.name = (char *)name;
    m.description = (char *)"desc";
    m.backend = (char *)"openai";
    m.base_url = (char *)"http://localhost:8080";
    m.model_identifier = (char *)"model-id";
    m.configuration = (char *)"{ }";

    return acta_db_model_update(db, &m);
}


/* ---------- 5.1: get — existing ---------- */
static void test_mr_get_existing(void) {
    const char *path = "test/acta_test_mr_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel");
    TEST_ASSERT(model_id > 0);

    /* Grab rev 1 by (model_id, 1), then fetch by its row id */
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1);
    TEST_ASSERT_NOT_NULL(rev);
    int rev_id = rev->id;
    acta_db_model_revision_free(rev);

    model_revision_t *by_id = acta_db_model_revision_get(db, rev_id);
    TEST_ASSERT_NOT_NULL(by_id);
    TEST_ASSERT_EQ_INT(by_id->id, rev_id);
    TEST_ASSERT_EQ_INT(by_id->model_id, model_id);
    TEST_ASSERT_EQ_INT(by_id->revision, 1);
    TEST_ASSERT_EQ_STR(by_id->name, "RevModel");
    TEST_ASSERT(by_id->deleted_at == NULL);
    acta_db_model_revision_free(by_id);

    test_db_teardown(db, path);
}

/* ---------- 5.2: get — non-existent ---------- */
static void test_mr_get_nonexistent(void) {
    const char *path = "test/acta_test_mr_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_revision_t *rev = acta_db_model_revision_get(db, 999999);
    TEST_ASSERT_NULL(rev);

    test_db_teardown(db, path);
}

/* ---------- 5.3: get_by_model_and_rev — existing ---------- */
static void test_mr_get_by_model_rev_existing(void) {
    const char *path = "test/acta_test_mr_gbmrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel53");
    TEST_ASSERT(model_id > 0);

    int rc = mr_update_model(db, model_id, "Updated");
    TEST_ASSERT_EQ_INT(rc, 0);

    model_revision_t *rev2 = acta_db_model_revision_get_by_model_and_rev(db, model_id, 2);
    TEST_ASSERT_NOT_NULL(rev2);
    TEST_ASSERT_EQ_INT(rev2->model_id, model_id);
    TEST_ASSERT_EQ_INT(rev2->revision, 2);
    TEST_ASSERT_EQ_STR(rev2->name, "Updated");
    acta_db_model_revision_free(rev2);

    test_db_teardown(db, path);
}

/* ---------- 5.4: get_by_model_and_rev — missing ---------- */
static void test_mr_get_by_model_rev_missing(void) {
    const char *path = "test/acta_test_mr_gbmrev_miss.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel54");
    TEST_ASSERT(model_id > 0);

    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 99);
    TEST_ASSERT_NULL(rev);

    test_db_teardown(db, path);
}

/* ---------- 5.5: get_by_model_and_rev — deleted revision ---------- */
static void test_mr_get_by_model_rev_deleted(void) {
    const char *path = "test/acta_test_mr_gbmrev_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel55");
    TEST_ASSERT(model_id > 0);

    int rc = acta_db_model_soft_delete(db, model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Deleted revision is row 2 (rev 1 = create, rev 2 = delete) */
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 2);
    TEST_ASSERT_NOT_NULL(rev);
    TEST_ASSERT_EQ_INT(rev->model_id, model_id);
    TEST_ASSERT_EQ_INT(rev->revision, 2);
    TEST_ASSERT_NOT_NULL(rev->deleted_at);
    acta_db_model_revision_free(rev);

    test_db_teardown(db, path);
}

/* ---------- 5.6: list_by_model — multiple revs ---------- */
static void test_mr_list_multiple(void) {
    const char *path = "test/acta_test_mr_list.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel56");
    TEST_ASSERT(model_id > 0);
    mr_update_model(db, model_id, "v2");
    mr_update_model(db, model_id, "v3");

    int count = 0;
    model_revision_t *items = acta_db_model_revision_list_by_model(db, model_id, &count);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQ_INT(items[i].model_id, model_id);
    }
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 5.7: list_by_model — ordering ---------- */
static void test_mr_list_ordering(void) {
    const char *path = "test/acta_test_mr_list_order.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel57");
    TEST_ASSERT(model_id > 0);
    mr_update_model(db, model_id, "v2");
    mr_update_model(db, model_id, "v3");

    int count = 0;
    model_revision_t *items = acta_db_model_revision_list_by_model(db, model_id, &count);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);

    /* Ascending revision order: 1, 2, 3 */
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQ_INT(items[i].revision, i + 1);
    }
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 5.8: list_by_model — includes deleted ---------- */
static void test_mr_list_includes_deleted(void) {
    const char *path = "test/acta_test_mr_list_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel58");
    TEST_ASSERT(model_id > 0);
    mr_update_model(db, model_id, "v2");

    int rc = acta_db_model_soft_delete(db, model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count = 0;
    model_revision_t *items = acta_db_model_revision_list_by_model(db, model_id, &count);
    TEST_ASSERT_NOT_NULL(items);
    /* 3 rows: rev1 (create), rev2 (update), rev3 (delete) */
    TEST_ASSERT_EQ_INT(count, 3);

    /* Last row is the deleted revision */
    TEST_ASSERT_NOT_NULL(items[2].deleted_at);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 5.9: free — valid ---------- */
static void test_mr_free_valid(void) {
    const char *path = "test/acta_test_mr_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel59");
    TEST_ASSERT(model_id > 0);

    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1);
    TEST_ASSERT_NOT_NULL(rev);
    acta_db_model_revision_free(rev);

    test_db_teardown(db, path);
}

/* ---------- 5.10: free — NULL ---------- */
static void test_mr_free_null(void) {
    acta_db_model_revision_free(NULL);
    TEST_ASSERT(1);
}

/* ---------- 5.11: list_free — valid ---------- */
static void test_mr_list_free_valid(void) {
    const char *path = "test/acta_test_mr_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel511");
    TEST_ASSERT(model_id > 0);
    mr_update_model(db, model_id, "v2");
    mr_update_model(db, model_id, "v3");

    int count = 0;
    model_revision_t *items = acta_db_model_revision_list_by_model(db, model_id, &count);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 5.12: get_latest — multiple revisions ---------- */
static void test_mr_get_latest_multi(void) {
    const char *path = "test/acta_test_mr_latest_multi.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel512");
    TEST_ASSERT(model_id > 0);
    mr_update_model(db, model_id, "v2");
    mr_update_model(db, model_id, "v3");

    model_revision_t *latest = acta_db_model_revision_get_latest(db, model_id);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(latest->model_id, model_id);
    TEST_ASSERT_EQ_INT(latest->revision, 3);
    TEST_ASSERT_EQ_STR(latest->name, "v3");
    acta_db_model_revision_free(latest);

    test_db_teardown(db, path);
}

/* ---------- 5.13: get_latest — single revision ---------- */
static void test_mr_get_latest_single(void) {
    const char *path = "test/acta_test_mr_latest_single.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_model(db, "RevModel513");
    TEST_ASSERT(model_id > 0);

    model_revision_t *latest = acta_db_model_revision_get_latest(db, model_id);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(latest->model_id, model_id);
    TEST_ASSERT_EQ_INT(latest->revision, 1);
    TEST_ASSERT_EQ_STR(latest->name, "RevModel513");
    acta_db_model_revision_free(latest);

    test_db_teardown(db, path);
}

/* ---------- 5.14: get_latest — non-existent model ---------- */
static void test_mr_get_latest_nonexistent(void) {
    const char *path = "test/acta_test_mr_latest_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    model_revision_t *latest = acta_db_model_revision_get_latest(db, 999999);
    TEST_ASSERT_NULL(latest);

    test_db_teardown(db, path);
}

/* ---------- 5.15: get_latest — NULL db ---------- */
static void test_mr_get_latest_null_db(void) {
    model_revision_t *latest = acta_db_model_revision_get_latest(NULL, 1);
    TEST_ASSERT_NULL(latest);
}

/* ---------- runner ---------- */
void run_model_revision_tests(void) {
    fprintf(stderr, "\n=== model_revision tests ===\n");
    test_mr_get_existing();
    test_mr_get_nonexistent();
    test_mr_get_by_model_rev_existing();
    test_mr_get_by_model_rev_missing();
    test_mr_get_by_model_rev_deleted();
    test_mr_list_multiple();
    test_mr_list_ordering();
    test_mr_list_includes_deleted();
    test_mr_free_valid();
    test_mr_free_null();
    test_mr_list_free_valid();
    test_mr_get_latest_multi();
    test_mr_get_latest_single();
    test_mr_get_latest_nonexistent();
    test_mr_get_latest_null_db();
}
