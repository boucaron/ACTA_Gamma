/* test_execution_deleted.c – unit tests for the execution soft-delete
 * lifecycle (delete / restore / reset-on-deleted / create-with-deleted-
 * context / live-only default vs include_deleted query flag).
 */

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

/* Open env + create one execution, return its id. */
static int env_exec(env_t *e, int parent_id) {
    int eid = exec_create(e->db, e->ctx_id, e->sr_id, e->mr_id, parent_id);
    TEST_ASSERT(eid > 0);
    return eid;
}

/* Transition helpers: drive an execution to the given terminal-ish
 * state and return ACTA_DB_OK or the failing code. */
static int env_to_completed(env_t *e, int eid) {
    int rc = acta_db_execution_start(e->db, eid);
    if (rc != ACTA_DB_OK) return rc;
    return acta_db_execution_complete(e->db, eid, "done");
}

static int env_to_failed(env_t *e, int eid) {
    int rc = acta_db_execution_start(e->db, eid);
    if (rc != ACTA_DB_OK) return rc;
    return acta_db_execution_fail(e->db, eid, "boom");
}

static int env_to_cancelled(env_t *e, int eid) {
    return acta_db_execution_cancel(e->db, eid);
}

/* ================================================================== */
/*  delete                                                            */
/* ================================================================== */

static void test_exec_delete_pending(void) {
    env_t e = env_open("test/acta_test_execdel_pending.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_NOT_NULL(got->deleted_at);
    acta_db_execution_free(got);

    env_close(&e);
}

static void test_exec_delete_completed(void) {
    env_t e = env_open("test/acta_test_execdel_completed.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(env_to_completed(&e, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    env_close(&e);
}

static void test_exec_delete_failed(void) {
    env_t e = env_open("test/acta_test_execdel_failed.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(env_to_failed(&e, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    env_close(&e);
}

static void test_exec_delete_cancelled(void) {
    env_t e = env_open("test/acta_test_execdel_cancelled.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(env_to_cancelled(&e, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    env_close(&e);
}

static void test_exec_delete_running_is_invalid(void) {
    env_t e = env_open("test/acta_test_execdel_running.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(e.db, eid), ACTA_DB_OK);
    /* deleting a running execution is forbidden */
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid),
                       ACTA_DB_ERR_INVALID);

    /* row is still live and running */
    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_NULL(got->deleted_at);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_RUNNING);
    acta_db_execution_free(got);

    env_close(&e);
}

static void test_exec_delete_already_deleted_is_not_found(void) {
    env_t e = env_open("test/acta_test_execdel_twice.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);
    /* second delete: row exists but is already deleted → NOT_FOUND
     * (same contract as context delete, where a missing or
     * already-deleted row both map to NOT_FOUND) */
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid),
                       ACTA_DB_ERR_NOT_FOUND);

    env_close(&e);
}

static void test_exec_delete_missing(void) {
    env_t e = env_open("test/acta_test_execdel_missing.db");

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, 9999),
                       ACTA_DB_ERR_NOT_FOUND);

    env_close(&e);
}

static void test_exec_delete_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(NULL, 1),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  restore                                                           */
/* ================================================================== */

static void test_exec_restore_round_trip_preserves_status(void) {
    env_t e = env_open("test/acta_test_execrestore_round.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(env_to_failed(&e, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    TEST_ASSERT_EQ_INT(acta_db_execution_restore(e.db, eid), ACTA_DB_OK);

    /* status untouched: still failed, and resettable again */
    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_NULL(got->deleted_at);
    TEST_ASSERT_EQ_STR(got->status, ACTA_EXEC_STATUS_FAILED);
    acta_db_execution_free(got);

    TEST_ASSERT_EQ_INT(acta_db_execution_reset(e.db, eid), ACTA_DB_OK);

    env_close(&e);
}

static void test_exec_restore_missing(void) {
    env_t e = env_open("test/acta_test_execrestore_missing.db");

    TEST_ASSERT_EQ_INT(acta_db_execution_restore(e.db, 9999),
                       ACTA_DB_ERR_NOT_FOUND);

    env_close(&e);
}

static void test_exec_restore_live_is_error(void) {
    env_t e = env_open("test/acta_test_execrestore_live.db");
    int eid = env_exec(&e, 0);

    /* strict: restoring a live row is NOT_FOUND (like context_restore) */
    TEST_ASSERT_EQ_INT(acta_db_execution_restore(e.db, eid),
                       ACTA_DB_ERR_NOT_FOUND);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_NULL(got->deleted_at);
    acta_db_execution_free(got);

    env_close(&e);
}

static void test_exec_restore_null_db(void) {
    TEST_ASSERT_EQ_INT(acta_db_execution_restore(NULL, 1),
                       ACTA_DB_ERR_INVALID);
}

/* ================================================================== */
/*  reset on a deleted row                                           */
/* ================================================================== */

static void test_exec_reset_deleted_is_not_found(void) {
    env_t e = env_open("test/acta_test_execreset_deleted.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(env_to_failed(&e, eid), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    /* a deleted execution must be restored before it can be reset */
    TEST_ASSERT_EQ_INT(acta_db_execution_reset(e.db, eid),
                       ACTA_DB_ERR_NOT_FOUND);

    env_close(&e);
}

/* ================================================================== */
/*  create with a deleted / missing context                          */
/* ================================================================== */

static void test_exec_create_deleted_context(void) {
    env_t e = env_open("test/acta_test_execcreate_deletedctx.db");
    int eid = env_exec(&e, 0);
    (void)eid;

    TEST_ASSERT_EQ_INT(acta_db_context_delete(e.db, e.ctx_id), ACTA_DB_OK);

    execution_t x = {0};
    x.context_id        = e.ctx_id;
    x.skill_revision_id = e.sr_id;
    x.model_revision_id = e.mr_id;
    int out_id = 0;
    TEST_ASSERT_EQ_INT(
        acta_db_execution_create(e.db, &x, &out_id),
        ACTA_DB_ERR_NOT_FOUND);

    env_close(&e);
}

static void test_exec_create_missing_context(void) {
    env_t e = env_open("test/acta_test_execcreate_missingctx.db");

    execution_t x = {0};
    x.context_id        = 9999;
    x.skill_revision_id = e.sr_id;
    x.model_revision_id = e.mr_id;
    int out_id = 0;
    /* unchanged FK semantics: a missing context is ERR_FK */
    TEST_ASSERT_EQ_INT(
        acta_db_execution_create(e.db, &x, &out_id),
        ACTA_DB_ERR_FK);

    env_close(&e);
}

/* ================================================================== */
/*  query / count: live-only default vs include_deleted              */
/* ================================================================== */

static void test_exec_query_default_live_only(void) {
    env_t e = env_open("test/acta_test_execquery_live.db");
    int a = env_exec(&e, 0);
    int b = env_exec(&e, 0);
    (void)b;

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, a), ACTA_DB_OK);

    int n = 0, err = 0;
    execution_t **rows = acta_db_execution_query(e.db,
                                                &ACTA_EXEC_QUERY_ANY,
                                                0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, b);
    TEST_ASSERT_NULL(rows[0]->deleted_at);
    acta_db_execution_list_free(rows, n);

    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db,
                                               &ACTA_EXEC_QUERY_ANY,
                                               &err), 1);

    env_close(&e);
}

static void test_exec_query_include_deleted(void) {
    env_t e = env_open("test/acta_test_execquery_wd.db");
    int a = env_exec(&e, 0);
    int b = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, a), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.include_deleted = 1;

    int n = 0, err = 0;
    execution_t **rows = acta_db_execution_query(e.db, &q, 0, 0,
                                                &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    TEST_ASSERT_EQ_INT(rows[0]->id, a);
    TEST_ASSERT_NOT_NULL(rows[0]->deleted_at);
    TEST_ASSERT_EQ_INT(rows[1]->id, b);
    TEST_ASSERT_NULL(rows[1]->deleted_at);
    acta_db_execution_list_free(rows, n);

    TEST_ASSERT_EQ_INT(acta_db_execution_count(e.db, &q, &err), 2);

    /* filtered: status + include_deleted still AND */
    q.status = ACTA_EXEC_STATUS_PENDING;
    n = 0; err = 0;
    rows = acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(rows, n);

    env_close(&e);
}

static void test_exec_query_include_deleted_null_q(void) {
    env_t e = env_open("test/acta_test_execquery_wd_null.db");
    int a = env_exec(&e, 0);
    int b = env_exec(&e, 0);
    (void)b;

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, a), ACTA_DB_OK);

    /* NULL query == ACTA_EXEC_QUERY_ANY == live-only */
    int n = 0, err = 0;
    execution_t **rows = acta_db_execution_query(e.db, NULL, 0, 0,
                                                &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, b);
    acta_db_execution_list_free(rows, n);

    env_close(&e);
}

/* ================================================================== */
/*  get on a deleted row                                              */
/* ================================================================== */

static void test_exec_get_deleted_row(void) {
    env_t e = env_open("test/acta_test_execget_deleted.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, eid);
    TEST_ASSERT_NOT_NULL(got->deleted_at);
    acta_db_execution_free(got);

    env_close(&e);
}

/* ================================================================== */
/*  free: deleted_at string is released                               */
/* ================================================================== */

static void test_exec_free_deleted_row(void) {
    env_t e = env_open("test/acta_test_execfree_deleted.db");
    int eid = env_exec(&e, 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_delete(e.db, eid), ACTA_DB_OK);

    int err = 0;
    execution_t *got = acta_db_execution_get(e.db, eid, &err);
    TEST_ASSERT_NOT_NULL(got);
    acta_db_execution_free(got);

    /* the only row is deleted: the live-only default (NULL query)
     * returns an empty result; include_deleted sees it */
    int n = 0;
    execution_t **rows = acta_db_execution_query(e.db, NULL, 0, 0,
                                                &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);
    TEST_ASSERT_NULL(rows);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.include_deleted = 1;
    rows = acta_db_execution_query(e.db, &q, 0, 0, &n, &err);
    TEST_ASSERT_NOT_NULL(rows);
    TEST_ASSERT_EQ_INT(n, 1);
    acta_db_execution_list_free(rows, n);

    env_close(&e);
}

/* ================================================================== */
/*  Entry point                                                       */
/* ================================================================== */

int run_execution_deleted_tests(void) {
    test_exec_delete_pending();
    test_exec_delete_completed();
    test_exec_delete_failed();
    test_exec_delete_cancelled();
    test_exec_delete_running_is_invalid();
    test_exec_delete_already_deleted_is_not_found();
    test_exec_delete_missing();
    test_exec_delete_null_db();

    test_exec_restore_round_trip_preserves_status();
    test_exec_restore_missing();
    test_exec_restore_live_is_error();
    test_exec_restore_null_db();

    test_exec_reset_deleted_is_not_found();

    test_exec_create_deleted_context();
    test_exec_create_missing_context();

    test_exec_query_default_live_only();
    test_exec_query_include_deleted();
    test_exec_query_include_deleted_null_q();

    test_exec_get_deleted_row();
    test_exec_free_deleted_row();

    return test_failures;
}
