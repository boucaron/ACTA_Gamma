#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>


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

/* ---------- 9.40: cancel — pending → cancelled ---------- */
static void test_exec_cancel_pending_to_cancelled(void) {
    const char *path = "test/acta_test_exec_cancel_pend.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CancelPend", 0);
    TEST_ASSERT(eid > 0);

    int rc = acta_db_execution_cancel(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.41: cancel — running → cancelled ---------- */
static void test_exec_cancel_running_to_cancelled(void) {
    const char *path = "test/acta_test_exec_cancel_run.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CancelRun", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    int rc = acta_db_execution_cancel(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ---------- 9.42: cancel — already cancelled ---------- */
static void test_exec_cancel_already_cancelled(void) {
    const char *path = "test/acta_test_exec_cancel_twice.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CancelTwice", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, eid), ACTA_DB_OK);

    int rc = acta_db_execution_cancel(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.43: cancel — completed execution ---------- */
static void test_exec_cancel_completed(void) {
    const char *path = "test/acta_test_exec_cancel_comp.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CancelComp", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, eid, "done"), ACTA_DB_OK);

    int rc = acta_db_execution_cancel(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.44: cancel — failed execution ---------- */
static void test_exec_cancel_failed(void) {
    const char *path = "test/acta_test_exec_cancel_fail.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CancelFail", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(db, eid, "oops"), ACTA_DB_OK);

    int rc = acta_db_execution_cancel(db, eid);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 9.45: cancel — non-existent id ---------- */
static void test_exec_cancel_nonexistent(void) {
    const char *path = "test/acta_test_exec_cancel_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int rc = acta_db_execution_cancel(db, 999999);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

/* ---------- 9.46: cancel — completed_at is set ---------- */
static void test_exec_cancel_completed_at(void) {
    const char *path = "test/acta_test_exec_cancel_at.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "CancelAt", 0);
    TEST_ASSERT(eid > 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_CANCELLED);
    TEST_ASSERT(got->completed_at != NULL);
    acta_db_execution_free(got);

    test_db_teardown(db, path);
}

/* ================================================================ */
/*  Unified query + count tests                                      */
/* ================================================================ */

/* ---------- Q.1: query with ACTA_EXEC_QUERY_ANY returns all ---------- */
static void test_exec_query_any_returns_all(void) {
    const char *path = "test/acta_test_exec_q_any.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "Q1", 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, "Q2", 0);
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, "Q3", 0);
    TEST_ASSERT(e1 > 0 && e2 > 0 && e3 > 0);

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(
        db, &ACTA_EXEC_QUERY_ANY, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 3);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.2: query with limit caps the result ---------- */
static void test_exec_query_limit_caps(void) {
    const char *path = "test/acta_test_exec_q_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 10; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(
        db, &ACTA_EXEC_QUERY_ANY, 0, 4, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 4);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.3: query with offset skips rows ---------- */
static void test_exec_query_offset_skips(void) {
    const char *path = "test/acta_test_exec_q_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int ids[5];
    for (int i = 0; i < 5; i++)
        ids[i] = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    /* offset=2, limit=2 → should get rows 3 and 4 (index 2,3) */
    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(
        db, &ACTA_EXEC_QUERY_ANY, 2, 2, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.4: query with offset beyond total → empty ---------- */
static void test_exec_query_offset_exhausted(void) {
    const char *path = "test/acta_test_exec_q_exhausted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    int n = 99, err = 0;
    execution_t **items = acta_db_execution_query(
        db, &ACTA_EXEC_QUERY_ANY, 50, 10, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);
    TEST_ASSERT(items == NULL);

    test_db_teardown(db, path);
}

/* ---------- Q.5: query filter by status ---------- */
static void test_exec_query_by_status(void) {
    const char *path = "test/acta_test_exec_q_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1), ACTA_DB_OK);
    /* e2, e3 remain pending */

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_list_free(items, n);

    /* now filter running → 1 */
    q.status = ACTA_EXEC_STATUS_RUNNING;
    items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_STR(items[0]->status, ACTA_EXEC_STATUS_RUNNING);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.6: query filter by context_id ---------- */
static void test_exec_query_by_context(void) {
    const char *path = "test/acta_test_exec_q_ctx.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, ctx2_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* create a second context */
    context_t c2 = { .type = (char *)"test", .content = (char *)"x",
                 .content_hash = (char *)"hash_q_ctx_2" };
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c2, &ctx2_id), ACTA_DB_OK);

    exec_create(db, ctx_id,  sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id,  sr_id, mr_id, "Q", 0);
    exec_create(db, ctx2_id, sr_id, mr_id, "Q", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.7: query filter by parent_execution_id ---------- */
static void test_exec_query_by_parent(void) {
    const char *path = "test/acta_test_exec_q_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent = exec_create(db, ctx_id, sr_id, mr_id, "Parent", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);  /* root, not a child */

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.8: query filter by skill_revision_id ---------- */
static void test_exec_query_by_skill_revision(void) {
    const char *path = "test/acta_test_exec_q_sr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int other_sr = sr_id + 999;  /* non-existent revision id */
    exec_create(db, ctx_id, sr_id,    mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id,    mr_id, "Q", 0);
    exec_create(db, ctx_id, other_sr, mr_id, "Q", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.skill_revision_id = sr_id;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.9: query filter by model_revision_id ---------- */
static void test_exec_query_by_model_revision(void) {
    const char *path = "test/acta_test_exec_q_mr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int other_mr = mr_id + 999;
    exec_create(db, ctx_id, sr_id, mr_id,    "Q", 0);
    exec_create(db, ctx_id, sr_id, other_mr, "Q", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.model_revision_id = mr_id;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.10: query combined filters (status + context) ---------- */
static void test_exec_query_combined(void) {
    const char *path = "test/acta_test_exec_q_combo.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, ctx2_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    context_t c2 = { .type = (char *)"test", .content = (char *)"y",
                 .content_hash = (char *)"hash_q_combo_2" };
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c2, &ctx2_id), ACTA_DB_OK);

    /* ctx1: pending, running */
    int e1 = exec_create(db, ctx_id,    sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id,    sr_id, mr_id, "Q", 0);
    /* ctx2: pending, running */
    int e3 = exec_create(db, ctx2_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx2_id, sr_id, mr_id, "Q", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e3), ACTA_DB_OK);
    /* e2, e4 remain pending */

    /* filter: ctx1 + running → only e1 */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    q.status = ACTA_EXEC_STATUS_RUNNING;

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, e1);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.11: query with NULL q pointer → same as ANY ---------- */
static void test_exec_query_null_q(void) {
    const char *path = "test/acta_test_exec_q_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    test_db_teardown(db, path);
}

/* ---------- Q.12: query NULL db → INVALID ---------- */
static void test_exec_query_null_db(void) {
    int n = 0, err = 0;
    execution_t **items = acta_db_execution_query(NULL, &ACTA_EXEC_QUERY_ANY,
                                                   0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT(items == NULL);
}

/* ---------- Q.13: count all ---------- */
static void test_exec_count_all(void) {
    const char *path = "test/acta_test_exec_cnt_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 7; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    int err = 0;
    int total = acta_db_execution_count(db, &ACTA_EXEC_QUERY_ANY, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 7);

    test_db_teardown(db, path);
}

/* ---------- Q.14: count by status ---------- */
static void test_exec_count_by_status(void) {
    const char *path = "test/acta_test_exec_cnt_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e2), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    q.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 2);

    test_db_teardown(db, path);
}

/* ---------- Q.15: count by context ---------- */
static void test_exec_count_by_context(void) {
    const char *path = "test/acta_test_exec_cnt_ctx.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, ctx2_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    context_t c2 = { .type = (char *)"test", .content = (char *)"z",
                 .content_hash = (char *)"hash_cnt_ctx_2" };
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c2, &ctx2_id), ACTA_DB_OK);

    exec_create(db, ctx_id,    sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id,    sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id,    sr_id, mr_id, "Q", 0);
    exec_create(db, ctx2_id, sr_id, mr_id, "Q", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 3);

    q.context_id = ctx2_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    test_db_teardown(db, path);
}

/* ---------- Q.16: count combined filters ---------- */
static void test_exec_count_combined(void) {
    const char *path = "test/acta_test_exec_cnt_combo.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e3), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, e1, "ok"), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(db, e2, "err"), ACTA_DB_OK);
    /* e3 still running */

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    q.status = ACTA_EXEC_STATUS_RUNNING;
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    test_db_teardown(db, path);
}

/* ---------- Q.17: count with no matches → 0 ---------- */
static void test_exec_count_zero(void) {
    const char *path = "test/acta_test_exec_cnt_zero.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_COMPLETED;  /* none completed */
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- Q.18: count with NULL db → error ---------- */
static void test_exec_count_null_db(void) {
    int err = 0;
    int n = acta_db_execution_count(NULL, &ACTA_EXEC_QUERY_ANY, &err);
    TEST_ASSERT_EQ_INT(n, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- Q.19: count with NULL q → all ---------- */
static void test_exec_count_null_q(void) {
    const char *path = "test/acta_test_exec_cnt_nullq.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, NULL, &err), 3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- Q.20: pagination loop drains all rows ---------- */
static void test_exec_query_pagination_loop(void) {
    const char *path = "test/acta_test_exec_q_page_loop.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int total_to_create = 25;
    for (int i = 0; i < total_to_create; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "Q", 0);

    int err = 0;
    int total = acta_db_execution_count(db, &ACTA_EXEC_QUERY_ANY, &err);
    TEST_ASSERT_EQ_INT(total, total_to_create);

    int page_size = 7;
    int fetched = 0;
    for (int offset = 0; offset < total; offset += page_size) {
        int n = 0;
        execution_t **items = acta_db_execution_query(
            db, &ACTA_EXEC_QUERY_ANY, offset, page_size, &n, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        TEST_ASSERT(n > 0 && n <= page_size);
        fetched += n;
        acta_db_execution_list_free(items, n);
    }
    TEST_ASSERT_EQ_INT(fetched, total_to_create);

    test_db_teardown(db, path);
}


/* ---------- runner (appended) ---------- */
void run_execution_lifecycle_tests(void) {
    fprintf(stderr, "\n=== execution lifecycle tests ===\n");
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
    test_exec_cancel_pending_to_cancelled();
    test_exec_cancel_running_to_cancelled();
    test_exec_cancel_already_cancelled();
    test_exec_cancel_completed();
    test_exec_cancel_failed();
    test_exec_cancel_nonexistent();
    test_exec_cancel_completed_at();
    test_exec_set_raw_valid();
    test_exec_set_raw_null();
    test_exec_set_raw_nonexistent();

    /* unified query + count */
    test_exec_query_any_returns_all();
    test_exec_query_limit_caps();
    test_exec_query_offset_skips();
    test_exec_query_offset_exhausted();
    test_exec_query_by_status();
    test_exec_query_by_context();
    test_exec_query_by_parent();
    test_exec_query_by_skill_revision();
    test_exec_query_by_model_revision();
    test_exec_query_combined();
    test_exec_query_null_q();
    test_exec_query_null_db();
    test_exec_count_all();
    test_exec_count_by_status();
    test_exec_count_by_context();
    test_exec_count_combined();
    test_exec_count_zero();
    test_exec_count_null_db();
    test_exec_count_null_q();
    test_exec_query_pagination_loop();
}

