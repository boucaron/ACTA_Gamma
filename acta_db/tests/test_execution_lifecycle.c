#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>

/* ================================================================== */
/*  Shared fixture                                                    */
/* ================================================================== */

typedef struct {
    db_t      *db;
    const char *path;
    int        ctx_id, sr_id, mr_id;
} env_t;

static env_t env_open(const char *path) {
    env_t e = { .path = path };

    char side[512];
    snprintf(side, sizeof(side), "%s-wal", path);
    remove(side);
    snprintf(side, sizeof(side), "%s-shm", path);
    remove(side);
    remove(path);

    e.db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(e.db);
    TEST_ASSERT_EQ_INT(exec_setup(e.db, &e.ctx_id, &e.sr_id, &e.mr_id),
                       ACTA_DB_OK);
    return e;
}


static void env_close(env_t *e) {
    test_db_teardown(e->db, e->path);
}

/* Shorthand: open env + create one execution, return its id. */
static int env_exec(env_t *e, const char *prompt, int parent_id) {
    int eid = exec_create(e->db, e->ctx_id, e->sr_id, e->mr_id,
                          prompt, parent_id);
    TEST_ASSERT(eid > 0);
    return eid;
}

/* Override context_id (uses default sr_id, mr_id). */
static int env_exec_with_ctx(env_t *e, int ctx_id,
                             const char *prompt, int parent_id) {
    int eid = exec_create(e->db, ctx_id, e->sr_id, e->mr_id,
                          prompt, parent_id);
    TEST_ASSERT(eid > 0);
    return eid;
}

/* Override parent_execution_id (uses default ctx_id, sr_id, mr_id). */
static int env_exec_with_parent(env_t *e, int parent_id,
                                const char *prompt) {
    int eid = exec_create(e->db, e->ctx_id, e->sr_id, e->mr_id,
                          prompt, parent_id);
    TEST_ASSERT(eid > 0);
    return eid;
}


/* ================================================================== */
/*  start                                                             */
/* ================================================================== */

static void test_exec_start_pending_to_running(void) {
    env_t e = env_open("test/acta_test_exec_start_pr.db");
    int eid = env_exec(&e, "StartTest", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_RUNNING);
    TEST_ASSERT(got->started_at != NULL);
    acta_db_execution_free(got);

    env_close(&e);
}

static void test_exec_start_already_running(void) {
    env_t e = env_open("test/acta_test_exec_start_rr.db");
    int eid = env_exec(&e, "DoubleStart", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_start_completed(void) {
    env_t e = env_open("test/acta_test_exec_start_comp.db");
    int eid = env_exec(&e, "CompThenStart", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, "done"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_start_failed(void) {
    env_t e = env_open("test/acta_test_exec_start_fail.db");
    int eid = env_exec(&e, "FailThenStart", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(e.db, eid, "oops"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_start_nonexistent(void) {
    env_t e = env_open("test/acta_test_exec_start_404.db");

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, 999999),
                       ACTA_DB_ERR_NOT_FOUND);
    env_close(&e);
}

static void test_exec_start_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_start(NULL, 1),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  complete                                                          */
/* ================================================================== */

static void test_exec_complete_running_to_completed(void) {
    env_t e = env_open("test/acta_test_exec_comp.db");
    int eid = env_exec(&e, "CompleteTest", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, "the result"),
                       ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_COMPLETED);
    TEST_ASSERT_EQ_STR(got->result, "the result");
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);
    env_close(&e);
}

static void test_exec_complete_pending(void) {
    env_t e = env_open("test/acta_test_exec_comp_pending.db");
    int eid = env_exec(&e, "SkipRunning", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, "nope"),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_complete_twice(void) {
    env_t e = env_open("test/acta_test_exec_comp_twice.db");
    int eid = env_exec(&e, "Twice", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, "first"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, "second"),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_complete_null_result(void) {
    env_t e = env_open("test/acta_test_exec_comp_null.db");
    int eid = env_exec(&e, "NullResult", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, NULL),
                       ACTA_DB_OK);
    env_close(&e);
}

static void test_exec_complete_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(NULL, 1, "r"),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  fail                                                              */
/* ================================================================== */

static void test_exec_fail_running_to_failed(void) {
    env_t e = env_open("test/acta_test_exec_fail.db");
    int eid = env_exec(&e, "FailTest", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(e.db, eid, "something broke"),
                       ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_FAILED);
    TEST_ASSERT_EQ_STR(got->error, "something broke");
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);
    env_close(&e);
}

static void test_exec_fail_pending(void) {
    env_t e = env_open("test/acta_test_exec_fail_pending.db");
    int eid = env_exec(&e, "FailPending", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_fail(e.db, eid, "nope"),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_fail_null_error(void) {
    env_t e = env_open("test/acta_test_exec_fail_null.db");
    int eid = env_exec(&e, "NullErr", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(e.db, eid, NULL),
                       ACTA_DB_OK);
    env_close(&e);
}

static void test_exec_fail_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(NULL, 1, "err"),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  cancel                                                            */
/* ================================================================== */

static void test_exec_cancel_pending_to_cancelled(void) {
    env_t e = env_open("test/acta_test_exec_cancel_pend.db");
    int eid = env_exec(&e, "CancelPend", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_free(got);
    env_close(&e);
}

static void test_exec_cancel_running_to_cancelled(void) {
    env_t e = env_open("test/acta_test_exec_cancel_run.db");
    int eid = env_exec(&e, "CancelRun", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid),
                       ACTA_DB_ERR_INVALID);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_RUNNING);
    acta_db_execution_free(got);
    env_close(&e);
}


static void test_exec_cancel_already_cancelled(void) {
    env_t e = env_open("test/acta_test_exec_cancel_twice.db");
    int eid = env_exec(&e, "CancelTwice", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_cancel_completed(void) {
    env_t e = env_open("test/acta_test_exec_cancel_comp.db");
    int eid = env_exec(&e, "CancelComp", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, eid, "done"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_cancel_failed(void) {
    env_t e = env_open("test/acta_test_exec_cancel_fail.db");
    int eid = env_exec(&e, "CancelFail", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(e.db, eid, "oops"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid),
                       ACTA_DB_ERR_INVALID);
    env_close(&e);
}

static void test_exec_cancel_nonexistent(void) {
    env_t e = env_open("test/acta_test_exec_cancel_404.db");

    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, 999999),
                       ACTA_DB_ERR_NOT_FOUND);
    env_close(&e);
}

static void test_exec_cancel_completed_at(void) {
    env_t e = env_open("test/acta_test_exec_cancel_at.db");
    int eid = env_exec(&e, "CancelAt", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(e.db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);
    env_close(&e);
}

static void test_exec_cancel_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(NULL, 1),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  set_raw_response                                                  */
/* ================================================================== */

static void test_exec_set_raw_valid(void) {
    env_t e = env_open("test/acta_test_exec_raw.db");
    int eid = env_exec(&e, "RawTest", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(
        acta_db_execution_set_raw_response(e.db, eid,
                                           "{\"key\":\"val\"}"),
        ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->raw_response, "{\"key\":\"val\"}");
    acta_db_execution_free(got);
    env_close(&e);
}

static void test_exec_set_raw_null(void) {
    env_t e = env_open("test/acta_test_exec_raw_null.db");
    int eid = env_exec(&e, "RawNull", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_set_raw_response(e.db, eid, NULL),
                       ACTA_DB_OK);
    env_close(&e);
}

static void test_exec_set_raw_nonexistent(void) {
    env_t e = env_open("test/acta_test_exec_raw_404.db");

    TEST_ASSERT_EQ_INT(
        acta_db_execution_set_raw_response(e.db, 999999, "data"),
        ACTA_DB_ERR_NOT_FOUND);
    env_close(&e);
}

static void test_exec_set_raw_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_set_raw_response(NULL, 1, "x"),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  query                                                             */
/* ================================================================== */

static void test_exec_query_any_returns_all(void) {
    env_t e = env_open("test/acta_test_exec_q_any.db");

    for (int i = 0; i < 3; i++)
        env_exec(&e, "Q", 0);

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &acta_db_execution_query_any(), 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 3);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_limit_caps(void) {
    env_t e = env_open("test/acta_test_exec_q_limit.db");

    for (int i = 0; i < 10; i++)
        env_exec(&e, "Q", 0);

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &acta_db_execution_query_any(), 0, 4, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 4);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_offset_skips(void) {
    env_t e = env_open("test/acta_test_exec_q_offset.db");

    for (int i = 0; i < 5; i++)
        env_exec(&e, "Q", 0);

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &acta_db_execution_query_any(), 2, 2, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_offset_exhausted(void) {
    env_t e = env_open("test/acta_test_exec_q_exhausted.db");

    env_exec(&e, "Q", 0);
    env_exec(&e, "Q", 0);

    int n = -1, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &acta_db_execution_query_any(), 50, 10,
                                &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);
    TEST_ASSERT(items == NULL);
    env_close(&e);
}

static void test_exec_query_negative_offset(void) {
    env_t e = env_open("test/acta_test_exec_q_negoff.db");

    env_exec(&e, "Q", 0);

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &acta_db_execution_query_any(), -1, 10,
                                &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT(items == NULL);
    env_close(&e);
}

/* ---- filters ---- */

static void test_exec_query_by_status(void) {
    env_t e = env_open("test/acta_test_exec_q_status.db");

    int e1 = env_exec(&e, "Q", 0);
    env_exec(&e, "Q", 0);
    env_exec(&e, "Q", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e1), ACTA_DB_OK);

    /* pending */
    execution_query_t q = acta_db_execution_query_any();
    q.status = ACTA_EXEC_STATUS_PENDING;

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    if (!items) { env_close(&e); return; }
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_list_free(items, n);

    /* running */
    q.status = ACTA_EXEC_STATUS_RUNNING;
    items = acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    if (!items) { env_close(&e); return; }
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_STR(items[0]->status, ACTA_EXEC_STATUS_RUNNING);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}


static void test_exec_query_by_context(void) {
    env_t e = env_open("test/acta_test_exec_q_ctx.db");

    context_t c2;
    memset(&c2, 0, sizeof(c2));
    c2.type         = "test";
    c2.content      = "x";
    c2.content_hash = "hash_q_ctx_2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(e.db, &c2, &ctx2_id),
                       ACTA_DB_OK);

    env_exec_with_ctx(&e, e.ctx_id,  "Q", 0);
    env_exec_with_ctx(&e, e.ctx_id,  "Q", 0);
    env_exec_with_ctx(&e, ctx2_id,   "Q", 0);

    execution_query_t q = acta_db_execution_query_any();
    q.context_id = e.ctx_id;

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_by_parent(void) {
    env_t e = env_open("test/acta_test_exec_q_parent.db");

    int parent = env_exec(&e, "Parent", 0);
    env_exec_with_parent(&e, parent, "Q");
    env_exec_with_parent(&e, parent, "Q");
    env_exec(&e, "Q", 0);  /* root */

    execution_query_t q = acta_db_execution_query_any();
    q.parent_execution_id = parent;

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_by_skill_revision(void) {
    env_t e = env_open("test/acta_test_exec_q_sr.db");

    exec_create(e.db, e.ctx_id, e.sr_id,    e.mr_id, "Q", 0);
    exec_create(e.db, e.ctx_id, e.sr_id,    e.mr_id, "Q", 0);
    exec_create(e.db, e.ctx_id, e.sr_id + 999, e.mr_id, "Q", 0);

    execution_query_t q = acta_db_execution_query_any();
    q.skill_revision_id = e.sr_id;

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_by_model_revision(void) {
    env_t e = env_open("test/acta_test_exec_q_mr.db");

    exec_create(e.db, e.ctx_id, e.sr_id, e.mr_id,    "Q", 0);
    exec_create(e.db, e.ctx_id, e.sr_id, e.mr_id + 999, "Q", 0);

    execution_query_t q = acta_db_execution_query_any();
    q.model_revision_id = e.mr_id;

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_combined(void) {
    env_t e = env_open("test/acta_test_exec_q_combo.db");

    context_t c2;
    memset(&c2, 0, sizeof(c2));
    c2.type         = "test";
    c2.content      = "y";
    c2.content_hash = "hash_q_combo_2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(e.db, &c2, &ctx2_id),
                       ACTA_DB_OK);

    int e1 = exec_create(e.db, e.ctx_id,  e.sr_id, e.mr_id, "Q", 0);
    exec_create(e.db, e.ctx_id,  e.sr_id, e.mr_id, "Q", 0);
    int e3 = exec_create(e.db, ctx2_id,   e.sr_id, e.mr_id, "Q", 0);
    exec_create(e.db, ctx2_id,   e.sr_id, e.mr_id, "Q", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e3), ACTA_DB_OK);

    execution_query_t q = acta_db_execution_query_any();
    q.context_id = e.ctx_id;
    q.status     = ACTA_EXEC_STATUS_RUNNING;

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, e1);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_null_q(void) {
    env_t e = env_open("test/acta_test_exec_q_null.db");

    env_exec(&e, "Q", 0);
    env_exec(&e, "Q", 0);

    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(e.db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);
    env_close(&e);
}

static void test_exec_query_null_db(void) {
    int n = 0, err = 0;
    execution_t **items =
        acta_db_execution_query(NULL, &acta_db_execution_query_any(), 0, 0,
                                &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT(items == NULL);
}

/* ================================================================== */
/*  count                                                             */
/* ================================================================== */

static void test_exec_count_all(void) {
    env_t e = env_open("test/acta_test_exec_cnt_all.db");

    for (int i = 0; i < 7; i++)
        env_exec(&e, "Q", 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(
        acta_db_execution_count(e.db, &acta_db_execution_query_any(), &err), 7);
    env_close(&e);
}

static void test_exec_count_by_status(void) {
    env_t e = env_open("test/acta_test_exec_cnt_status.db");

    int e1 = env_exec(&e, "Q", 0);
    int e2 = env_exec(&e, "Q", 0);
    env_exec(&e, "Q", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e2), ACTA_DB_OK);

    execution_query_t q = acta_db_execution_query_any();
    q.status = ACTA_EXEC_STATUS_PENDING;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 1);

    q.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 2);
    env_close(&e);
}

static void test_exec_count_by_context(void) {
    env_t e = env_open("test/acta_test_exec_cnt_ctx.db");

    context_t c2;
    memset(&c2, 0, sizeof(c2));
    c2.type         = "test";
    c2.content      = "z";
    c2.content_hash = "hash_cnt_ctx_2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(e.db, &c2, &ctx2_id),
                       ACTA_DB_OK);

    exec_create(e.db, e.ctx_id, e.sr_id, e.mr_id, "Q", 0);
    exec_create(e.db, e.ctx_id, e.sr_id, e.mr_id, "Q", 0);
    exec_create(e.db, e.ctx_id, e.sr_id, e.mr_id, "Q", 0);
    exec_create(e.db, ctx2_id,  e.sr_id, e.mr_id, "Q", 0);

    execution_query_t q = acta_db_execution_query_any();
    q.context_id = e.ctx_id;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 3);

    q.context_id = ctx2_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 1);
    env_close(&e);
}

static void test_exec_count_combined(void) {
    env_t e = env_open("test/acta_test_exec_cnt_combo.db");

    int e1 = env_exec(&e, "Q", 0);
    int e2 = env_exec(&e, "Q", 0);
    int e3 = env_exec(&e, "Q", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, e3), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(e.db, e1, "ok"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(e.db, e2, "err"),
                       ACTA_DB_OK);
    /* e3 still running */

    execution_query_t q = acta_db_execution_query_any();
    q.context_id = e.ctx_id;
    q.status     = ACTA_EXEC_STATUS_RUNNING;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 1);
    env_close(&e);
}

static void test_exec_count_zero(void) {
    env_t e = env_open("test/acta_test_exec_cnt_zero.db");

    env_exec(&e, "Q", 0);

    execution_query_t q = acta_db_execution_query_any();
    q.status = ACTA_EXEC_STATUS_COMPLETED;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    env_close(&e);
}

static void test_exec_count_null_db(void) {
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(NULL, &acta_db_execution_query_any(),
                                                &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_exec_count_null_q(void) {
    env_t e = env_open("test/acta_test_exec_cnt_nullq.db");

    for (int i = 0; i < 3; i++)
        env_exec(&e, "Q", 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, NULL, &err), 3);
    env_close(&e);
}

/* ================================================================== */
/*  pagination                                                        */
/* ================================================================== */

static void test_exec_query_pagination_loop(void) {
    env_t e = env_open("test/acta_test_exec_q_page_loop.db");

    int total_to_create = 25;
    for (int i = 0; i < total_to_create; i++)
        env_exec(&e, "Q", 0);

    int err = 0;
    int total = acta_db_execution_count(e.db, &acta_db_execution_query_any(), &err);
    TEST_ASSERT_EQ_INT(total, total_to_create);

    int page_size = 7;
    int fetched   = 0;
    for (int offset = 0; offset < total; offset += page_size) {
        int n = 0;
        execution_t **items =
            acta_db_execution_query(e.db, &acta_db_execution_query_any(),
                                    offset, page_size, &n, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        TEST_ASSERT(n > 0 && n <= page_size);
        fetched += n;
        acta_db_execution_list_free(items, n);
    }
    TEST_ASSERT_EQ_INT(fetched, total_to_create);
    env_close(&e);
}

/* ================================================================== */
/*  Runner                                                            */
/* ================================================================== */

void run_execution_lifecycle_tests(void) {
    fprintf(stderr, "\n=== execution lifecycle ===\n");

    /* start */
    test_exec_start_pending_to_running();
    test_exec_start_already_running();
    test_exec_start_completed();
    test_exec_start_failed();
    test_exec_start_nonexistent();
    test_exec_start_null_db();

    /* complete */
    test_exec_complete_running_to_completed();
    test_exec_complete_pending();
    test_exec_complete_twice();
    test_exec_complete_null_result();
    test_exec_complete_null_db();

    /* fail */
    test_exec_fail_running_to_failed();
    test_exec_fail_pending();
    test_exec_fail_null_error();
    test_exec_fail_null_db();

    /* cancel */
    test_exec_cancel_pending_to_cancelled();
    test_exec_cancel_running_to_cancelled();
    test_exec_cancel_already_cancelled();
    test_exec_cancel_completed();
    test_exec_cancel_failed();
    test_exec_cancel_nonexistent();
    test_exec_cancel_completed_at();
    test_exec_cancel_null_db();

    /* set_raw_response */
    test_exec_set_raw_valid();
    test_exec_set_raw_null();
    test_exec_set_raw_nonexistent();
    test_exec_set_raw_null_db();

    /* query */
    test_exec_query_any_returns_all();
    test_exec_query_limit_caps();
    test_exec_query_offset_skips();
    test_exec_query_offset_exhausted();
    test_exec_query_negative_offset();
    test_exec_query_by_status();
    test_exec_query_by_context();
    test_exec_query_by_parent();
    test_exec_query_by_skill_revision();
    test_exec_query_by_model_revision();
    test_exec_query_combined();
    test_exec_query_null_q();
    test_exec_query_null_db();

    /* count */
    test_exec_count_all();
    test_exec_count_by_status();
    test_exec_count_by_context();
    test_exec_count_combined();
    test_exec_count_zero();
    test_exec_count_null_db();
    test_exec_count_null_q();

    /* pagination */
    test_exec_query_pagination_loop();
}
