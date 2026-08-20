#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>

/* ---------- 9.1: create — happy ---------- */
static void test_exec_create_happy(void) {
    const char *path = "test/acta_test_exec_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.prompt              = "Hello";
    e.status              = ACTA_EXEC_STATUS_PENDING;
    e.parent_execution_id = 0;

    int out_id = 0;
    int rc = acta_db_execution_create(db, &e, &out_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT(out_id > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, out_id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.2: create — invalid context_id ---------- */
static void test_exec_create_invalid_ctx(void) {
    const char *path = "test/acta_test_exec_inv_ctx.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = 999999;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.prompt              = "Hello";
    e.parent_execution_id = 0;

    int out_id = 0;
    int rc = acta_db_execution_create(db, &e, &out_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.3: create — invalid skill_revision_id ---------- */
static void test_exec_create_invalid_sr(void) {
    const char *path = "test/acta_test_exec_inv_sr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = 999999;
    e.model_revision_id   = mr_id;
    e.prompt              = "Hello";
    e.parent_execution_id = 0;

    int out_id = 0;
    int rc = acta_db_execution_create(db, &e, &out_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.4: create — invalid model_revision_id ---------- */
static void test_exec_create_invalid_mr(void) {
    const char *path = "test/acta_test_exec_inv_mr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = 999999;
    e.prompt              = "Hello";
    e.parent_execution_id = 0;

    int out_id = 0;
    int rc = acta_db_execution_create(db, &e, &out_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.5: create — NULL prompt ---------- */
static void test_exec_create_null_prompt(void) {
    const char *path = "test/acta_test_exec_null_prompt.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.prompt              = NULL;
    e.status              = ACTA_EXEC_STATUS_PENDING;
    e.parent_execution_id = 0;

    int out_id = 0;
    int rc = acta_db_execution_create(db, &e, &out_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT(out_id > 0);

    test_db_teardown(db, path);
}

/* ---------- 9.6: create — default status ---------- */
static void test_exec_create_default_status(void) {
    const char *path = "test/acta_test_exec_def_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "Hi", 0);
    TEST_ASSERT(eid > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    TEST_ASSERT(got->started_at == NULL);
    TEST_ASSERT(got->completed_at == NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.7: create — with parent ---------- */
static void test_exec_create_with_parent(void) {
    const char *path = "test/acta_test_exec_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent_id = exec_create(db, ctx_id, sr_id, mr_id, "Parent", 0);
    TEST_ASSERT(parent_id > 0);

    int child_id = exec_create(db, ctx_id, sr_id, mr_id, "Child", parent_id);
    TEST_ASSERT(child_id > 0);

    int err = 0;
    execution_t *child = acta_db_execution_get(db, child_id, &err);
    TEST_ASSERT_NOT_NULL(child);
    TEST_ASSERT_EQ_INT(child->parent_execution_id, parent_id);
    acta_db_execution_free(child);

    test_db_teardown(db, path);
}

/* ---------- 9.8: create — invalid parent_execution_id ---------- */
static void test_exec_create_invalid_parent(void) {
    const char *path = "test/acta_test_exec_inv_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.prompt              = "Hello";
    e.parent_execution_id = 999999;

    int out_id = 0;
    int rc = acta_db_execution_create(db, &e, &out_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.9: get — existing ---------- */
static void test_exec_get_existing(void) {
    const char *path = "test/acta_test_exec_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "GetPrompt", 0);
    TEST_ASSERT(eid > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, eid);
    TEST_ASSERT_EQ_INT(got->context_id, ctx_id);
    TEST_ASSERT_EQ_INT(got->skill_revision_id, sr_id);
    TEST_ASSERT_EQ_INT(got->model_revision_id, mr_id);
    TEST_ASSERT_EQ_STR(got->prompt, "GetPrompt");
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.10: get — non-existent ---------- */
static void test_exec_get_nonexistent(void) {
    const char *path = "test/acta_test_exec_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, 999999, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* NULL-err guard: must not crash, still returns NULL */
    execution_t *got2 = acta_db_execution_get(db, 999999, NULL);
    TEST_ASSERT_NULL(got2);

    test_db_teardown(db, path);
}


/* ---------- free / raw ---------- */
/* ---------- 9.33: free — valid ---------- */
static void test_exec_free_valid(void) {
    const char *path = "test/acta_test_exec_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "FreeTest", 0);
    TEST_ASSERT(eid > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    acta_db_execution_free(got);
    TEST_ASSERT(1);

    test_db_teardown(db, path);
}

/* ---------- 9.34: free — NULL ---------- */
static void test_exec_free_null(void) {
    acta_db_execution_free(NULL);
    TEST_ASSERT(1);
}

/* ---------- 9.35: list_free — valid ---------- */
static void test_exec_list_free_valid(void) {
    const char *path = "test/acta_test_exec_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* Create 3 executions */
    for (int i = 0; i < 3; i++) {
        char prompt[16];
        snprintf(prompt, sizeof(prompt), "L%d", i);
        TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, prompt, 0) > 0);
    }

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_PENDING, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 9.36: raw insert — FK bypass (validates exec_insert_raw helper) ---------- */
static void test_exec_raw_insert(void) {
    const char *path = "test/acta_test_exec_raw_insert.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* No setup — just insert with fake FK values, FK checks disabled */
    int rc = exec_insert_raw(db, 42, 9999, 9999, 9999, 0);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    /* Verify it was inserted */
    int err = 0;
    execution_t *got = acta_db_execution_get(db, 42, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, 42);
    TEST_ASSERT_EQ_INT(got->context_id, 9999);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- runner ---------- */
void run_execution_create_tests(void) {
    fprintf(stderr, "\n=== execution create/get/free tests ===\n");
    test_exec_create_happy();
    test_exec_create_invalid_ctx();
    test_exec_create_invalid_sr();
    test_exec_create_invalid_mr();
    test_exec_create_null_prompt();
    test_exec_create_default_status();
    test_exec_create_with_parent();
    test_exec_create_invalid_parent();
    test_exec_get_existing();
    test_exec_get_nonexistent();
    test_exec_free_valid();
    test_exec_free_null();
    test_exec_list_free_valid();
    test_exec_raw_insert();
}
