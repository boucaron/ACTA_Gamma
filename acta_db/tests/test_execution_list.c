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
    int e3 = exec_create(db, ctx_id, sr_id, mr_id, "C", 0);
    int e4 = exec_create(db, ctx_id, sr_id, mr_id, "D", 0);
    int e5 = exec_create(db, ctx_id, sr_id, mr_id, "E", 0);

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







/* ---------- runner ---------- */
void run_execution_list_tests(void) {
    fprintf(stderr, "\n=== execution lister tests ===\n");
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
}
