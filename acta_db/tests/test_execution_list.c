#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>



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

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_PENDING, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

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

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_COMPLETED, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

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

    int out_count = -1;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_FAILED, 0, 0, &out_count, &err);
    /* Either error or empty list */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(out_count, 0);
    }
    if (items) acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 9.29: list_by_status — invalid status ---------- */
static void test_exec_list_by_status_invalid(void) {
    const char *path = "test/acta_test_exec_list_bogus.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int out_count = -1;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, "bogus", 0, 0, &out_count, &err);
    /* Either an error or 0 results */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(out_count, 0);
    }
    if (items) acta_db_execution_list_free(items, out_count);

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

    int out_count = 0;
    int err = 0;
    execution_t **children = acta_db_execution_list_children(db, parent_id, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(children);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(children[i]->parent_execution_id, parent_id);
    }
    acta_db_execution_list_free(children, out_count);

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

    int out_count = -1;
    int err = 0;
    execution_t **children = acta_db_execution_list_children(db, leaf_id, 0, 0, &out_count, &err);
    /* Either error or empty list */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(out_count, 0);
    }
    if (children) acta_db_execution_list_free(children, out_count);

    test_db_teardown(db, path);
}

/* ---------- 9.32: list_children — non-existent parent ---------- */
static void test_exec_list_children_nonexistent(void) {
    const char *path = "test/acta_test_exec_children_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int out_count = -1;
    int err = 0;
    execution_t **children = acta_db_execution_list_children(db, 999999, 0, 0, &out_count, &err);
    /* Either an error or 0 results */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(out_count, 0);
    }
    if (children) acta_db_execution_list_free(children, out_count);

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

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_context(db, ctx_id, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx_id);
    }
    acta_db_execution_list_free(items, out_count);

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

    int out_count = -1;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_context(db, 999999, 0, 0, &out_count, &err);
    /* Either error or empty list */
    if (err == ACTA_DB_OK) {
        TEST_ASSERT_EQ_INT(out_count, 0);
    }
    if (items) acta_db_execution_list_free(items, out_count);

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

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_context(db, ctx_id, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx_id);
    }
    acta_db_execution_list_free(items, out_count);

    /* Verify ctx2 */
    out_count = 0;
    err = 0;
    items = acta_db_execution_list_by_context(db, ctx2_id, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx2_id);
    }
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ---------- 9.47: list_by_status — "cancelled" ---------- */
static void test_exec_list_by_status_cancelled(void) {
    const char *path = "test/acta_test_exec_list_cancel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* 2 cancelled */
    int c1 = exec_create(db, ctx_id, sr_id, mr_id, "Canc1", 0);
    int c2 = exec_create(db, ctx_id, sr_id, mr_id, "Canc2", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, c1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(db, c2), ACTA_DB_OK);

    /* 1 pending (should not appear) */
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, mr_id, "Pend", 0) > 0);

    /* 1 completed (should not appear) */
    int comp = exec_create(db, ctx_id, sr_id, mr_id, "Comp", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, comp), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, comp, "ok"), ACTA_DB_OK);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_status(db, ACTA_EXEC_STATUS_CANCELLED, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_CANCELLED);
    }
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ---------- 9.48: list_by_skill_revision — happy path ---------- */
static void test_exec_list_by_skill_revision_with(void) {
    const char *path = "test/acta_test_exec_list_skillrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* 3 executions under the same skill revision */
    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "S1", 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, "S2", 0);
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, "S3", 0);
    TEST_ASSERT(e1 > 0 && e2 > 0 && e3 > 0);

    /* 1 execution under a different skill revision (should not appear) */
    int other_sr = 0;
    int rv = acta_db_exec(db,
        "INSERT INTO skill_revisions (skill_id, revision, name, description, "
        "prompt_template, output_schema) "
        "VALUES (1, 2, 'other-skill', 'desc', 'tmpl', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    int err2 = 0;
    skill_revision_t *srev2 = acta_db_skill_revision_get_by_skill_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(srev2);
    other_sr = srev2->id;
    acta_db_skill_revision_free(srev2);
    TEST_ASSERT(exec_create(db, ctx_id, other_sr, mr_id, "Other", 0) > 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_skill_revision(
        db, sr_id, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->skill_revision_id, sr_id);
    }
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ---------- 9.49: list_by_skill_revision — no match ---------- */
static void test_exec_list_by_skill_revision_no_match(void) {
    const char *path = "test/acta_test_exec_list_skillrev_nom.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "X", 0);
    TEST_ASSERT(eid > 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_skill_revision(
        db, 99999, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- 9.50: list_by_skill_revision — mixed revisions ---------- */
static void test_exec_list_by_skill_revision_mixed(void) {
    const char *path = "test/acta_test_exec_list_skillrev_mix.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* second skill revision */
    int sr2 = 0;
    int rv = acta_db_exec(db,
        "INSERT INTO skill_revisions (skill_id, revision, name, description, "
        "prompt_template, output_schema) "
        "VALUES (1, 2, 'skill2', 'desc', 'tmpl', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    int err2 = 0;
    skill_revision_t *srev2 = acta_db_skill_revision_get_by_skill_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(srev2);
    sr2 = srev2->id;
    acta_db_skill_revision_free(srev2);

    /* 2 under sr_id, 2 under sr2 */
    exec_create(db, ctx_id, sr_id, mr_id, "A1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "A2", 0);
    exec_create(db, ctx_id, sr2,   mr_id, "B1", 0);
    exec_create(db, ctx_id, sr2,   mr_id, "B2", 0);

    /* list by sr2 → expect exactly 2 */
    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_skill_revision(
        db, sr2, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->skill_revision_id, sr2);
    }
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ---------- 9.51: list_by_model_revision — happy path ---------- */
static void test_exec_list_by_model_revision_with(void) {
    const char *path = "test/acta_test_exec_list_modrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* 3 executions under the same model revision */
    int e1 = exec_create(db, ctx_id, sr_id, mr_id, "M1", 0);
    int e2 = exec_create(db, ctx_id, sr_id, mr_id, "M2", 0);
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, "M3", 0);
    TEST_ASSERT(e1 > 0 && e2 > 0 && e3 > 0);

    /* 1 execution under a different model revision (should not appear) */
    int other_mr = 0;
    int rv = acta_db_exec(db,
        "INSERT INTO model_revisions (model_id, revision, name, description, "
        "backend, base_url, model_identifier, configuration) "
        "VALUES (1, 2, 'other-model', 'desc', 'openai', 'http://localhost', "
        "'other-model-id', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    int err2 = 0;
    model_revision_t *mrev2 = acta_db_model_revision_get_by_model_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(mrev2);
    other_mr = mrev2->id;
    acta_db_model_revision_free(mrev2);
    TEST_ASSERT(exec_create(db, ctx_id, sr_id, other_mr, "Other", 0) > 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_model_revision(
        db, mr_id, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->model_revision_id, mr_id);
    }
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ---------- 9.52: list_by_model_revision — no match ---------- */
static void test_exec_list_by_model_revision_no_match(void) {
    const char *path = "test/acta_test_exec_list_modrev_nom.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int eid = exec_create(db, ctx_id, sr_id, mr_id, "X", 0);
    TEST_ASSERT(eid > 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_model_revision(
        db, 99999, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- 9.53: list_by_model_revision — mixed revisions ---------- */
static void test_exec_list_by_model_revision_mixed(void) {
    const char *path = "test/acta_test_exec_list_modrev_mix.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* second model revision */
    int mr2 = 0;
    int rv = acta_db_exec(db,
        "INSERT INTO model_revisions (model_id, revision, name, description, "
        "backend, base_url, model_identifier, configuration) "
        "VALUES (1, 2, 'model2', 'desc', 'openai', 'http://localhost', "
        "'model2-id', '{}')");
    TEST_ASSERT_EQ_INT(rv, ACTA_DB_OK);
    int err2 = 0;
    model_revision_t *mrev2 = acta_db_model_revision_get_by_model_and_rev(db, 1, 2, &err2);
    TEST_ASSERT_NOT_NULL(mrev2);
    mr2 = mrev2->id;
    acta_db_model_revision_free(mrev2);

    /* 2 under mr_id, 2 under mr2 */
    exec_create(db, ctx_id, sr_id, mr_id, "A1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "A2", 0);
    exec_create(db, ctx_id, sr_id, mr2,   "B1", 0);
    exec_create(db, ctx_id, sr_id, mr2,   "B2", 0);

    /* list by mr2 → expect exactly 2 */
    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_by_model_revision(
        db, mr2, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->model_revision_id, mr2);
    }
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ---------- 9.54: list_all — with data ---------- */
static void test_exec_list_all_with_data(void) {
    const char *path = "test/acta_test_exec_list_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "B", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "C", 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_all(db, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 9.55: list_all — empty table ---------- */
static void test_exec_list_all_empty(void) {
    const char *path = "test/acta_test_exec_list_all_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_all(db, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);

    test_db_teardown(db, path);
}

/* ---------- 9.56: list_all — limit ---------- */
static void test_exec_list_all_limit(void) {
    const char *path = "test/acta_test_exec_list_all_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 5; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_all(db, 0, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 9.57: list_all — offset ---------- */
static void test_exec_list_all_offset(void) {
    const char *path = "test/acta_test_exec_list_all_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 5; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_all(db, 3, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 9.58: list_all — null db ---------- */
static void test_exec_list_all_null_db(void) {
    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_all(NULL, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);
}

/* ---------- 9.59: list_all — mixed statuses (no filter) ---------- */
static void test_exec_list_all_no_filter(void) {
    const char *path = "test/acta_test_exec_list_all_nofilter.db";
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

    /* e1 → running (started, not completed) */
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e1), ACTA_DB_OK);

    /* e2 → completed (started, then completed) */
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, e2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, e2, "ok"), ACTA_DB_OK);

    /* e3, e4, e5 → pending (never started) */

    int out_count = 0;
    int err = 0;
    execution_t **items = acta_db_execution_list_all(db, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 5);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}




/* ========================================================================== */
/*  New: acta_db_execution_query (unified lister)                            */
/* ========================================================================== */

/* ---------- 10.01: query — no filter (ACTA_EXEC_QUERY_ANY) ---------- */
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

/* ---------- 10.02: query — NULL query (same as ANY) ---------- */
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

/* ---------- 10.03: query — filter by status ---------- */
static void test_exec_query_by_status(void) {
    const char *path = "test/acta_test_exec_q_status.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    exec_create(db, ctx_id, sr_id, mr_id, "P1", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "P2", 0);
    int r1 = exec_create(db, ctx_id, sr_id, mr_id, "R1", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, r1), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_RUNNING;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 1);
    TEST_ASSERT_EQ_STR(items[0]->status, ACTA_EXEC_STATUS_RUNNING);
    acta_db_execution_list_free(items, out_count);

    /* verify the pending ones are excluded */
    q.status = ACTA_EXEC_STATUS_PENDING;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 10.04: query — filter by context_id ---------- */
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

    exec_create(db, ctx_id,   sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id,   sr_id, mr_id, "B", 0);
    exec_create(db, ctx2_id,  sr_id, mr_id, "C", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 10.05: query — filter by parent_execution_id ---------- */
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
    exec_create(db, ctx_id, sr_id, mr_id, "Unrelated", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    for (int i = 0; i < out_count; i++)
        TEST_ASSERT_EQ_INT(items[i]->parent_execution_id, parent);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 10.06: query — combined: status + context_id ---------- */
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

    /* Query: ctx1 + pending → expect a, b (2) */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ctx_id;
    q.status     = ACTA_EXEC_STATUS_PENDING;

    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 2);
    acta_db_execution_list_free(items, out_count);

    /* Query: ctx2 + running → expect e (1) */
    q.context_id = ctx2_id;
    q.status     = ACTA_EXEC_STATUS_RUNNING;
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, e);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 10.07: query — combined: all five filters ---------- */
static void test_exec_query_combined_all(void) {
    const char *path = "test/acta_test_exec_q_combo5.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    /* Create a second skill revision and model revision for the "wrong" rows */
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

    /* The one "correct" row: ctx_id, sr_id, mr_id, status=pending, parent=0 */
    int good = exec_create(db, ctx_id, sr_id, mr_id, "GOOD", 0);
    /* Start it so it's running */
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, good), ACTA_DB_OK);

    /* "Wrong" rows that differ in one dimension each */
    exec_create(db, ctx2_id, sr_id, mr_id, "WRONG_CTX", 0);
    exec_create(db, ctx_id, sr2_id, mr_id, "WRONG_SR", 0);
    exec_create(db, ctx_id, sr_id, mr2_id, "WRONG_MR", 0);

    /* Query with all five set → only the one "running" row in ctx_id/sr_id/mr_id */
    int parent = exec_create(db, ctx_id, sr_id, mr_id, "PARENT", 0);
    exec_create(db, ctx_id, sr_id, mr_id, "CHILD", parent);
    /* start child so it's running too */
    int child = exec_create(db, ctx_id, sr_id, mr_id, "CHILD2", parent);
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

/* ---------- 10.08: query — pagination (offset + limit) ---------- */
static void test_exec_query_pagination(void) {
    const char *path = "test/acta_test_exec_q_page.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);

    for (int i = 0; i < 7; i++)
        exec_create(db, ctx_id, sr_id, mr_id, "X", 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    /* page 1: offset=0, limit=3 → 3 items */
    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(out_count, 3);
    acta_db_execution_list_free(items, out_count);

    /* page 3: offset=6, limit=3 → 1 item */
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 6, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 1);
    acta_db_execution_list_free(items, out_count);

    /* beyond end: offset=10 → 0 items */
    out_count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 10, 3, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(out_count, 0);
    if (items) acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}

/* ---------- 10.09: query — null db ---------- */
static void test_exec_query_null_db(void) {
    int out_count = 0, err = 0;
    execution_t **items = acta_db_execution_query(NULL, &ACTA_EXEC_QUERY_ANY, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(out_count, 0);
}

/* ---------- 10.10: query — empty table ---------- */
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


/* ========================================================================== */
/*  New: acta_db_execution_count                                             */
/* ========================================================================== */

/* ---------- 10.11: count — no filter ---------- */
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

/* ---------- 10.12: count — filter by status ---------- */
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

/* ---------- 10.13: count — filter by context_id ---------- */
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

/* ---------- 10.14: count — filter by parent_execution_id ---------- */
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

/* ---------- 10.15: count — combined filters ---------- */
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

    /* ctx1: 2 pending, 1 running */
    exec_create(db, ctx_id,  sr_id, mr_id, "A", 0);
    exec_create(db, ctx_id,  sr_id, mr_id, "B", 0);
    int c = exec_create(db, ctx_id,  sr_id, mr_id, "C", 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, c), ACTA_DB_OK);

    /* ctx2: 1 pending */
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

/* ---------- 10.16: count — no matches returns 0 ---------- */
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

/* ---------- 10.17: count — null db ---------- */
static void test_exec_count_null_db(void) {
    int err = 0;
    int total = acta_db_execution_count(NULL, &ACTA_EXEC_QUERY_ANY, &err);
    TEST_ASSERT_EQ_INT(total, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- 10.18: count — NULL query (same as ANY) ---------- */
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

/* ---------- 10.19: count — matches lister (cross-check) ---------- */
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

    /* lister with no limit should return the same count */
    int out_count = 0;
    execution_t **items = acta_db_execution_query(db, &q, 0, 0, &out_count, &err);
    TEST_ASSERT_EQ_INT(out_count, 10);
    acta_db_execution_list_free(items, out_count);

    test_db_teardown(db, path);
}


/* ========================================================================== */
/*  Updated runner                                                          */
/* ========================================================================== */

void run_execution_list_tests(void) {
    fprintf(stderr, "\n=== execution lister tests ===\n");
    /* ── legacy (wrapper) tests ── */
    test_exec_list_by_status_pending();
    test_exec_list_by_status_completed();
    test_exec_list_by_status_no_match();
    test_exec_list_by_status_invalid();
    test_exec_list_by_status_cancelled();
    test_exec_list_children_with();
    test_exec_list_children_none();
    test_exec_list_children_nonexistent();
    test_exec_list_by_context_with();
    test_exec_list_by_context_no_match();
    test_exec_list_by_context_mixed();
    test_exec_list_by_skill_revision_with();
    test_exec_list_by_skill_revision_no_match();
    test_exec_list_by_skill_revision_mixed();
    test_exec_list_by_model_revision_with();
    test_exec_list_by_model_revision_no_match();
    test_exec_list_by_model_revision_mixed();
    test_exec_list_all_with_data();
    test_exec_list_all_empty();
    test_exec_list_all_limit();
    test_exec_list_all_offset();
    test_exec_list_all_null_db();
    test_exec_list_all_no_filter();

    /* ── acta_db_execution_query ── */
    test_exec_query_any();
    test_exec_query_null_query();
    test_exec_query_by_status();
    test_exec_query_by_context();
    test_exec_query_by_parent();
    test_exec_query_combined_two();
    test_exec_query_combined_all();
    test_exec_query_pagination();
    test_exec_query_null_db();
    test_exec_query_empty_table();

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

