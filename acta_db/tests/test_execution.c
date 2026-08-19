/* test_execution.c — Tests for execution.h (tests 9.1 – 9.39) */

#include "test_common.h"
#include "execution.h"
#include "skill_revision.h"
#include "model_revision.h"
#include "context.h"

/* ---------- helpers ---------- */

/* Insert an execution row directly via SQL, temporarily disabling FK checks.
 * Useful for seeding rows without requiring valid FK targets.
 * Returns 0 on success, non-zero on failure. */
static int exec_insert_raw(db_t *db, int execution_id, int ctx_id,
                           int sr_id, int mr_id, int parent_id) {
    acta_db_exec(db, "PRAGMA foreign_keys=OFF;");
    char sql[256];
    if (parent_id > 0) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO executions (id, context_id, skill_revision_id, "
                 "model_revision_id, parent_execution_id) "
                 "VALUES (%d, %d, %d, %d, %d);",
                 execution_id, ctx_id, sr_id, mr_id, parent_id);
    } else {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO executions (id, context_id, skill_revision_id, "
                 "model_revision_id) "
                 "VALUES (%d, %d, %d, %d);",
                 execution_id, ctx_id, sr_id, mr_id);
    }
    int rc = acta_db_exec(db, sql);
    acta_db_exec(db, "PRAGMA foreign_keys=ON;");
    return rc;
}

/* Create a minimal valid context + skill_revision + model_revision.
 * Returns 0 on success, fills out_* with the IDs needed for execution_create. */
static int exec_setup(db_t *db, int *out_ctx, int *out_sr, int *out_mr) {
    /* --- context --- */
    context_t ctx = {0};
    ctx.type         = (char *)"test";
    ctx.content      = (char *)"hello";
    ctx.content_hash = (char *)"deadbeef";
    int ctx_id = 0;
    if (acta_db_context_create(db, &ctx, &ctx_id) != ACTA_DB_OK) return -1;

    /* --- skill --- */
    skill_t sk = {0};
    sk.name            = (char *)"ExecSkill";
    sk.description     = (char *)"desc";
    sk.prompt_template = (char *)"You are helpful.";
    sk.output_schema   = (char *)"json";
    int skill_id = 0;
    if (acta_db_skill_create(db, &sk, &skill_id) != ACTA_DB_OK) return -1;
    int err = 0;
    skill_revision_t *srev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    if (!srev) return -1;
    int sr_id = srev->id;
    acta_db_skill_revision_free(srev);

    /* --- model --- */
    model_t m = {0};
    m.name             = (char *)"ExecModel";
    m.backend          = (char *)"openai";
    m.model_identifier = (char *)"gpt-4";
    int model_id = 0;
    if (acta_db_model_create(db, &m, &model_id) != ACTA_DB_OK) return -1;
    model_revision_t *mrev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
    if (!mrev) return -1;
    int mr_id = mrev->id;
    acta_db_model_revision_free(mrev);

    *out_ctx = ctx_id;
    *out_sr  = sr_id;
    *out_mr  = mr_id;
    return ACTA_DB_OK;
}

/* Convenience: create an execution with default fields. Returns row id or -1. */
static int exec_create(db_t *db, int ctx_id, int sr_id, int mr_id,
                       const char *prompt, int parent_id) {
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.prompt              = (char *)prompt;
    e.status              = ACTA_EXEC_STATUS_PENDING;
    e.parent_execution_id = parent_id;

    int out_id = 0;
    if (acta_db_execution_create(db, &e, &out_id) != ACTA_DB_OK) return -1;
    return out_id;
}

/* Count elements in a null-terminated execution_t** array. */
static int exec_list_count(execution_t **items) {
    if (!items) return 0;
    int n = 0;
    while (items[n] != NULL) n++;
    return n;
}


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



/* ---------- 9.11: start — pending → running ---------- */
static void test_exec_start_pending_to_running(void) {
    const char *path = "test/acta_test_exec_start_pr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "StartTest", 0);
    TEST_ASSERT(eid > 0);

    int rc = acta_db_execution_start(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_RUNNING);
    TEST_ASSERT(got->started_at != NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.12: start — already running ---------- */
static void test_exec_start_already_running(void) {
    const char *path = "test/acta_test_exec_start_rr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "DoubleStart", 0);
    TEST_ASSERT(eid > 0);

    int rc1 = acta_db_execution_start(db, eid);
    TEST_ASSERT_EQ_INT(rc1, ACTA_DB_OK);

    int rc2 = acta_db_execution_start(db, eid);
    TEST_ASSERT_EQ_INT(rc2, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.13: start — completed execution ---------- */
static void test_exec_start_completed(void) {
    const char *path = "test/acta_test_exec_start_comp.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CompThenStart", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, eid, "done"), ACTA_DB_OK);

    int rc = acta_db_execution_start(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.14: start — failed execution ---------- */
static void test_exec_start_failed(void) {
    const char *path = "test/acta_test_exec_start_fail.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "FailThenStart", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(db, eid, "oops"), ACTA_DB_OK);

    int rc = acta_db_execution_start(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.15: start — non-existent id ---------- */
static void test_exec_start_nonexistent(void) {
    const char *path = "test/acta_test_exec_start_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_execution_start(db, 999999);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 9.16: complete — running → completed ---------- */
static void test_exec_complete_running_to_completed(void) {
    const char *path = "test/acta_test_exec_comp.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CompleteTest", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_complete(db, eid, "the result");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_COMPLETED);
    TEST_ASSERT_EQ_STR(got->result, "the result");
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.17: complete — pending (skip running) ---------- */
static void test_exec_complete_pending(void) {
    const char *path = "test/acta_test_exec_comp_pending.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "SkipRunning", 0);
    TEST_ASSERT(eid > 0);

    int rc = acta_db_execution_complete(db, eid, "should fail");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.18: complete — already completed ---------- */
static void test_exec_complete_twice(void) {
    const char *path = "test/acta_test_exec_comp_twice.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "Twice", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, eid, "first"), ACTA_DB_OK);

    int rc = acta_db_execution_complete(db, eid, "second");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.19: complete — NULL result ---------- */
static void test_exec_complete_null_result(void) {
    const char *path = "test/acta_test_exec_comp_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "NullResult", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_complete(db, eid, NULL);
    /* result is nullable — accept return 0 */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 9.20: fail — running → failed ---------- */
static void test_exec_fail_running_to_failed(void) {
    const char *path = "test/acta_test_exec_fail.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "FailTest", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_fail(db, eid, "something broke");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_FAILED);
    TEST_ASSERT_EQ_STR(got->error, "something broke");
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.21: fail — pending → failed ---------- */
static void test_exec_fail_pending(void) {
    const char *path = "test/acta_test_exec_fail_pending.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "FailPending", 0);
    TEST_ASSERT(eid > 0);

    int rc = acta_db_execution_fail(db, eid, "should fail");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.22: fail — NULL error ---------- */
static void test_exec_fail_null_error(void) {
    const char *path = "test/acta_test_exec_fail_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "NullErr", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_fail(db, eid, NULL);
    /* error is nullable — accept return 0 */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 9.23: set_raw_response — valid ---------- */
static void test_exec_set_raw_valid(void) {
    const char *path = "test/acta_test_exec_raw.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "RawTest", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_set_raw_response(db, eid, "{\"key\":\"val\"}");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->raw_response, "{\"key\":\"val\"}");
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.24: set_raw_response — NULL ---------- */
static void test_exec_set_raw_null(void) {
    const char *path = "test/acta_test_exec_raw_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "RawNull", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_set_raw_response(db, eid, NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 9.25: set_raw_response — non-existent id ---------- */
static void test_exec_set_raw_nonexistent(void) {
    const char *path = "test/acta_test_exec_raw_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_execution_set_raw_response(db, 999999, "data");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 9.26: list_by_status — "pending" ---------- */
static void test_exec_list_by_status_pending(void) {
    const char *path = "test/acta_test_exec_list_pend.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* 3 pending */
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "P1", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "P2", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "P3", 0) > 0);

    /* 2 running */
    int r1 = exec_create(db, ctx_id, sr_id, mr_id, "R1", 0);
    int r2 = exec_create(db, ctx_id, sr_id, mr_id, "R2", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, r1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, r2), ACTA_DB_OK);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_PENDING, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(exec_list_count(items), 3);
    acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}

/* ---------- 9.27: list_by_status — "completed" ---------- */
static void test_exec_list_by_status_completed(void) {
    const char *path = "test/acta_test_exec_list_comp.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* 2 completed */
    int c1 = exec_create(db, ctx_id, sr_id, mr_id, "C1", 0);
    int c2 = exec_create(db, ctx_id, sr_id, mr_id, "C2", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, c1, "ok"), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, c2, "ok"), ACTA_DB_OK);

    /* 1 pending (should not appear) */
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "P1", 0) > 0);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_COMPLETED, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(exec_list_count(items), 2);
    acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}

/* ---------- 9.28: list_by_status — no matches ---------- */
static void test_exec_list_by_status_no_match(void) {
    const char *path = "test/acta_test_exec_list_nomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* Create only pending executions */
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "X", 0) > 0);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_FAILED, &err);
    /* Either error or empty list */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(exec_list_count(items), 0);
    }
    if (items) acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}

/* ---------- 9.29: list_by_status — invalid status ---------- */
static void test_exec_list_by_status_invalid(void) {
    const char *path = "test/acta_test_exec_list_bogus.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, "bogus", &err);
    /* Either an error or 0 results */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(exec_list_count(items), 0);
    }
    if (items) acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}

/* ---------- 9.30: list_children — with children ---------- */
static void test_exec_list_children_with(void) {
    const char *path = "test/acta_test_exec_children.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent_id = exec_create(db, ctx_id, sr_id, mr_id, "Parent", 0);
    TEST_ASSERT(parent_id > 0);

    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "C1", parent_id) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "C2", parent_id) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "C3", parent_id) > 0);

    /* Also an unrelated root execution (should not appear) */
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "Unrelated", 0) > 0);

    int err = 0;
    execution_t **children = acta_db_execution_list_children(db, parent_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(children);
    TEST_ASSERT_EQ_INT(exec_list_count(children), 3);
    for (int i = 0; children[i] != NULL; i++) {
        TEST_ASSERT_EQ_INT(children[i]->parent_execution_id, parent_id);
    }
    acta_db_execution_list_free(children);

    test_db_teardown(db, path);
}

/* ---------- 9.31: list_children — no children ---------- */
static void test_exec_list_children_none(void) {
    const char *path = "test/acta_test_exec_children_none.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int leaf_id = exec_create(db, ctx_id, sr_id, mr_id, "Leaf", 0);
    TEST_ASSERT(leaf_id > 0);

    int err = 0;
    execution_t **children = acta_db_execution_list_children(db, leaf_id, &err);
    /* Either error or empty list */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(exec_list_count(children), 0);
    }
    if (children) acta_db_execution_list_free(children);

    test_db_teardown(db, path);
}

/* ---------- 9.32: list_children — non-existent parent ---------- */
static void test_exec_list_children_nonexistent(void) {
    const char *path = "test/acta_test_exec_children_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    execution_t **children = acta_db_execution_list_children(db, 999999, &err);
    /* Either an error or 0 results */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(exec_list_count(children), 0);
    }
    if (children) acta_db_execution_list_free(children);

    test_db_teardown(db, path);
}

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

    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_PENDING, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(exec_list_count(items), 3);
    acta_db_execution_list_free(items);

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

/* ---------- 9.37: list_by_context — with matches ---------- */
static void test_exec_list_by_context_with(void) {
    const char *path = "test/acta_test_exec_ctx_with.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "A1", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "A2", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "A3", 0) > 0);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_context(db, ctx_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(exec_list_count(items), 3);
    for (int i = 0; items[i] != NULL; i++) {
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx_id);
    }
    acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}

/* ---------- 9.38: list_by_context — no matches ---------- */
static void test_exec_list_by_context_no_match(void) {
    const char *path = "test/acta_test_exec_ctx_nomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "X", 0) > 0);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_context(db, 999999, &err);
    /* Either error or empty list */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(exec_list_count(items), 0);
    }
    if (items) acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}

/* ---------- 9.39: list_by_context — mixed contexts ---------- */
static void test_exec_list_by_context_mixed(void) {
    const char *path = "test/acta_test_exec_ctx_mixed.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* Second context */
    context_t ctx2 = {0};
    ctx2.type         = (char *)"test2";
    ctx2.content      = (char *)"world";
    ctx2.content_hash = (char *)"cafebabe";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &ctx2, &ctx2_id), ACTA_DB_OK);

    /* 3 in ctx1, 2 in ctx2 */
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "C1a", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "C1b", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "C1c", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx2_id, sr_id, mr_id, "C2a", 0) > 0);
    TEST_ASSERT(exec_create(db, ctx2_id, sr_id, mr_id, "C2b", 0) > 0);

    int err = 0;
    execution_t **items = acta_db_execution_list_by_context(db, ctx_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(exec_list_count(items), 3);
    for (int i = 0; items[i] != NULL; i++) {
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx_id);
    }
    acta_db_execution_list_free(items);

    /* Verify ctx2 */
    err = 0;
    items = acta_db_execution_list_by_context(db, ctx2_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(exec_list_count(items), 2);
    for (int i = 0; items[i] != NULL; i++) {
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx2_id);
    }
    acta_db_execution_list_free(items);

    test_db_teardown(db, path);
}


/* ---------- runner ---------- */
void run_execution_tests(void) {
    fprintf(stderr, "\n=== execution tests ===\n");
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
    test_exec_start_pending_to_running();
    test_exec_start_already_running();
    test_exec_start_completed();
    test_exec_start_failed();
    test_exec_start_nonexistent();
    test_exec_complete_running_to_completed();
    test_exec_complete_pending();
    test_exec_complete_twice();
    test_exec_complete_null_result();
    test_exec_fail_running_to_failed();
    test_exec_fail_pending();
    test_exec_fail_null_error();
    test_exec_set_raw_valid();
    test_exec_set_raw_null();
    test_exec_set_raw_nonexistent();
    test_exec_list_by_status_pending();
    test_exec_list_by_status_completed();
    test_exec_list_by_status_no_match();
    test_exec_list_by_status_invalid();
    test_exec_list_children_with();
    test_exec_list_children_none();
    test_exec_list_children_nonexistent();
    test_exec_free_valid();
    test_exec_free_null();
    test_exec_list_free_valid();
    test_exec_raw_insert();
    test_exec_list_by_context_with();
    test_exec_list_by_context_no_match();
    test_exec_list_by_context_mixed();
}
