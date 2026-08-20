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



/* ---------- runner ---------- */
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
}
