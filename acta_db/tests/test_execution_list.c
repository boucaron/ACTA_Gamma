#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================================================== */
/*  acta_db_execution_query (unified lister)                                */
/* ========================================================================== */

/* ---------- query — no filter (ACTA_EXEC_QUERY_ANY) ---------- */
static void test_exec_query_any(void) {
    const char *path = "test/acta_test_exec_q_any.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "B", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "C", 0);

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — NULL query (same as ANY) ---------- */
static void test_exec_query_null_query(void) {
    const char *path = "test/acta_test_exec_q_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "B", 0);

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, NULL, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — filter by status: pending ---------- */
static void test_exec_query_by_status_pending(void) {
    const char *path = "test/acta_test_exec_q_st_pend.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "P1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "P2", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "P3", 0);
    int r1 = exec_create(db, ctx_id, sr_id, mr_id, "R1", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, r1), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — filter by status: completed ---------- */
static void test_exec_query_by_status_completed(void) {
    const char *path = "test/acta_test_exec_q_st_comp.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int c1 = exec_create(db, ctx_id, sr_id, mr_id, "C1", 0);
    int c2 = exec_create(db, ctx_id, sr_id, mr_id, "C2", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, c1, "ok"), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, c2, "done"), ACTA_DB_OK);
    exec_create(db, ctx_id, sr_id, mr_id, "P1", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_COMPLETED;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_COMPLETED);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — filter by status: cancelled ---------- */
static void test_exec_query_by_status_cancelled(void) {
    const char *path = "test/acta_test_exec_q_st_canc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int a = exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    int b = exec_create(db, ctx_id, sr_id, mr_id, "B", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, a), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, b), ACTA_DB_OK);
    exec_create(db, ctx_id, sr_id, mr_id, "Pend", 0);
    int c = exec_create(db, ctx_id, sr_id, mr_id, "Comp", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, c, "ok"), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_CANCELLED;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — filter by status: no matches ---------- */
static void test_exec_query_by_status_no_match(void) {
    const char *path = "test/acta_test_exec_q_st_nomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_FAILED;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — filter by context_id ---------- */
static void test_exec_query_by_context(void) {
    const char *path = "test/acta_test_exec_q_ctx.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    context_t ctx2 = {0};
    ctx2.type         = (char *)"t2";
    ctx2.content      = (char *)"c2";
    ctx2.content_hash = (char *)"h2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &ctx2, &ctx2_id), ACTA_DB_OK);

    exec_create(db, ctx_id,  sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id,  sr_id, mr_id, "B", 0);
    exec_create(db, ctx2_id, sr_id, mr_id, "C", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx_id);
    acta_db_execution_list_free(items, out_count);

    /* verify ctx2 */
    q.context_id = ctx2_id;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 1);
    TEST_ASSERT_EQ_INT(items[0]->context_id, ctx2_id);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — filter by context_id: no match ---------- */
static void test_exec_query_by_context_no_match(void) {
    const char *path = "test/acta_test_exec_q_ctx_nomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = 99999;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — filter by parent_execution_id ---------- */
static void test_exec_query_by_parent(void) {
    const char *path = "test/acta_test_exec_q_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent = exec_create(db, ctx_id, sr_id, mr_id, "P", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "C1", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "C2", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "C3", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "Unrelated", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->parent_execution_id, parent);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — filter by parent: no children ---------- */
static void test_exec_query_by_parent_none(void) {
    const char *path = "test/acta_test_exec_q_parent_none.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int leaf = exec_create(db, ctx_id, sr_id, mr_id, "Leaf", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = leaf;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — filter by skill_revision ---------- */
static void test_exec_query_by_skill_revision(void) {
    const char *path = "test/acta_test_exec_q_skillrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* second skill revision */
    int rv = acta_db_exec(db,
        "INSERT INTO skill_revisions (skill_id, revision, name, description, "
        "prompt_template, output_schema) VALUES (1, 2, 'skill2', 'd', 't', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    int err2 = 0;
    skill_revision_t *srev2 = acta_db_skill_revision_get_by_skill_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(srev2);
    int sr2 = srev2->id;
    acta_db_skill_revision_free(srev2);

    exec_create(db, ctx_id, sr_id, mr_id, "A1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "A2", 0);
    exec_create(db, ctx_id, sr2,   mr_id, "B1", 0);
    exec_create(db, ctx_id, sr2,   mr_id, "B2", 0);

    /* filter by sr_id → 2 */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.skill_revision_id = sr_id;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->skill_revision_id, sr_id);
    acta_db_execution_list_free(items, out_count);

    /* filter by sr2 → 2 */
    q.skill_revision_id = sr2;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->skill_revision_id, sr2);
    acta_db_execution_list_free(items, out_count);

    /* nonexistent revision → 0 */
    q.skill_revision_id = 99999;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — filter by model_revision ---------- */
static void test_exec_query_by_model_revision(void) {
    const char *path = "test/acta_test_exec_q_modrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* second model revision */
    int rv = acta_db_exec(db,
        "INSERT INTO model_revisions (model_id, revision, name, description, "
        "backend, base_url, model_identifier, configuration) "
        "VALUES (1, 2, 'model2', 'd', 'openai', 'u', 'mid2', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    int err2 = 0;
    model_revision_t *mrev2 = acta_db_model_revision_get_by_model_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(mrev2);
    int mr2 = mrev2->id;
    acta_db_model_revision_free(mrev2);

    exec_create(db, ctx_id, sr_id, mr_id, "A1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "A2", 0);
    exec_create(db, ctx_id, sr_id, mr2,   "B1", 0);
    exec_create(db, ctx_id, sr_id, mr2,   "B2", 0);

    /* filter by mr_id → 2 */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.model_revision_id = mr_id;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_revision_id, mr_id);
    acta_db_execution_list_free(items, out_count);

    /* filter by mr2 → 2 */
    q.model_revision_id = mr2;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_revision_id, mr2);
    acta_db_execution_list_free(items, out_count);

    /* nonexistent revision → 0 */
    q.model_revision_id = 99999;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — combined: status + context_id ---------- */
static void test_exec_query_combined_two(void) {
    const char *path = "test/acta_test_exec_q_combo2.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    context_t ctx2 = {0};
    ctx2.type         = (char *)"t2";
    ctx2.content      = (char *)"c2";
    ctx2.content_hash = (char *)"h2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &ctx2, &ctx2_id), ACTA_DB_OK);

    /* ctx1: 2 pending, 1 running */
    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "B", 0);
    int c = exec_create(db, ctx_id, sr_id, mr_id, "C", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c), ACTA_DB_OK);

    /* ctx2: 1 pending, 1 running */
    exec_create(db, ctx2_id, sr_id, mr_id, "D", 0);
    int e = exec_create(db, ctx2_id, sr_id, mr_id, "E", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e), ACTA_DB_OK);

    /* ctx1 + pending → 2 */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    q.status     = ACTA_EXEC_STATUS_PENDING;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    /* ctx2 + running → 1 */
    q.context_id = ctx2_id;
    q.status     = ACTA_EXEC_STATUS_RUNNING;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, e);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — combined: all five filters ---------- */
static void test_exec_query_combined_all(void) {
    const char *path = "test/acta_test_exec_q_combo5.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* Create a second skill revision and model revision for "wrong" rows */
    int rv = acta_db_exec(db,
        "INSERT INTO skill_revisions (skill_id, revision, name, description, "
        "prompt_template, output_schema) VALUES (1, 2, 's2', 'd', 't', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    rv = acta_db_exec(db,
        "INSERT INTO model_revisions (model_id, revision, name, description, "
        "backend, base_url, model_identifier, configuration) "
        "VALUES (1, 2, 'm2', 'd', 'openai', 'u', 'mid', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);

    int err2 = 0;
    skill_revision_t *sr2 = acta_db_skill_revision_get_by_skill_and_rev(db, 1, 2, &err2);
    model_revision_t *mr2 = acta_db_model_revision_get_by_model_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(sr2);
    TEST_ASSERT_NOT_NULL(mr2);
    int sr2_id = sr2->id, mr2_id = mr2->id;
    acta_db_skill_revision_free(sr2);
    acta_db_model_revision_free(mr2);

    int ctx2_id = 0;
    context_t ctx2 = {0};
    ctx2.type = (char *)"t2"; ctx2.content = (char *)"c2"; ctx2.content_hash = (char *)"h2";
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &ctx2, &ctx2_id), ACTA_DB_OK);

    /* "Wrong" rows differing in one dimension each */
    exec_create(db, ctx2_id, sr_id, mr_id, "WRONG_CTX", 0);
    exec_create(db, ctx_id, sr2_id, mr_id, "WRONG_SR", 0);
    exec_create(db, ctx_id, sr_id, mr2_id, "WRONG_MR", 0);

    /* The one matching row: running, parent set, ctx_id, sr_id, mr_id */
    int parent = exec_create(db, ctx_id, sr_id, mr_id, "PARENT", 0);
    int child  = exec_create(db, ctx_id, sr_id, mr_id, "CHILD", parent);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, child), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status              = ACTA_EXEC_STATUS_RUNNING;
    q.parent_execution_id = parent;
    q.context_id          = ctx_id;
    q.skill_revision_id   = sr_id;
    q.model_revision_id   = mr_id;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, child);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — pagination: limit ---------- */
static void test_exec_query_limit(void) {
    const char *path = "test/acta_test_exec_q_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 7; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — pagination: offset ---------- */
static void test_exec_query_offset(void) {
    const char *path = "test/acta_test_exec_q_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 7; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    /* skip 5, no limit → 2 remaining */
    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 5, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    /* offset beyond end → 0 */
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 10, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — pagination: offset + limit combined ---------- */
static void test_exec_query_offset_limit(void) {
    const char *path = "test/acta_test_exec_q_pag.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 7; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    /* page 1: offset=0, limit=3 → 3 */
    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    /* page 2: offset=3, limit=3 → 3 */
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 3, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    /* page 3: offset=6, limit=3 → 1 */
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 6, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 1);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- query — null db ---------- */
static void test_exec_query_null_db(void) {
    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(NULL, &ACTA_EXEC_QUERY_ANY, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);
}

/* ---------- query — negative offset ---------- */
static void test_exec_query_negative_offset(void) {
    const char *path = "test/acta_test_exec_q_negoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, -1, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);

    test_db_teardown(db, path);
}

/* ---------- query — empty table ---------- */
static void test_exec_query_empty_table(void) {
    const char *path = "test/acta_test_exec_q_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- query — mixed statuses, no filter (returns all) ---------- */
static void test_exec_query_all_statuses(void) {
    const char *path = "test/acta_test_exec_q_allstat.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, "B", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "C", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "D", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "E", 0);

    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, e2, "ok"), ACTA_DB_OK);

    /* 5 rows total: 1 running, 1 completed, 3 pending */
    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 5);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ========================================================================== */
/*  acta_db_execution_count                                                   */
/* ========================================================================== */

/* ---------- count — no filter ---------- */
static void test_exec_count_any(void) {
    const char *path = "test/acta_test_exec_cnt_any.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "B", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "C", 0);

    int err = 0;
    int total = acta_db_execution_count(db, &ACTA_EXEC_QUERY_ANY, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 3);

    test_db_teardown(db, path);
}

/* ---------- count — filter by status ---------- */
static void test_exec_count_by_status(void) {
    const char *path = "test/acta_test_exec_cnt_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "P1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "P2", 0);
    int r1 = exec_create(db, ctx_id, sr_id, mr_id, "R1", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, r1), ACTA_DB_OK);
    int c1 = exec_create(db, ctx_id, sr_id, mr_id, "C1", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, c1, "ok"), ACTA_DB_OK);

    int err = 0;

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 2);

    q.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    q.status = ACTA_EXEC_STATUS_COMPLETED;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    q.status = ACTA_EXEC_STATUS_FAILED;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 0);

    test_db_teardown(db, path);
}

/* ---------- count — filter by context_id ---------- */
static void test_exec_count_by_context(void) {
    const char *path = "test/acta_test_exec_cnt_ctx.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    context_t ctx2 = {0};
    ctx2.type = (char *)"t2"; ctx2.content = (char *)"c2"; ctx2.content_hash = (char *)"h2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &ctx2, &ctx2_id), ACTA_DB_OK);

    exec_create(db, ctx_id,  sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id,  sr_id, mr_id, "B", 0);
    exec_create(db, ctx2_id, sr_id, mr_id, "C", 0);

    int err = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 2);

    q.context_id = ctx2_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    test_db_teardown(db, path);
}

/* ---------- count — filter by parent_execution_id ---------- */
static void test_exec_count_by_parent(void) {
    const char *path = "test/acta_test_exec_cnt_parent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int parent = exec_create(db, ctx_id, sr_id, mr_id, "P", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "C1", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "C2", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "C3", parent);
    exec_create(db, ctx_id, sr_id, mr_id, "Root", 0);

    int err = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 3);

    test_db_teardown(db, path);
}

/* ---------- count — combined filters ---------- */
static void test_exec_count_combined(void) {
    const char *path = "test/acta_test_exec_cnt_combo.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    context_t ctx2 = {0};
    ctx2.type = (char *)"t2"; ctx2.content = (char *)"c2"; ctx2.content_hash = (char *)"h2";
    int ctx2_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &ctx2, &ctx2_id), ACTA_DB_OK);

    exec_create(db, ctx_id,  sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id,  sr_id, mr_id, "B", 0);
    int c = exec_create(db, ctx_id,  sr_id, mr_id, "C", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c), ACTA_DB_OK);

    exec_create(db, ctx2_id, sr_id, mr_id, "D", 0);

    int err = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    q.status     = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 2);

    q.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    q.context_id = ctx2_id;
    q.status     = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 1);

    test_db_teardown(db, path);
}

/* ---------- count — no matches returns 0 ---------- */
static void test_exec_count_zero(void) {
    const char *path = "test/acta_test_exec_cnt_zero.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);

    int err = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = 99999;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(db, &q, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- count — null db ---------- */
static void test_exec_count_null_db(void) {
    int err = 0;
    int total = acta_db_execution_count(NULL, &ACTA_EXEC_QUERY_ANY, &err);
    TEST_ASSERT_EQ_INT(total, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- count — NULL query (same as ANY) ---------- */
static void test_exec_count_null_query(void) {
    const char *path = "test/acta_test_exec_cnt_nullq.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "B", 0);

    int err = 0;
    int total = acta_db_execution_count(db, NULL, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 2);

    test_db_teardown(db, path);
}

/* ---------- count — matches lister (cross-check) ---------- */
static void test_exec_count_matches_lister(void) {
    const char *path = "test/acta_test_exec_cnt_xcheck.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 10; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);
    int r = exec_create(db, ctx_id, sr_id, mr_id, "R", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, r), ACTA_DB_OK);

    int err = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;

    int total = acta_db_execution_count(db, &q, &err);
    TEST_ASSERT_EQ_INT(total, 10);

    int out_count = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 10);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ========================================================================== */
/*  Runner                                                                  */
/* ========================================================================== */

void run_execution_list_tests(void) {
    fprintf(stderr, "\n=== execution query/count tests ===\n");

    /* ── acta_db_execution_query ── */
    test_exec_query_any();
    test_exec_query_null_query();
    test_exec_query_by_status_pending();
    test_exec_query_by_status_completed();
    test_exec_query_by_status_cancelled();
    test_exec_query_by_status_no_match();
    test_exec_query_by_context();
    test_exec_query_by_context_no_match();
    test_exec_query_by_parent();
    test_exec_query_by_parent_none();
    test_exec_query_by_skill_revision();
    test_exec_query_by_model_revision();
    test_exec_query_combined_two();
    test_exec_query_combined_all();
    test_exec_query_limit();
    test_exec_query_offset();
    test_exec_query_offset_limit();
    test_exec_query_null_db();
    test_exec_query_negative_offset();
    test_exec_query_empty_table();
    test_exec_query_all_statuses();

    /* ── acta_db_execution_count ── */
    test_exec_count_any();
    test_exec_count_by_status();
    test_exec_count_by_context();
    test_exec_count_by_parent();
    test_exec_count_combined();
    test_exec_count_zero();
    test_exec_count_null_db();
    test_exec_count_null_query();
    test_exec_count_matches_lister();
}
