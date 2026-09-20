#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>

/* ================================================================== */
/*  Create                                                           */
/* ================================================================== */

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
    e.status              = ACTA_EXEC_STATUS_PENDING;
    e.parent_execution_id = 0;

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, &out_id), ACTA_DB_OK);
    TEST_ASSERT(out_id > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, out_id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_create_null_db(void) {
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id        = 1;
    e.skill_revision_id = 1;
    e.model_revision_id = 1;

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(NULL, &e, &out_id),
                       ACTA_DB_ERR_INVALID);
}

static void test_exec_create_null_e(void) {
    const char *path = "test/acta_test_exec_null_e.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, NULL, &out_id),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

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

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, &out_id),
                       ACTA_DB_ERR_FK);

    test_db_teardown(db, path);
}

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

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, &out_id),
                       ACTA_DB_ERR_FK);

    test_db_teardown(db, path);
}

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

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, &out_id),
                       ACTA_DB_ERR_FK);

    test_db_teardown(db, path);
}

static void test_exec_create_default_status(void) {
    const char *path = "test/acta_test_exec_def_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
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

static void test_exec_create_with_parent(void) {
    const char *path = "test/acta_test_exec_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent_id = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT(parent_id > 0);

    int child_id = exec_create(db, ctx_id, sr_id, mr_id, parent_id);
    TEST_ASSERT(child_id > 0);

    int err = 0;
    execution_t *child = acta_db_execution_get(db, child_id, &err);
    TEST_ASSERT_NOT_NULL(child);
    TEST_ASSERT_EQ_INT(child->parent_execution_id, parent_id);
    acta_db_execution_free(child);

    test_db_teardown(db, path);
}

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
    e.parent_execution_id = 999999;

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, &out_id),
                       ACTA_DB_ERR_FK);
}

static void test_exec_create_zero_ids(void) {
    const char *path = "test/acta_test_exec_zero_ids.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* each id field == 0 must be rejected up front as INVALID */
    execution_t e;
    memset(&e, 0, sizeof(e));

    e.context_id = 0; e.skill_revision_id = sr_id; e.model_revision_id = mr_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, NULL), ACTA_DB_ERR_INVALID);

    e.context_id = ctx_id; e.skill_revision_id = 0; e.model_revision_id = mr_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, NULL), ACTA_DB_ERR_INVALID);

    e.context_id = ctx_id; e.skill_revision_id = sr_id; e.model_revision_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, NULL), ACTA_DB_ERR_INVALID);

    /* negative ids too */
    e.context_id = -1; e.skill_revision_id = sr_id; e.model_revision_id = mr_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, NULL), ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_exec_create_ignores_status(void) {
    const char *path = "test/acta_test_exec_ign_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id        = ctx_id;
    e.skill_revision_id = sr_id;
    e.model_revision_id = mr_id;
    e.status            = ACTA_EXEC_STATUS_COMPLETED;  /* must be ignored */

    int out_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &e, &out_id), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, out_id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ================================================================== */
/*  Get                                                              */
/* ================================================================== */

static void test_exec_get_existing(void) {
    const char *path = "test/acta_test_exec_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT(eid > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, eid);
    TEST_ASSERT_EQ_INT(got->context_id, ctx_id);
    TEST_ASSERT_EQ_INT(got->skill_revision_id, sr_id);
    TEST_ASSERT_EQ_INT(got->model_revision_id, mr_id);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_get_nonexistent(void) {
    const char *path = "test/acta_test_exec_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, 999999, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* NULL-err guard */
    execution_t *got2 = acta_db_execution_get(db, 999999, NULL);
    TEST_ASSERT_NULL(got2);

    test_db_teardown(db, path);
}

static void test_exec_get_null_db(void) {
    int err = 0;
    execution_t *got = acta_db_execution_get(NULL, 1, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_exec_get_bad_id(void) {
    const char *path = "test/acta_test_exec_get_badid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, 0, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ================================================================== */
/*  State transitions                                                */
/* ================================================================== */

/* ── start ── */

static void test_exec_start_happy(void) {
    const char *path = "test/acta_test_exec_start.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_RUNNING);
    TEST_ASSERT(got->started_at != NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_start_already_running(void) {
    const char *path = "test/acta_test_exec_start_inv.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_exec_start_from_terminal(void) {
    const char *path = "test/acta_test_exec_start_term.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* completed */
    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e1);
    acta_db_execution_complete(db, e1, "ok");
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1),
                       ACTA_DB_ERR_INVALID);

    /* cancelled */
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_cancel(db, e2);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e2),
                       ACTA_DB_ERR_INVALID);

    /* failed */
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e3);
    acta_db_execution_fail(db, e3, "x");
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e3),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ── complete ── */

static void test_exec_complete_happy(void) {
    const char *path = "test/acta_test_exec_complete.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, eid);

    TEST_ASSERT_EQ_INT(
        acta_db_execution_complete(db, eid, "the answer"), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_COMPLETED);
    TEST_ASSERT_EQ_STR(got->result, "the answer");
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* complete requires running; pending must be rejected. */
static void test_exec_complete_from_pending(void) {
    const char *path = "test/acta_test_exec_complete_pending.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT_EQ_INT(
        acta_db_execution_complete(db, eid, "x"),
        ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_exec_complete_from_terminal(void) {
    const char *path = "test/acta_test_exec_complete_term.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* from cancelled */
    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_cancel(db, e1);
    TEST_ASSERT_EQ_INT(
        acta_db_execution_complete(db, e1, "x"),
        ACTA_DB_ERR_INVALID);

    /* double-complete */
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e2);
    acta_db_execution_complete(db, e2, "first");
    TEST_ASSERT_EQ_INT(
        acta_db_execution_complete(db, e2, "second"),
        ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ── fail ── */

static void test_exec_fail_happy(void) {
    const char *path = "test/acta_test_exec_fail.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, eid);

    TEST_ASSERT_EQ_INT(
        acta_db_execution_fail(db, eid, "something broke"), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_FAILED);
    TEST_ASSERT_EQ_STR(got->error, "something broke");
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* fail requires running; pending must be rejected. */
static void test_exec_fail_from_pending(void) {
    const char *path = "test/acta_test_exec_fail_pending.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT_EQ_INT(
        acta_db_execution_fail(db, eid, "x"),
        ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_exec_fail_from_terminal(void) {
    const char *path = "test/acta_test_exec_fail_term.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* from completed */
    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e1);
    acta_db_execution_complete(db, e1, "ok");
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(db, e1, "x"),
                       ACTA_DB_ERR_INVALID);

    /* from cancelled */
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_cancel(db, e2);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(db, e2, "x"),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ── cancel ── */

static void test_exec_cancel_pending(void) {
    const char *path = "test/acta_test_exec_cancel_p.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_cancel_running(void) {
    const char *path = "test/acta_test_exec_cancel_r.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, eid);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_cancel_from_terminal(void) {
    const char *path = "test/acta_test_exec_cancel_t.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* completed */
    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e1);
    acta_db_execution_complete(db, e1, "ok");
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, e1),
                       ACTA_DB_ERR_INVALID);

    /* cancelled (double-cancel) */
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_cancel(db, e2);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, e2),
                       ACTA_DB_ERR_INVALID);

    /* failed */
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e3);
    acta_db_execution_fail(db, e3, "x");
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, e3),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ── set_raw_response ── */

static void test_exec_set_raw_response_pending(void) {
    const char *path = "test/acta_test_exec_raw_p.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    const char *raw = "{\"tokens\": 42}";
    TEST_ASSERT_EQ_INT(
        acta_db_execution_set_raw_response(db, eid, raw), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->raw_response, raw);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* set_raw_response has no status restriction; terminal states allowed. */
static void test_exec_set_raw_response_terminal(void) {
    const char *path = "test/acta_test_exec_raw_t.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, eid);
    acta_db_execution_complete(db, eid, "done");

    const char *raw = "{\"final\": true}";
    TEST_ASSERT_EQ_INT(
        acta_db_execution_set_raw_response(db, eid, raw), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->raw_response, raw);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_COMPLETED);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_set_raw_response_nonexistent(void) {
    const char *path = "test/acta_test_exec_raw_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_execution_set_raw_response(db, 999999, "x");
    TEST_ASSERT(rc != ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ================================================================== */
/*  Unified query                                                    */
/* ================================================================== */

static void test_exec_query_any(void) {
    const char *path = "test/acta_test_exec_q_any.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 4; i++)
        TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                                  0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 4);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

static void test_exec_query_by_status(void) {
    const char *path = "test/acta_test_exec_q_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    acta_db_execution_start(db, e2);
    acta_db_execution_complete(db, e2, "done");
    (void)e1;

    int err = 0;

    execution_query_t q1 = ACTA_EXEC_QUERY_ANY;
    q1.status = ACTA_EXEC_STATUS_PENDING;
    int n1 = 0;
    execution_t **r1 = acta_db_execution_query(db, &q1, 0, 0, &n1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n1, 1);
    acta_db_execution_list_free(r1, n1);

    execution_query_t q2 = ACTA_EXEC_QUERY_ANY;
    q2.status = ACTA_EXEC_STATUS_COMPLETED;
    int n2 = 0;
    execution_t **r2 = acta_db_execution_query(db, &q2, 0, 0, &n2, &err);
    TEST_ASSERT_EQ_INT(n2, 1);
    acta_db_execution_list_free(r2, n2);

    execution_query_t q3 = ACTA_EXEC_QUERY_ANY;
    q3.status = ACTA_EXEC_STATUS_FAILED;
    int n3 = 0;
    acta_db_execution_query(db, &q3, 0, 0, &n3, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n3, 0);

    test_db_teardown(db, path);
}

static void test_exec_query_by_parent(void) {
    const char *path = "test/acta_test_exec_q_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent = exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, parent);
    exec_create(db, ctx_id, sr_id, mr_id, parent);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->parent_execution_id, parent);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

static void test_exec_query_by_context(void) {
    const char *path = "test/acta_test_exec_q_ctx.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    int err = 0;

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    int n = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 3);
    acta_db_execution_list_free(items, n);

    q.context_id = 999999;
    int n2 = 0;
    acta_db_execution_query(db, &q, 0, 0, &n2, &err);
    TEST_ASSERT_EQ_INT(n2, 0);

    test_db_teardown(db, path);
}

static void test_exec_query_by_skill_rev(void) {
    const char *path = "test/acta_test_exec_q_sr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    int err = 0;

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.skill_revision_id = sr_id;
    int n = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    q.skill_revision_id = 999999;
    int n2 = 0;
    acta_db_execution_query(db, &q, 0, 0, &n2, &err);
    TEST_ASSERT_EQ_INT(n2, 0);

    test_db_teardown(db, path);
}

static void test_exec_query_by_model_rev(void) {
    const char *path = "test/acta_test_exec_q_mr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, 0);

    int err = 0;

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.model_revision_id = mr_id;
    int n = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    acta_db_execution_list_free(items, n);

    q.model_revision_id = 999999;
    int n2 = 0;
    acta_db_execution_query(db, &q, 0, 0, &n2, &err);
    TEST_ASSERT_EQ_INT(n2, 0);

    test_db_teardown(db, path);
}

static void test_exec_query_combined(void) {
    const char *path = "test/acta_test_exec_q_combined.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    acta_db_execution_start(db, e1);
    acta_db_execution_complete(db, e1, "ok");

    int err = 0;

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status     = ACTA_EXEC_STATUS_COMPLETED;
    q.context_id = ctx_id;
    int n = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, e1);
    acta_db_execution_list_free(items, n);

    execution_query_t q2 = ACTA_EXEC_QUERY_ANY;
    q2.status     = ACTA_EXEC_STATUS_PENDING;
    q2.context_id = ctx_id;
    int n2 = 0;
    execution_t **r2 = acta_db_execution_query(db, &q2, 0, 0, &n2, &err);
    TEST_ASSERT_EQ_INT(n2, 1);
    acta_db_execution_list_free(r2, n2);

    execution_query_t q3 = ACTA_EXEC_QUERY_ANY;
    q3.status     = ACTA_EXEC_STATUS_COMPLETED;
    q3.context_id = 999999;
    int n3 = 0;
    acta_db_execution_query(db, &q3, 0, 0, &n3, &err);
    TEST_ASSERT_EQ_INT(n3, 0);

    test_db_teardown(db, path);
}

static void test_exec_query_pagination(void) {
    const char *path = "test/acta_test_exec_q_paging.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 7; i++)
        TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    int err = 0;

    int n1 = 0;
    execution_t **p1 = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                               0, 3, &n1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n1, 3);
    acta_db_execution_list_free(p1, n1);

    int n2 = 0;
    execution_t **p2 = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                               3, 3, &n2, &err);
    TEST_ASSERT_EQ_INT(n2, 3);
    acta_db_execution_list_free(p2, n2);

    int n3 = 0;
    execution_t **p3 = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                               6, 3, &n3, &err);
    TEST_ASSERT_EQ_INT(n3, 1);
    acta_db_execution_list_free(p3, n3);

    int n4 = 0;
    acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 7, 3, &n4, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n4, 0);

    test_db_teardown(db, path);
}

static void test_exec_query_null_q(void) {
    const char *path = "test/acta_test_exec_q_nullq.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

static void test_exec_query_null_db(void) {
    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(NULL, &ACTA_EXEC_QUERY_ANY,
                                                  0, 10, &n, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_exec_query_negative_offset(void) {
    const char *path = "test/acta_test_exec_q_negoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                                  -1, 10, &n, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_exec_query_null_outparams(void) {
    const char *path = "test/acta_test_exec_q_nullout.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                                  0, 0, NULL, NULL);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_execution_list_free(items, 1);

    test_db_teardown(db, path);
}

/* ================================================================== */
/*  Count                                                            */
/* ================================================================== */

static void test_exec_count_all(void) {
    const char *path = "test/acta_test_exec_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 5; i++)
        TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &ACTA_EXEC_QUERY_ANY,
                                               &err), 5);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_exec_count_by_status(void) {
    const char *path = "test/acta_test_exec_count_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    acta_db_execution_start(db, e1);
    acta_db_execution_complete(db, e1, "ok");
    acta_db_execution_start(db, e2);

    int err = 0;

    execution_query_t qp = ACTA_EXEC_QUERY_ANY;
    qp.status = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &qp, &err), 1);

    execution_query_t qr = ACTA_EXEC_QUERY_ANY;
    qr.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &qr, &err), 1);

    execution_query_t qc = ACTA_EXEC_QUERY_ANY;
    qc.status = ACTA_EXEC_STATUS_COMPLETED;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &qc, &err), 1);

    test_db_teardown(db, path);
}

static void test_exec_count_by_parent(void) {
    const char *path = "test/acta_test_exec_count_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent = exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, parent);
    exec_create(db, ctx_id, sr_id, mr_id, parent);
    exec_create(db, ctx_id, sr_id, mr_id, parent);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 3);

    test_db_teardown(db, path);
}

static void test_exec_count_combined(void) {
    const char *path = "test/acta_test_exec_count_comb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    exec_create(db, ctx_id, sr_id, mr_id, 0);

    acta_db_execution_start(db, e1);
    acta_db_execution_complete(db, e1, "done");

    int err = 0;

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status     = ACTA_EXEC_STATUS_COMPLETED;
    q.context_id = ctx_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    execution_query_t q2 = ACTA_EXEC_QUERY_ANY;
    q2.status     = ACTA_EXEC_STATUS_PENDING;
    q2.context_id = ctx_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q2, &err), 1);

    execution_query_t q3 = ACTA_EXEC_QUERY_ANY;
    q3.status     = ACTA_EXEC_STATUS_COMPLETED;
    q3.context_id = 999999;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q3, &err), 0);

    test_db_teardown(db, path);
}

static void test_exec_count_empty(void) {
    const char *path = "test/acta_test_exec_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &ACTA_EXEC_QUERY_ANY,
                                               &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_exec_count_null_db(void) {
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(NULL, &ACTA_EXEC_QUERY_ANY,
                                               &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_exec_count_null_q(void) {
    const char *path = "test/acta_test_exec_count_nullq.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, NULL, &err), 2);

    test_db_teardown(db, path);
}

static void test_exec_count_null_err(void) {
    const char *path = "test/acta_test_exec_count_noerr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    TEST_ASSERT_EQ_INT(
        acta_db_execution_count(db, &ACTA_EXEC_QUERY_ANY, NULL), 1);

    test_db_teardown(db, path);
}

/* ================================================================== */
/*  Free                                                             */
/* ================================================================== */

static void test_exec_free_valid(void) {
    const char *path = "test/acta_test_exec_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT(eid > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

static void test_exec_free_null(void) {
    acta_db_execution_free(NULL);
}

static void test_exec_list_free_valid(void) {
    const char *path = "test/acta_test_exec_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 3; i++)
        TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, 0) > 0);

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY,
                                                  0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

static void test_exec_list_free_null(void) {
    acta_db_execution_list_free(NULL, 0);
}

/* FK bypass via raw SQL — documents that the C API is the only
 * validation layer; the schema does not enforce FK at the SQLite level. */
static void test_exec_raw_insert_fk_bypass(void) {
    const char *path = "test/acta_test_exec_raw_insert.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(exec_insert_raw(db, 42, 9999, 9999, 9999, 0),
                       ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, 42, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, 42);
    TEST_ASSERT_EQ_INT(got->context_id, 9999);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ================================================================== */
/*  Runners                                                          */
/* ================================================================== */

int run_execution_create_tests(void) {
    fprintf(stderr, "\n--- execution: create ---\n");
    test_exec_create_happy();
    test_exec_create_null_db();
    test_exec_create_null_e();    
    test_exec_create_invalid_ctx();
    test_exec_create_invalid_sr();
    test_exec_create_invalid_mr();
    test_exec_create_zero_ids();
    test_exec_create_default_status();
    test_exec_create_with_parent();
    test_exec_create_invalid_parent();
    test_exec_create_ignores_status();

    return test_failures;
}

void run_execution_get_tests(void) {
    fprintf(stderr, "\n--- execution: get ---\n");
    test_exec_get_existing();
    test_exec_get_nonexistent();
    test_exec_get_null_db();
    test_exec_get_bad_id();
}

void run_execution_state_tests(void) {
    fprintf(stderr, "\n--- execution: state transitions ---\n");
    /* start */
    test_exec_start_happy();
    test_exec_start_already_running();
    test_exec_start_from_terminal();
    /* complete */
    test_exec_complete_happy();
    test_exec_complete_from_pending();
    test_exec_complete_from_terminal();
    /* fail */
    test_exec_fail_happy();
    test_exec_fail_from_pending();
    test_exec_fail_from_terminal();
    /* cancel */
    test_exec_cancel_pending();
    test_exec_cancel_running();
    test_exec_cancel_from_terminal();
    /* set_raw_response */
    test_exec_set_raw_response_pending();
    test_exec_set_raw_response_terminal();
    test_exec_set_raw_response_nonexistent();
}

void run_execution_query_tests(void) {
    fprintf(stderr, "\n--- execution: query ---\n");
    test_exec_query_any();
    test_exec_query_by_status();
    test_exec_query_by_parent();
    test_exec_query_by_context();
    test_exec_query_by_skill_rev();
    test_exec_query_by_model_rev();
    test_exec_query_combined();
    test_exec_query_pagination();
    test_exec_query_null_q();
    test_exec_query_null_db();
    test_exec_query_negative_offset();
    test_exec_query_null_outparams();
}

void run_execution_count_tests(void) {
    fprintf(stderr, "\n--- execution: count ---\n");
    test_exec_count_all();
    test_exec_count_by_status();
    test_exec_count_by_parent();
    test_exec_count_combined();
    test_exec_count_empty();
    test_exec_count_null_db();
    test_exec_count_null_q();
    test_exec_count_null_err();
}

void run_execution_free_tests(void) {
    fprintf(stderr, "\n--- execution: free ---\n");
    test_exec_free_valid();
    test_exec_free_null();
    test_exec_list_free_valid();
    test_exec_list_free_null();
    test_exec_raw_insert_fk_bypass();
}

/* Runs every execution test group. */
void run_execution_all_tests(void) {
    fprintf(stderr, "\n========== execution ==========\n");
    run_execution_create_tests();
    run_execution_get_tests();
    run_execution_state_tests();
    run_execution_query_tests();
    run_execution_count_tests();
    run_execution_free_tests();
}
