#include "test_execution_common.h"
#include "test_common.h"
#include <stdio.h>

/* ========================================================================== */
/*  Fixture                                                                  */
/* ========================================================================== */

typedef struct {
    const char *path;
    db_t       *db;
    int         ctx_id;
    int         sr_id;
    int         mr_id;
} fx_t;

/* Opens the DB, runs exec_setup, and returns a populated fixture.
 * Aborts (via TEST_ASSERT) on any failure. */
static fx_t fx_open(const char *path) {
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int ctx_id = 0, sr_id = 0, mr_id = 0;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), ACTA_DB_OK);
    fx_t fx = { .path = path, .db = db,
                .ctx_id = ctx_id, .sr_id = sr_id, .mr_id = mr_id };
    return fx;
}

static void fx_close(fx_t *fx) {
    test_db_teardown(fx->db, fx->path);
}

/* ── Secondary-entity helpers (eliminate repeated raw-SQL / struct init) ── */

/* Create and return a second context. */
static int fx_add_ctx(fx_t *fx) {
    context_t c = {0};
    c.type         = (char *)"t2";
    c.content      = (char *)"c2";
    c.content_hash = (char *)"h2";
    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(fx->db, &c, &id), ACTA_DB_OK);
    return id;
}

/* Create a second skill revision (revision = 2) via direct SQL and
 * return its row id. */
static int fx_add_skill_rev(fx_t *fx) {
    TEST_ASSERT_EQ_INT(acta_db_exec(fx->db,
        "INSERT INTO skill_revisions (skill_id, revision, name, description, "
        "prompt_template, output_schema) "
        "VALUES (1, 2, 's2', 'd', 't', '{}')"), ACTA_DB_OK);
    int e = 0;
    skill_revision_t *sr = acta_db_skill_revision_get_by_skill_and_rev(
        fx->db, 1, 2, &e);
    TEST_ASSERT_NOT_NULL(sr);
    int id = sr->id;
    acta_db_skill_revision_free(sr);
    return id;
}

/* Create a second model revision (revision = 2) via direct SQL and
 * return its row id. */
static int fx_add_model_rev(fx_t *fx) {
    TEST_ASSERT_EQ_INT(acta_db_exec(fx->db,
        "INSERT INTO model_revisions (model_id, revision, name, description, "
        "backend, base_url, model_identifier, configuration) "
        "VALUES (1, 2, 'm2', 'd', 'openai', 'u', 'mid2', '{}')"), ACTA_DB_OK);
    int e = 0;
    model_revision_t *mr = acta_db_model_revision_get_by_model_and_rev(
        fx->db, 1, 2, &e);
    TEST_ASSERT_NOT_NULL(mr);
    int id = mr->id;
    acta_db_model_revision_free(mr);
    return id;
}

/* Create `n` filler executions (all pending, root, ctx/sr/mr from fixture).
 * Aborts on any failed insert. */
static void fx_fill(fx_t *fx, int n) {
    for (int i = 0; i < n; i++) {
        int id = exec_create(fx->db, fx->ctx_id, fx->sr_id, fx->mr_id, 0);
        TEST_ASSERT_EQ_INT(id > 0, 1);
    }
}


/* ========================================================================== */
/*  acta_db_execution_query                                                 */
/* ========================================================================== */

static void test_exec_query_any(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_any.db");
    fx_fill(&fx, 3);

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &ACTA_EXEC_QUERY_ANY, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 3);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_null_query(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_null.db");
    fx_fill(&fx, 2);

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, NULL, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_status_pending(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_st_pend.db");
    fx_fill(&fx, 3);
    int r1 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, r1), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 3);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_PENDING);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_status_completed(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_st_comp.db");

    int c1 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int c2 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, c1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(fx.db, c1, "ok"), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, c2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(fx.db, c2, "done"), ACTA_DB_OK);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_COMPLETED;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_COMPLETED);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_status_cancelled(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_st_canc.db");

    int a = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int b = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(fx.db, a), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_cancel(fx.db, b), ACTA_DB_OK);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    int c = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, c), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(fx.db, c, "ok"), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_CANCELLED;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_CANCELLED);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_status_failed(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_st_failed.db");

    int f1 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int f2 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, f1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(fx.db, f1, "boom"), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, f2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_fail(fx.db, f2, "oops"), ACTA_DB_OK);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_FAILED;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQ_STR(items[i]->status, ACTA_EXEC_STATUS_FAILED);
        TEST_ASSERT_NOT_NULL(items[i]->error);
    }
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_status_no_match(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_st_nomatch.db");
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_FAILED;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_by_context(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_ctx.db");
    int ctx2 = fx_add_ctx(&fx);

    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, ctx2, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = fx.ctx_id;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->context_id, fx.ctx_id);
    acta_db_execution_list_free(items, n);

    q.context_id = ctx2;
    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(items[0]->context_id, ctx2);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_context_no_match(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_ctx_nomatch.db");
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = 99999;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_by_parent(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_parent.db");

    int parent = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 3);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->parent_execution_id, parent);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_by_parent_none(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_parent_none.db");
    int leaf = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = leaf;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_by_skill_revision(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_skillrev.db");
    int sr2 = fx_add_skill_rev(&fx);

    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, sr2, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, sr2, fx.mr_id, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    q.skill_revision_id = fx.sr_id;
    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->skill_revision_id, fx.sr_id);
    acta_db_execution_list_free(items, n);

    q.skill_revision_id = sr2;
    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->skill_revision_id, sr2);
    acta_db_execution_list_free(items, n);

    q.skill_revision_id = 99999;
    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_by_model_revision(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_modrev.db");
    int mr2 = fx_add_model_rev(&fx);

    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, mr2, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, mr2, 0);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    q.model_revision_id = fx.mr_id;
    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_revision_id, fx.mr_id);
    acta_db_execution_list_free(items, n);

    q.model_revision_id = mr2;
    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(n, 2);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_revision_id, mr2);
    acta_db_execution_list_free(items, n);

    q.model_revision_id = 99999;
    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_combined_two(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_combo2.db");
    int ctx2 = fx_add_ctx(&fx);

    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int c = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, c), ACTA_DB_OK);

    exec_create(fx.db, ctx2, fx.sr_id, fx.mr_id, 0);
    int e_id = exec_create(fx.db, ctx2, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, e_id), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    q.context_id = fx.ctx_id;
    q.status     = ACTA_EXEC_STATUS_PENDING;
    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    q.context_id = ctx2;
    q.status     = ACTA_EXEC_STATUS_RUNNING;
    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, e_id);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_combined_all(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_combo5.db");
    int sr2  = fx_add_skill_rev(&fx);
    int mr2  = fx_add_model_rev(&fx);
    int ctx2 = fx_add_ctx(&fx);

    exec_create(fx.db, ctx2, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, sr2, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, mr2, 0);

    int parent = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int child  = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, child), ACTA_DB_OK);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status              = ACTA_EXEC_STATUS_RUNNING;
    q.parent_execution_id = parent;
    q.context_id          = fx.ctx_id;
    q.skill_revision_id   = fx.sr_id;
    q.model_revision_id   = fx.mr_id;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, child);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

/* ── pagination ── */

static void test_exec_query_limit(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_limit.db");
    fx_fill(&fx, 7);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 3, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 3);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

static void test_exec_query_offset(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_offset.db");
    fx_fill(&fx, 7);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 5, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_execution_list_free(items, n);

    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 10, 3, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_offset_limit(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_pag.db");
    fx_fill(&fx, 7);

    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    int n = 0, e = 0;
    execution_t **items;

    items = acta_db_execution_query(fx.db, &q, 0, 3, &n, &e);
    TEST_ASSERT_EQ_INT(n, 3);
    acta_db_execution_list_free(items, n);

    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 3, 3, &n, &e);
    TEST_ASSERT_EQ_INT(n, 3);
    acta_db_execution_list_free(items, n);

    n = 0; e = 0;
    items = acta_db_execution_query(fx.db, &q, 6, 3, &n, &e);
    TEST_ASSERT_EQ_INT(n, 1);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

/* ── edge cases ── */

static void test_exec_query_null_db(void) {
    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(NULL, &ACTA_EXEC_QUERY_ANY, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);
}

static void test_exec_query_negative_offset(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_negoff.db");

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &ACTA_EXEC_QUERY_ANY, -1, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(items);

    fx_close(&fx);
}

static void test_exec_query_empty_table(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_empty.db");

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &ACTA_EXEC_QUERY_ANY, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 0);

    fx_close(&fx);
}

static void test_exec_query_all_statuses(void) {
    fx_t fx = fx_open("test/acta_test_exec_q_allstat.db");

    int e1 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int e2 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    fx_fill(&fx, 3);   /* C, D, E → pending */

    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, e1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, e2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(fx.db, e2, "ok"), ACTA_DB_OK);

    int n = 0, e = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &ACTA_EXEC_QUERY_ANY, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(n, 5);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

/* ========================================================================== */
/*  acta_db_execution_count                                                  */
/* ========================================================================== */

static void test_exec_count_any(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_any.db");
    fx_fill(&fx, 3);

    int e = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &ACTA_EXEC_QUERY_ANY, &e), 3);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);

    fx_close(&fx);
}

static void test_exec_count_by_status(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_status.db");

    fx_fill(&fx, 2);   /* 2 pending */
    int r1 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, r1), ACTA_DB_OK);
    int c1 = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, c1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(fx.db, c1, "ok"), ACTA_DB_OK);

    int e = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    q.status = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 2);
    q.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 1);
    q.status = ACTA_EXEC_STATUS_COMPLETED;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 1);
    q.status = ACTA_EXEC_STATUS_FAILED;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 0);

    fx_close(&fx);
}

static void test_exec_count_by_context(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_ctx.db");
    int ctx2 = fx_add_ctx(&fx);

    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, ctx2, fx.sr_id, fx.mr_id, 0);

    int e = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = fx.ctx_id;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 2);
    q.context_id = ctx2;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 1);

    fx_close(&fx);
}

static void test_exec_count_by_parent(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_parent.db");

    int parent = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, parent);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    int e = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = parent;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 3);

    fx_close(&fx);
}

static void test_exec_count_combined(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_combo.db");
    int ctx2 = fx_add_ctx(&fx);

    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    int c = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, c), ACTA_DB_OK);
    exec_create(fx.db, ctx2, fx.sr_id, fx.mr_id, 0);

    int e = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;

    q.context_id = fx.ctx_id;
    q.status     = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 2);

    q.status = ACTA_EXEC_STATUS_RUNNING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 1);

    q.context_id = ctx2;
    q.status     = ACTA_EXEC_STATUS_PENDING;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 1);

    fx_close(&fx);
}

static void test_exec_count_zero(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_zero.db");
    exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);

    int e = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = 99999;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 0);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);

    fx_close(&fx);
}

static void test_exec_count_null_db(void) {
    int e = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(NULL, &ACTA_EXEC_QUERY_ANY, &e), -1);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_ERR_INVALID);
}

static void test_exec_count_null_query(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_nullq.db");
    fx_fill(&fx, 2);

    int e = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, NULL, &e), 2);
    TEST_ASSERT_EQ_INT(e, ACTA_DB_OK);

    fx_close(&fx);
}

static void test_exec_count_matches_lister(void) {
    fx_t fx = fx_open("test/acta_test_exec_cnt_xcheck.db");
    fx_fill(&fx, 10);
    int r = exec_create(fx.db, fx.ctx_id, fx.sr_id, fx.mr_id, 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(fx.db, r), ACTA_DB_OK);

    int e = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = ACTA_EXEC_STATUS_PENDING;

    TEST_ASSERT_EQ_INT(acta_db_execution_count(fx.db, &q, &e), 10);

    int n = 0;
    execution_t **items =
        acta_db_execution_query(fx.db, &q, 0, 0, &n, &e);
    TEST_ASSERT_EQ_INT(n, 10);
    acta_db_execution_list_free(items, n);

    fx_close(&fx);
}

/* ========================================================================== */
/*  Runner                                                                   */
/* ========================================================================== */

int run_execution_list_tests(void) {
    fprintf(stderr, "\n=== execution query/count tests ===\n");

    test_exec_query_any();
    test_exec_query_null_query();
    test_exec_query_by_status_pending();
    test_exec_query_by_status_completed();
    test_exec_query_by_status_cancelled();
    test_exec_query_by_status_failed();
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

    test_exec_count_any();
    test_exec_count_by_status();
    test_exec_count_by_context();
    test_exec_count_by_parent();
    test_exec_count_combined();
    test_exec_count_zero();
    test_exec_count_null_db();
    test_exec_count_null_query();
    test_exec_count_matches_lister();

    return test_failures;
}
