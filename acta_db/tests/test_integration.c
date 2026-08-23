/* test_integration.c */
#include "test_common.h"

/* ================================================================
 *  Helpers — collapse the repeated "create ctx/model/skill/rev"
 *  boilerplate that appears in nearly every test below.
 * ================================================================ */

static int make_context(db_t *db, const char *type,
                        const char *content, const char *hash) {
    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type         = type;
    ctx.content      = content;
    ctx.content_hash = hash;
    ctx.metadata     = NULL;
    int id = 0;
    int rc = acta_db_context_create(db, &ctx, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    return id;
}

static int make_model(db_t *db, const char *name) {
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id        = 0;
    m.name             = name;
    m.description      = NULL;
    m.backend          = "openai";
    m.base_url         = NULL;
    m.model_identifier = "gpt-4";
    m.configuration    = NULL;
    int id = 0;
    int rc = acta_db_model_create(db, &m, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    return id;
}

static int make_skill(db_t *db, const char *name) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id       = 0;
    s.name            = name;
    s.description     = NULL;
    s.prompt_template = "prompt";
    s.output_schema   = NULL;
    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    return id;
}

static model_revision_t *get_model_rev(db_t *db, int model_id, int rev) {
    int err = 0;
    model_revision_t *r = acta_db_model_revision_get_by_model_and_rev(db, model_id, rev, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static skill_revision_t *get_skill_rev(db_t *db, int skill_id, int rev) {
    skill_revision_t *r = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, rev, NULL);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static int make_execution(db_t *db, int ctx_id,
                          int skill_rev_id, int model_rev_id,
                          const char *prompt) {
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id        = ctx_id;
    e.skill_revision_id = skill_rev_id;
    e.model_revision_id = model_rev_id;
    e.prompt            = prompt;
    e.status            = ACTA_EXEC_STATUS_PENDING;
    int id = 0;
    int rc = acta_db_execution_create(db, &e, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    return id;
}

/* Convenience: build the full 4-piece set (ctx, model, skill, revs) */
typedef struct {
    int context_id;
    int model_id;
    int skill_id;
    model_revision_t *mrev;
    skill_revision_t *srev;
} test_set_t;

static test_set_t make_test_set(db_t *db, const char *tag) {
    test_set_t ts;
    ts.context_id = make_context(db, tag, "content", tag);
    ts.model_id   = make_model(db, tag);
    ts.skill_id   = make_skill(db, tag);
    ts.mrev       = get_model_rev(db, ts.model_id, 1);
    ts.srev       = get_skill_rev(db, ts.skill_id, 1);
    return ts;
}

static void free_test_set(test_set_t *ts) {
    acta_db_model_revision_free(ts->mrev);
    acta_db_skill_revision_free(ts->srev);
}

/* ================================================================
 *  Tests
 * ================================================================ */

/* ---------- 11.1: Full lifecycle — model ---------- */
static void test_integration_model_lifecycle(void) {
    const char *path = "test/acta_test_int_model_lc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Folder */
    int folder_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_model_folder_create(db, "TestFolder", 0, &folder_id), 0);
    TEST_ASSERT(folder_id > 0);

    /* Create model in that folder */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id        = folder_id;
    m.name             = "TestModel";
    m.description      = "A test model";
    m.backend          = "openai";
    m.base_url         = "https://api.openai.com";
    m.model_identifier = "gpt-4";
    m.configuration    = NULL;

    int model_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_model_create(db, &m, &model_id), 0);
    TEST_ASSERT(model_id > 0);

    /* Revision 1 exists and carries the full model snapshot */
    int rev_count = 0;
    int err = 0;
    model_revision_t **revs = acta_db_model_revision_list_by_model(db, model_id, 0, -1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT(revs != NULL);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 1);
    TEST_ASSERT_EQ_INT(revs[0]->model_id, model_id);
    TEST_ASSERT_EQ_INT(revs[0]->folder_id, folder_id);
    TEST_ASSERT_EQ_STR(revs[0]->name, "TestModel");
    TEST_ASSERT_EQ_STR(revs[0]->description, "A test model");
    TEST_ASSERT_EQ_STR(revs[0]->backend, "openai");
    TEST_ASSERT_EQ_STR(revs[0]->base_url, "https://api.openai.com");
    TEST_ASSERT_EQ_STR(revs[0]->model_identifier, "gpt-4");
    TEST_ASSERT(revs[0]->deleted_at == NULL);
    acta_db_model_revision_list_free(revs, rev_count);

    /* Update → revision 2 (snapshot captures new description) */
    model_t m2;
    memset(&m2, 0, sizeof(m2));
    m2.id             = model_id;
    m2.folder_id      = folder_id;
    m2.name           = "TestModel";
    m2.description    = "Updated description";
    m2.backend        = "openai";
    m2.base_url       = "https://api.openai.com";
    m2.model_identifier = "gpt-4";
    m2.configuration  = NULL;

    TEST_ASSERT_EQ_INT(acta_db_model_update(db, &m2), 0);

    rev_count = 0; err = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, 0, -1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 2);
    /* rev 1 retains original description */
    TEST_ASSERT_EQ_STR(revs[0]->description, "A test model");
    /* rev 2 has the updated description */
    TEST_ASSERT_EQ_INT(revs[1]->revision, 2);
    TEST_ASSERT_EQ_STR(revs[1]->description, "Updated description");
    acta_db_model_revision_list_free(revs, rev_count);

    /* Soft-delete → revision 3 with deleted_at */
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), 0);

    rev_count = 0; err = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, 0, -1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 3);
    TEST_ASSERT(revs[2]->deleted_at != NULL);
    /* rev 3 snapshot still carries the model fields */
    TEST_ASSERT_EQ_STR(revs[2]->name, "TestModel");
    TEST_ASSERT_EQ_STR(revs[2]->description, "Updated description");
    acta_db_model_revision_list_free(revs, rev_count);

    /* acta_db_model_revision_get by explicit id */
    {
        model_revision_t *single = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        TEST_ASSERT_NOT_NULL(single);
        TEST_ASSERT_EQ_INT(single->revision, 1);
        TEST_ASSERT_EQ_STR(single->name, "TestModel");
        acta_db_model_revision_free(single);
    }

    /* acta_db_model_revision_get_latest returns the most recent revision */
    {
        err = 0;
        model_revision_t *latest = acta_db_model_revision_get_latest(db, model_id, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        TEST_ASSERT_NOT_NULL(latest);
        TEST_ASSERT_EQ_INT(latest->revision, 3);
        TEST_ASSERT(latest->deleted_at != NULL);
        acta_db_model_revision_free(latest);
    }

    /* Pagination: limit=1 offset=0 → only rev 1 */
    rev_count = 0; err = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, 0, 1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 1);
    acta_db_model_revision_list_free(revs, rev_count);

    /* Pagination: limit=1 offset=2 → only rev 3 */
    rev_count = 0; err = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, 2, 1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 3);
    TEST_ASSERT(revs[0]->deleted_at != NULL);
    acta_db_model_revision_list_free(revs, rev_count);

    /* Pagination: limit=2 offset=1 → rev 2, rev 3 */
    rev_count = 0; err = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, 1, 2, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 2);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 2);
    TEST_ASSERT_EQ_INT(revs[1]->revision, 3);
    acta_db_model_revision_list_free(revs, rev_count);

    /* Pagination: offset beyond last row → empty, no error */
    rev_count = -1; err = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, 100, 10, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 0);
    TEST_ASSERT(revs == NULL);
    acta_db_model_revision_list_free(revs, rev_count);

    /* get_live returns NULL after soft-delete */
    err = 0;
    TEST_ASSERT_NULL(acta_db_model_get_live(db, model_id, &err));

    test_db_teardown(db, path);
}

/* ---------- 11.2: Full lifecycle — skill ---------- */
static void test_integration_skill_lifecycle(void) {
    const char *path = "test/acta_test_int_skill_lc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int folder_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_folder_create(db, "TestSkillFolder", 0, &folder_id), 0);
    TEST_ASSERT(folder_id > 0);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id       = folder_id;
    s.name            = "TestSkill";
    s.description     = "A test skill";
    s.prompt_template = "You are a helpful assistant.";
    s.output_schema   = NULL;

    int skill_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_skill_create(db, &s, &skill_id), 0);
    TEST_ASSERT(skill_id > 0);

    /* Revision 1 */
    int err = 0, rev_count = 0;
    skill_revision_t **revs = acta_db_skill_revision_list_by_skill(db, skill_id, 0, -1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 1);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Update → revision 2 */
    skill_t s2;
    memset(&s2, 0, sizeof(s2));
    s2.id             = skill_id;
    s2.folder_id      = folder_id;
    s2.name           = "TestSkill";
    s2.description    = "Updated";
    s2.prompt_template = "You are a very helpful assistant.";
    s2.output_schema  = NULL;

    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s2), 0);

    err = 0; rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, 0, -1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 2);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Soft-delete → revision 3 */
    TEST_ASSERT_EQ_INT(acta_db_skill_soft_delete(db, skill_id), 0);

    err = 0; rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, 0, -1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 3);
    TEST_ASSERT(revs[2]->deleted_at != NULL);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Pagination: limit=1 offset=0 → only rev 1 */
    err = 0; rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, 0, 1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 1);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Pagination: limit=1 offset=2 → only rev 3 */
    err = 0; rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, 2, 1, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 3);
    TEST_ASSERT(revs[0]->deleted_at != NULL);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Pagination: limit=2 offset=1 → rev 2, rev 3 */
    err = 0; rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, 1, 2, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 2);
    TEST_ASSERT_EQ_INT(revs[0]->revision, 2);
    TEST_ASSERT_EQ_INT(revs[1]->revision, 3);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Pagination: offset beyond last row → empty, no error */
    err = 0; rev_count = -1;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, 100, 10, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 0);
    TEST_ASSERT(revs == NULL);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* get_live returns NULL after soft-delete */
    TEST_ASSERT_NULL(acta_db_skill_get_live(db, skill_id, NULL));

    test_db_teardown(db, path);
}

/* ---------- 11.3: Execution references revisions ---------- */
static void test_integration_execution_references_revisions(void) {
    const char *path = "test/acta_test_int_exec_ref.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    test_set_t ts = make_test_set(db, "ExecRef");

    int exec_id = make_execution(db, ts.context_id, ts.srev->id, ts.mrev->id, "Say hello");
    TEST_ASSERT(exec_id > 0);

    int err = 0;
    execution_t *got = acta_db_execution_get(db, exec_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->context_id,        ts.context_id);
    TEST_ASSERT_EQ_INT(got->skill_revision_id, ts.srev->id);
    TEST_ASSERT_EQ_INT(got->model_revision_id, ts.mrev->id);
    acta_db_execution_free(got);

    free_test_set(&ts);
    test_db_teardown(db, path);
}

/* ---------- 11.4: Execution with updated revisions ---------- */
static void test_integration_execution_updated_revisions(void) {
    const char *path = "test/acta_test_int_exec_upd_rev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id   = make_context(db, "test", "data", "hash1");
    int model_id = make_model(db, "UpdRevModel");
    int skill_id = make_skill(db, "UpdRevSkill");

    /* Update model → rev 2 */
    model_t m_upd;
    memset(&m_upd, 0, sizeof(m_upd));
    m_upd.id             = model_id;
    m_upd.folder_id      = 0;
    m_upd.name           = "UpdRevModel";
    m_upd.description    = "updated";
    m_upd.backend        = "openai";
    m_upd.base_url       = NULL;
    m_upd.model_identifier = "gpt-4";
    TEST_ASSERT_EQ_INT(acta_db_model_update(db, &m_upd), 0);

    /* Update skill → rev 2 */
    skill_t s_upd;
    memset(&s_upd, 0, sizeof(s_upd));
    s_upd.id             = skill_id;
    s_upd.folder_id      = 0;
    s_upd.name           = "UpdRevSkill";
    s_upd.prompt_template = "v2";
    TEST_ASSERT_EQ_INT(acta_db_skill_update(db, &s_upd), 0);

    int err = 0;
    model_revision_t *mrev2 = acta_db_model_revision_get_by_model_and_rev(db, model_id, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(mrev2);

    skill_revision_t  *srev2 = get_skill_rev(db, skill_id, 2);

    int exec_id = make_execution(db, ctx_id, srev2->id, mrev2->id, "test");
    TEST_ASSERT(exec_id > 0);

    acta_db_model_revision_free(mrev2);
    acta_db_skill_revision_free(srev2);
    test_db_teardown(db, path);
}

/* ---------- 11.5: Context reuse ---------- */
static void test_integration_context_reuse(void) {
    const char *path = "test/acta_test_int_ctx_reuse.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id   = make_context(db, "shared", "shared content", "shared_hash");
    int model_id = make_model(db, "ReuseModel");
    int skill_id = make_skill(db, "ReuseSkill");

    model_revision_t *mrev = get_model_rev(db, model_id, 1);
    skill_revision_t  *srev = get_skill_rev(db, skill_id, 1);

    /* Three executions share the same context */
    int exec_ids[3];
    for (int i = 0; i < 3; i++) {
        exec_ids[i] = make_execution(db, ctx_id, srev->id, mrev->id, "exec");
    }

    int err = 0;
    for (int i = 0; i < 3; i++) {
        err = 0;
        execution_t *got = acta_db_execution_get(db, exec_ids[i], &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        TEST_ASSERT_NOT_NULL(got);
        TEST_ASSERT_EQ_INT(got->context_id, ctx_id);
        acta_db_execution_free(got);
    }

    /* Query by hash returns exactly one context */
    context_query_t cq;
    cq.type = NULL;
    cq.hash = "shared_hash";

    int ctx_count = 0, ctx_err = 0;
    context_t **ctx_list = acta_db_context_query(db, &cq, 0, 0, &ctx_count, &ctx_err);
    TEST_ASSERT_EQ_INT(ctx_err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(ctx_count, 1);
    acta_db_context_list_free(ctx_list, ctx_count);

    acta_db_model_revision_free(mrev);
    acta_db_skill_revision_free(srev);
    test_db_teardown(db, path);
}

/* ---------- 11.6: Nested executions ---------- */
static void test_integration_nested_executions(void) {
    const char *path = "test/acta_test_int_nested.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    test_set_t ts = make_test_set(db, "Nested");

    int root_id = make_execution(db, ts.context_id, ts.srev->id, ts.mrev->id, "root");

    /* child → parent = root */
    execution_t child;
    memset(&child, 0, sizeof(child));
    child.context_id        = ts.context_id;
    child.skill_revision_id = ts.srev->id;
    child.model_revision_id = ts.mrev->id;
    child.prompt            = "child";
    child.status            = ACTA_EXEC_STATUS_PENDING;
    child.parent_execution_id = root_id;
    int child_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &child, &child_id), 0);

    /* grandchild → parent = child */
    execution_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.context_id        = ts.context_id;
    gc.skill_revision_id = ts.srev->id;
    gc.model_revision_id = ts.mrev->id;
    gc.prompt            = "grandchild";
    gc.status            = ACTA_EXEC_STATUS_PENDING;
    gc.parent_execution_id = child_id;
    int gc_id = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_create(db, &gc, &gc_id), 0);

    /* Verify tree via unified query */
    int err = 0;
    int kcount = 0;

    err = 0; kcount = 0;
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.parent_execution_id = root_id;
    execution_t **kids = acta_db_execution_query(db, &q, 0, -1, &kcount, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(kids);
    TEST_ASSERT_EQ_INT(kcount, 1);
    TEST_ASSERT(kids[0]->id == child_id);
    acta_db_execution_list_free(kids, kcount);

    err = 0; kcount = 0;
    q.parent_execution_id = child_id;
    kids = acta_db_execution_query(db, &q, 0, -1, &kcount, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(kids);
    TEST_ASSERT_EQ_INT(kcount, 1);
    TEST_ASSERT(kids[0]->id == gc_id);
    acta_db_execution_list_free(kids, kcount);

    err = 0; kcount = 0;
    q.parent_execution_id = gc_id;
    kids = acta_db_execution_query(db, &q, 0, -1, &kcount, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT(kids == NULL || kcount == 0);
    acta_db_execution_list_free(kids, kcount);

    free_test_set(&ts);
    test_db_teardown(db, path);
}

/* ---------- 11.7: WAL mode ---------- */
static void test_integration_wal_mode(void) {
    const char *path = "test/acta_test_int_wal.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    TEST_ASSERT_EQ_INT(acta_db_exec(db, "PRAGMA journal_mode=WAL;"), 0);

    char wal_path[512], shm_path[512];
    snprintf(wal_path, sizeof(wal_path), "%s-wal", path);
    snprintf(shm_path, sizeof(shm_path), "%s-shm", path);

    FILE *f = fopen(wal_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);

    test_db_teardown(db, path);
    remove(wal_path);
    remove(shm_path);
}

/* ---------- 11.8: Foreign key enforcement ---------- */
static void test_integration_fk_enforcement(void) {
    const char *path = "test/acta_test_int_fk.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = make_model(db, "FKModel");
    int skill_id = make_skill(db, "FKSkill");
    model_revision_t *mrev = get_model_rev(db, model_id, 1);
    skill_revision_t  *srev = get_skill_rev(db, skill_id, 1);

    /* Bad context_id */
    {
        execution_t e;
        memset(&e, 0, sizeof(e));
        e.context_id        = 999999;
        e.skill_revision_id = srev->id;
        e.model_revision_id = mrev->id;
        e.prompt            = "test";
        e.status            = ACTA_EXEC_STATUS_PENDING;
        int id = 0;
        int rc = acta_db_execution_create(db, &e, &id);
        TEST_ASSERT(rc < 0);
    }

    int ctx_id = make_context(db, "fk_test", "x", "fk_hash");

    /* Bad skill_revision_id */
    {
        execution_t e;
        memset(&e, 0, sizeof(e));
        e.context_id        = ctx_id;
        e.skill_revision_id = 999999;
        e.model_revision_id = mrev->id;
        e.prompt            = "test";
        e.status            = ACTA_EXEC_STATUS_PENDING;
        int id = 0;
        TEST_ASSERT(acta_db_execution_create(db, &e, &id) < 0);
    }

    /* Bad model_revision_id */
    {
        execution_t e;
        memset(&e, 0, sizeof(e));
        e.context_id        = ctx_id;
        e.skill_revision_id = srev->id;
        e.model_revision_id = 999999;
        e.prompt            = "test";
        e.status            = ACTA_EXEC_STATUS_PENDING;
        int id = 0;
        TEST_ASSERT(acta_db_execution_create(db, &e, &id) < 0);
    }

    acta_db_model_revision_free(mrev);
    acta_db_skill_revision_free(srev);
    test_db_teardown(db, path);
}

/* ---------- 11.9: Concurrency — two connections ---------- */
static void test_integration_concurrency_two_connections(void) {
    const char *path = "test/acta_test_int_conc.db";
    remove(path);

    db_t *db1 = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db1);

    int errorno = 0;
    db_t *db2 = acta_db_open(path, &errorno);
    TEST_ASSERT_NOT_NULL(db2);
    TEST_ASSERT_EQ_INT(errorno, ACTA_DB_OK);

    int id1 = make_context(db1, "conn1", "from_conn1", "conc_hash_1");
    int id2 = make_context(db2, "conn2", "from_conn2", "conc_hash_2");

    /* Both rows visible from both connections */
    int err = 0;
    context_t *g1       = acta_db_context_get(db1, id1, &err);
    context_t *g1_from2 = acta_db_context_get(db2, id1, &err);
    context_t *g2       = acta_db_context_get(db2, id2, &err);
    context_t *g2_from1 = acta_db_context_get(db1, id2, &err);

    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(g1);
    TEST_ASSERT_NOT_NULL(g1_from2);
    TEST_ASSERT_NOT_NULL(g2);
    TEST_ASSERT_NOT_NULL(g2_from1);
    TEST_ASSERT_EQ_STR(g1->content, "from_conn1");
    TEST_ASSERT_EQ_STR(g2->content, "from_conn2");

    acta_db_context_free(g1);
    acta_db_context_free(g1_from2);
    acta_db_context_free(g2);
    acta_db_context_free(g2_from1);

    acta_db_close(db2);
    test_db_teardown(db1, path);
}

/* ---------- 11.10: Large content ---------- */
static void test_integration_large_content(void) {
    const char *path = "test/acta_test_int_large.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    const size_t size = 1024 * 1024;
    char *big = malloc(size + 1);
    TEST_ASSERT_NOT_NULL(big);
    for (size_t i = 0; i < size; i++)
        big[i] = (char)('a' + (i % 26));
    big[size] = '\0';

    int ctx_id = make_context(db, "large", big, "large_hash");

    int err = 0;
    context_t *got = acta_db_context_get(db, ctx_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT(got->content != NULL);
    TEST_ASSERT_EQ_INT((int)strlen(got->content), (int)size);
    TEST_ASSERT(strcmp(got->content, big) == 0);

    acta_db_context_free(got);
    free(big);
    test_db_teardown(db, path);
}

/* ---------- 11.11: Unicode content ---------- */
static void test_integration_unicode_content(void) {
    const char *path = "test/acta_test_int_unicode.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    const char *utf8 = "caf\xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xf0\x9f\x98\x80";
    int ctx_id = make_context(db, "unicode", utf8, "unicode_hash");

    int err = 0;
    context_t *got = acta_db_context_get(db, ctx_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT(strcmp(got->content, utf8) == 0);

    acta_db_context_free(got);
    test_db_teardown(db, path);
}

/* ---------- 11.12: Memory leak sweep ---------- */
static void test_integration_memory_leak_sweep(void) {
    const char *path = "test/acta_test_int_leak.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* model folder */
    int fid = 0;
    acta_db_model_folder_create(db, "LeakFolder", 0, &fid);
    { int ferr = 0; model_folder_t *mf = acta_db_model_folder_get(db, fid, &ferr); acta_db_model_folder_free(mf); }

    /* model */
    int mid = make_model(db, "LeakModel");
    { int merr = 0; model_t *mg = acta_db_model_get(db, mid, &merr); acta_db_model_free(mg); }

    /* model revision */
    { model_revision_t *mr = get_model_rev(db, mid, 1); acta_db_model_revision_free(mr); }

    /* skill */
    int sid = make_skill(db, "LeakSkill");
    { skill_t *sg = acta_db_skill_get(db, sid, NULL); acta_db_skill_free(sg); }

    /* skill revision */
    { skill_revision_t *sr = get_skill_rev(db, sid, 1); acta_db_skill_revision_free(sr); }

    /* context */
    int cid = make_context(db, "leak", "c", "leak_hash");
    { context_t *cg = acta_db_context_get(db, cid, NULL); acta_db_context_free(cg); }

    /* execution */
    model_revision_t *mrev = get_model_rev(db, mid, 1);
    skill_revision_t  *srev = get_skill_rev(db, sid, 1);
    int eid = make_execution(db, cid, srev->id, mrev->id, "p");
    {
        int err = 0;
        execution_t *eg = acta_db_execution_get(db, eid, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        acta_db_execution_free(eg);
    }

    /* execution log  (pagination API: offset=0, limit=-1 → all rows) */
    {
        execution_log_t log;
        memset(&log, 0, sizeof(log));
        log.execution_id = eid;
        log.level        = ACTA_LOG_LEVEL_INFO;
        log.event        = "test_event";
        log.message      = "msg";
        log.metadata     = NULL;

        int log_id = 0;
        TEST_ASSERT_EQ_INT(acta_db_execution_log_create(db, &log, &log_id), 0);

        int log_count = 0, log_err = 0;
        execution_log_t **logs = acta_db_execution_log_list_by_execution(db, eid, NULL, 0, -1, &log_count, &log_err);
        TEST_ASSERT_EQ_INT(log_err, ACTA_DB_OK);
        acta_db_execution_log_list_free(logs, log_count);
    }

    /* list-and-free: model revisions (offset=0, limit=-1 → all) */
    { int n = 0, err = 0; model_revision_t **r = acta_db_model_revision_list_by_model(db, mid, 0, -1, &n, &err);
      acta_db_model_revision_list_free(r, n); }

    /* list-and-free: skill revisions (offset=0, limit=-1 → all) */
    { int n = 0, err = 0; skill_revision_t **r = acta_db_skill_revision_list_by_skill(db, sid, 0, -1, &n, &err);
      acta_db_skill_revision_list_free(r, n); }

    /* list-and-free: model folders  (offset=0, limit=-1 → all) */
    { int n = 0, err = 0; model_folder_t **f = acta_db_model_folder_list_all(db, 0, -1, &n, &err);
      acta_db_model_folder_list_free(f, n); }

    /* list-and-free: skill folders  (offset=0, limit=-1 → all) */
    { int n = 0, err = 0; skill_folder_t **f = acta_db_skill_folder_list_all(db, 0, -1, &n, &err);
      acta_db_skill_folder_list_free(f, n); }

    /* list-and-free: models  (offset=0, limit=-1 → all) */
    { int n = 0, err = 0; model_t **m = acta_db_model_list_all(db, 0, -1, &n, &err);
      acta_db_model_list_free(m, n); }

    /* list-and-free: skills  (offset=0, limit=-1 → all) */
    { int n = 0, err = 0; skill_t **s = acta_db_skill_list_all(db, 0, -1, &n, &err);
      acta_db_skill_list_free(s, n); }

    /* list-and-free: executions (unified query, no filter) */
    {
        int n = 0, err = 0;
        execution_t **e = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 0, -1, &n, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        acta_db_execution_list_free(e, n);
    }

    acta_db_model_revision_free(mrev);
    acta_db_skill_revision_free(srev);

    TEST_ASSERT(1); /* reached here → no crash; valgrind/ASan check leaks */
    test_db_teardown(db, path);
}

/* ---------- 11.13: NULL safety ---------- */
static void test_integration_null_safety(void) {
    /* context */
    TEST_ASSERT_NULL(acta_db_context_get(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_context_query(NULL, NULL, 0, 0, NULL, NULL));
    acta_db_context_free(NULL);
    acta_db_context_list_free(NULL, 0);

    /* model */
    TEST_ASSERT_NULL(acta_db_model_get(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_model_get_live(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_model_list_all(NULL, 0, -1, NULL, NULL));
    acta_db_model_free(NULL);
    acta_db_model_list_free(NULL, 0);

    /* model folder */
    TEST_ASSERT_NULL(acta_db_model_folder_get(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_model_folder_list_all(NULL, 0, -1, NULL, NULL));
    acta_db_model_folder_free(NULL);
    acta_db_model_folder_list_free(NULL, 0);

    /* model revision — verify err is negative on NULL db */
    {
        int err = 0;
        TEST_ASSERT_NULL(acta_db_model_revision_get(NULL, 1, &err));
        TEST_ASSERT(err < 0);
    }
    {
        int err = 0;
        TEST_ASSERT_NULL(acta_db_model_revision_get_by_model_and_rev(NULL, 1, 1, &err));
        TEST_ASSERT(err < 0);
    }
    {
        int err = 0;
        TEST_ASSERT_NULL(acta_db_model_revision_get_latest(NULL, 1, &err));
        TEST_ASSERT(err < 0);
    }
    {
        int cnt = 0, err = 0;
        model_revision_t **items = acta_db_model_revision_list_by_model(NULL, 1, 0, -1, &cnt, &err);
        TEST_ASSERT(err < 0);
        TEST_ASSERT_NULL(items);
    }
    acta_db_model_revision_free(NULL);
    acta_db_model_revision_list_free(NULL, 0);

    /* skill */
    TEST_ASSERT_NULL(acta_db_skill_get(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_skill_get_live(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_skill_list_all(NULL, 0, -1, NULL, NULL));
    acta_db_skill_free(NULL);
    acta_db_skill_list_free(NULL, 0);

    /* skill folder */
    TEST_ASSERT_NULL(acta_db_skill_folder_get(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_skill_folder_list_all(NULL, 0, -1, NULL, NULL));
    acta_db_skill_folder_free(NULL);
    acta_db_skill_folder_list_free(NULL, 0);

    /* skill revision */
    TEST_ASSERT_NULL(acta_db_skill_revision_get(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_skill_revision_get_by_skill_and_rev(NULL, 1, 1, NULL));
    TEST_ASSERT_NULL(acta_db_skill_revision_get_latest(NULL, 1, NULL));
    TEST_ASSERT_NULL(acta_db_skill_revision_list_by_skill(NULL, 1, 0, -1, NULL, NULL));
    acta_db_skill_revision_free(NULL);
    acta_db_skill_revision_list_free(NULL, 0);

    /* execution */
    {
        int err = 0;
        TEST_ASSERT_NULL(acta_db_execution_get(NULL, 1, &err));
        TEST_ASSERT(err < 0);
    }
    {
        int err = 0;
        TEST_ASSERT_NULL(acta_db_execution_query(NULL, NULL, 0, -1, NULL, &err));
        TEST_ASSERT(err < 0);
    }
    {
        int err = 0;
        int rc = acta_db_execution_count(NULL, NULL, &err);
        TEST_ASSERT(rc < 0);
        TEST_ASSERT(err < 0);
    }
    acta_db_execution_free(NULL);
    acta_db_execution_list_free(NULL, 0);

    /* execution log  (pagination API: offset=0, limit=-1) */
    {
        int cnt = 0, err = 0;
        execution_log_t **items = acta_db_execution_log_list_by_execution(NULL, 1, NULL, 0, -1, &cnt, &err);
        TEST_ASSERT(err < 0);
        TEST_ASSERT_NULL(items);
    }
    acta_db_execution_log_free(NULL);
    acta_db_execution_log_list_free(NULL, 0);

    /* db */
    acta_db_close(NULL);
    TEST_ASSERT(1);
}

/* ---------- 11.14: Repeated open/close ---------- */
static void test_integration_repeated_open_close(void) {
    const char *path = "test/acta_test_int_repeat.db";
    remove(path);

    for (int i = 0; i < 100; i++) {
        db_t *db = test_db_open(path);
        TEST_ASSERT_NOT_NULL(db);
        acta_db_close(db);
    }

    remove(path);
    remove("test/acta_test_int_repeat.db-wal");
    remove("test/acta_test_int_repeat.db-shm");
}

/* ---------- 11.15: Execution pagination ---------- */
static void test_integration_execution_pagination(void) {
    const char *path = "test/acta_test_int_exec_pager.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    test_set_t ts = make_test_set(db, "ExecPag");

    /* Create 5 pending executions in the same context */
    int exec_ids[5];
    for (int i = 0; i < 5; i++)
        exec_ids[i] = make_execution(db, ts.context_id, ts.srev->id, ts.mrev->id, "page_test");

    int err = 0, count = 0;

    /* All rows: no filter */
    count = 0; err = 0;
    execution_t **items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 5);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_execution_list_free(items, count);

    /* limit=2, offset=0 → first two */
    count = 0; err = 0;
    items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_list_free(items, count);

    /* limit=2, offset=2 → middle two */
    count = 0; err = 0;
    items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_list_free(items, count);

    /* limit=1, offset=4 → last one */
    count = 0; err = 0;
    items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 4, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_execution_list_free(items, count);

    /* offset beyond last → empty */
    count = -1; err = 0;
    items = acta_db_execution_query(db, &ACTA_EXEC_QUERY_ANY, 100, 10, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    acta_db_execution_list_free(items, count);

    /* by context: limit=3 */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.context_id = ts.context_id;
    count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 0, 3, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_execution_list_free(items, count);

    /* by context: offset=3, limit=10 → remaining 2 */
    count = 0; err = 0;
    items = acta_db_execution_query(db, &q, 3, 10, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_list_free(items, count);

    free_test_set(&ts);
    test_db_teardown(db, path);
}

/* ================================================================
 *  Runner
 * ================================================================ */
void run_integration_tests(void) {
    fprintf(stderr, "\n=== Integration / Cross-Cutting Tests ===\n");
    test_integration_model_lifecycle();
    test_integration_skill_lifecycle();
    test_integration_execution_references_revisions();
    test_integration_execution_updated_revisions();
    test_integration_context_reuse();
    test_integration_nested_executions();
    test_integration_wal_mode();
    test_integration_fk_enforcement();
    test_integration_concurrency_two_connections();
    test_integration_large_content();
    test_integration_unicode_content();
    test_integration_memory_leak_sweep();
    test_integration_null_safety();
    test_integration_repeated_open_close();
    test_integration_execution_pagination();
}
