/* test_integration.c */
#include "test_common.h"

/* ---------- 11.1: Full lifecycle — model ---------- */
static void test_integration_model_lifecycle(void) {
    const char *path = "test/acta_test_int_model_lc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create a folder */
    int folder_id = 0;
    int rc = acta_db_model_folder_create(db, "TestFolder", 0, &folder_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(folder_id > 0);

    /* Create a model in that folder */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = folder_id;
    m.name = "TestModel";
    m.description = "A test model";
    m.backend = "openai";
    m.base_url = "https://api.openai.com";
    m.model_identifier = "gpt-4";
    m.configuration = NULL;

    int model_id = 0;
    rc = acta_db_model_create(db, &m, &model_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(model_id > 0);

    /* Verify initial revision exists */
    int rev_count = 0;
    model_revision_t *revs = acta_db_model_revision_list_by_model(db, model_id, &rev_count, NULL);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT(revs != NULL);
    TEST_ASSERT_EQ_INT(revs[0].revision, 1);
    acta_db_model_revision_list_free(revs, rev_count);

    /* Update the model — should create revision 2 */
    model_t m2;
    memset(&m2, 0, sizeof(m2));
    m2.id = model_id;
    m2.folder_id = folder_id;
    m2.name = "TestModel";
    m2.description = "Updated description";
    m2.backend = "openai";
    m2.base_url = "https://api.openai.com";
    m2.model_identifier = "gpt-4";
    m2.configuration = NULL;

    rc = acta_db_model_update(db, &m2);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Verify revision 2 was created */
    rev_count = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, &rev_count, NULL);
    TEST_ASSERT_EQ_INT(rev_count, 2);
    acta_db_model_revision_list_free(revs, rev_count);

    /* Soft-delete the model */
    rc = acta_db_model_soft_delete(db, model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Verify a deleted revision was created (revision 3) */
    rev_count = 0;
    revs = acta_db_model_revision_list_by_model(db, model_id, &rev_count, NULL);
    TEST_ASSERT_EQ_INT(rev_count, 3);
    /* The last revision should have deleted_at set */
    TEST_ASSERT(revs[2].deleted_at != NULL);
    acta_db_model_revision_list_free(revs, rev_count);

    /* get_live should return NULL for soft-deleted model */
    model_t *live = acta_db_model_get_live(db, model_id);
    TEST_ASSERT_NULL(live);

    test_db_teardown(db, path);
}

/* ---------- 11.2: Full lifecycle — skill ---------- */
static void test_integration_skill_lifecycle(void) {
    const char *path = "test/acta_test_int_skill_lc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;

    /* Create a skill folder */
    int folder_id = 0;
    int rc = acta_db_skill_folder_create(db, "TestSkillFolder", 0, &folder_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(folder_id > 0);

    /* Create a skill */
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = folder_id;
    s.name = "TestSkill";
    s.description = "A test skill";
    s.prompt_template = "You are a helpful assistant.";
    s.output_schema = NULL;

    int skill_id = 0;
    rc = acta_db_skill_create(db, &s, &skill_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(skill_id > 0);

    /* Verify initial revision */
    int rev_count = 0;
    skill_revision_t *revs = acta_db_skill_revision_list_by_skill(db, skill_id, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 1);
    TEST_ASSERT(revs != NULL);
    TEST_ASSERT_EQ_INT(revs[0].revision, 1);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Update the skill — should create revision 2 */
    skill_t s2;
    memset(&s2, 0, sizeof(s2));
    s2.id = skill_id;
    s2.folder_id = folder_id;
    s2.name = "TestSkill";
    s2.description = "Updated";
    s2.prompt_template = "You are a very helpful assistant.";
    s2.output_schema = NULL;

    rc = acta_db_skill_update(db, &s2);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 2);
    acta_db_skill_revision_list_free(revs, rev_count);

    /* Soft-delete */
    rc = acta_db_skill_soft_delete(db, skill_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    rev_count = 0;
    revs = acta_db_skill_revision_list_by_skill(db, skill_id, &rev_count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev_count, 3);
    TEST_ASSERT(revs != NULL);
    TEST_ASSERT(revs[2].deleted_at != NULL);
    acta_db_skill_revision_list_free(revs, rev_count);

    skill_t *live = acta_db_skill_get_live(db, skill_id);
    TEST_ASSERT_NULL(live);

    test_db_teardown(db, path);
}


/* ---------- 11.3: Execution references revisions ---------- */
static void test_integration_execution_references_revisions(void) {
    const char *path = "test/acta_test_int_exec_ref.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;

    /* Create a context */
    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "conversation";
    ctx.content = "Hello";
    ctx.content_hash = "abc123";
    ctx.metadata = NULL;

    int context_id = 0;
    int rc = acta_db_context_create(db, &ctx, &context_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Create a model (auto-creates revision 1) */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = 0;
    m.name = "ExecRefModel";
    m.description = NULL;
    m.backend = "openai";
    m.base_url = "https://api.openai.com";
    m.model_identifier = "gpt-4";
    m.configuration = NULL;

    int model_id = 0;
    rc = acta_db_model_create(db, &m, &model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    model_revision_t *mrev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(mrev);

    /* Create a skill (auto-creates revision 1) */
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = 0;
    s.name = "ExecRefSkill";
    s.description = NULL;
    s.prompt_template = "Test prompt";
    s.output_schema = NULL;

    int skill_id = 0;
    rc = acta_db_skill_create(db, &s, &skill_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    skill_revision_t *srev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(srev);

    /* Create execution referencing revision 1 of both */
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id = context_id;
    e.skill_revision_id = srev->id;
    e.model_revision_id = mrev->id;
    e.prompt = "Say hello";
    e.status = "pending";

    int exec_id = 0;
    rc = acta_db_execution_create(db, &e, &exec_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(exec_id > 0);

    execution_t *got = acta_db_execution_get(db, exec_id);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->context_id, context_id);
    TEST_ASSERT_EQ_INT(got->skill_revision_id, srev->id);
    TEST_ASSERT_EQ_INT(got->model_revision_id, mrev->id);
    acta_db_execution_free(got);

    acta_db_model_revision_free(mrev);
    acta_db_skill_revision_free(srev);
    test_db_teardown(db, path);
}


/* ---------- 11.4: Execution with updated revisions ---------- */
static void test_integration_execution_updated_revisions(void) {
    const char *path = "test/acta_test_int_exec_upd_rev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;

    /* Context */
    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "test";
    ctx.content = "data";
    ctx.content_hash = "hash1";
    ctx.metadata = NULL;

    int context_id = 0;
    int rc = acta_db_context_create(db, &ctx, &context_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Model — create, then update to get revision 2 */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = 0;
    m.name = "UpdRevModel";
    m.description = NULL;
    m.backend = "openai";
    m.base_url = "https://api.openai.com";
    m.model_identifier = "gpt-4";
    m.configuration = NULL;

    int model_id = 0;
    rc = acta_db_model_create(db, &m, &model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    model_t m_upd;
    memset(&m_upd, 0, sizeof(m_upd));
    m_upd.id = model_id;
    m_upd.folder_id = 0;
    m_upd.name = "UpdRevModel";
    m_upd.description = "updated";
    m_upd.backend = "openai";
    m_upd.base_url = "https://api.openai.com";
    m_upd.model_identifier = "gpt-4";
    m_upd.configuration = NULL;
    rc = acta_db_model_update(db, &m_upd);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    model_revision_t *mrev2 = acta_db_model_revision_get_by_model_and_rev(db, model_id, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(mrev2);

    /* Skill — create, then update to get revision 2 */
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = 0;
    s.name = "UpdRevSkill";
    s.description = NULL;
    s.prompt_template = "v1";
    s.output_schema = NULL;

    int skill_id = 0;
    rc = acta_db_skill_create(db, &s, &skill_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    skill_t s_upd;
    memset(&s_upd, 0, sizeof(s_upd));
    s_upd.id = skill_id;
    s_upd.folder_id = 0;
    s_upd.name = "UpdRevSkill";
    s_upd.description = NULL;
    s_upd.prompt_template = "v2";
    s_upd.output_schema = NULL;
    rc = acta_db_skill_update(db, &s_upd);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    skill_revision_t *srev2 = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(srev2);

    /* Create execution referencing revision 2 of both */
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id = context_id;
    e.skill_revision_id = srev2->id;
    e.model_revision_id = mrev2->id;
    e.prompt = "test";
    e.status = "pending";

    int exec_id = 0;
    rc = acta_db_execution_create(db, &e, &exec_id);
    TEST_ASSERT_EQ_INT(rc, 0);
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

    int err = 0;

    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "shared";
    ctx.content = "shared content";
    ctx.content_hash = "shared_hash";
    ctx.metadata = NULL;

    int context_id = 0;
    int rc = acta_db_context_create(db, &ctx, &context_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Create a model */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = 0;
    m.name = "ReuseModel";
    m.description = NULL;
    m.backend = "openai";
    m.base_url = NULL;
    m.model_identifier = "gpt-4";
    m.configuration = NULL;

    int model_id = 0;
    rc = acta_db_model_create(db, &m, &model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    model_revision_t *mrev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(mrev);

    /* Create a skill */
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = 0;
    s.name = "ReuseSkill";
    s.description = NULL;
    s.prompt_template = "prompt";
    s.output_schema = NULL;

    int skill_id = 0;
    rc = acta_db_skill_create(db, &s, &skill_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    skill_revision_t *srev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(srev);

    /* Create 3 executions all referencing the same context */
    int exec_ids[3];
    for (int i = 0; i < 3; i++) {
        execution_t e;
        memset(&e, 0, sizeof(e));
        e.context_id = context_id;
        e.skill_revision_id = srev->id;
        e.model_revision_id = mrev->id;
        e.prompt = "exec";
        e.status = "pending";

        rc = acta_db_execution_create(db, &e, &exec_ids[i]);
        TEST_ASSERT_EQ_INT(rc, 0);
    }

    /* Verify all 3 exist and reference the same context */
    for (int i = 0; i < 3; i++) {
        execution_t *got = acta_db_execution_get(db, exec_ids[i]);
        TEST_ASSERT_NOT_NULL(got);
        TEST_ASSERT_EQ_INT(got->context_id, context_id);
        acta_db_execution_free(got);
    }

    /* Verify list_by_hash returns the single context */
    int ctx_count = 0;
    int ctx_err = 0;
    context_t **ctx_list = acta_db_context_list_by_hash(db, "shared_hash", &ctx_count, &ctx_err);
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

    int err = 0;

    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "nested";
    ctx.content = "n";
    ctx.content_hash = "nested_hash";
    ctx.metadata = NULL;

    int context_id = 0;
    int rc = acta_db_context_create(db, &ctx, &context_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = 0;
    m.name = "NestedModel";
    m.description = NULL;
    m.backend = "openai";
    m.base_url = NULL;
    m.model_identifier = "gpt-4";
    m.configuration = NULL;

    int model_id = 0;
    rc = acta_db_model_create(db, &m, &model_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    model_revision_t *mrev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(mrev);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = 0;
    s.name = "NestedSkill";
    s.description = NULL;
    s.prompt_template = "p";
    s.output_schema = NULL;

    int skill_id = 0;
    rc = acta_db_skill_create(db, &s, &skill_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    err = 0;
    skill_revision_t *srev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(srev);

    /* Create root execution */
    execution_t root;
    memset(&root, 0, sizeof(root));
    root.context_id = context_id;
    root.skill_revision_id = srev->id;
    root.model_revision_id = mrev->id;
    root.prompt = "root";
    root.status = "pending";

    int root_id = 0;
    rc = acta_db_execution_create(db, &root, &root_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Create child execution */
    execution_t child;
    memset(&child, 0, sizeof(child));
    child.context_id = context_id;
    child.skill_revision_id = srev->id;
    child.model_revision_id = mrev->id;
    child.prompt = "child";
    child.status = "pending";
    child.parent_execution_id = root_id;

    int child_id = 0;
    rc = acta_db_execution_create(db, &child, &child_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Create grandchild execution */
    execution_t grandchild;
    memset(&grandchild, 0, sizeof(grandchild));
    grandchild.context_id = context_id;
    grandchild.skill_revision_id = srev->id;
    grandchild.model_revision_id = mrev->id;
    grandchild.prompt = "grandchild";
    grandchild.status = "pending";
    grandchild.parent_execution_id = child_id;

    int grandchild_id = 0;
    rc = acta_db_execution_create(db, &grandchild, &grandchild_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Verify tree structure */
    int child_count = 0;
    execution_t *children = acta_db_execution_list_children(db, root_id, &child_count);
    TEST_ASSERT_EQ_INT(child_count, 1);
    TEST_ASSERT(children != NULL);
    TEST_ASSERT_EQ_INT(children[0].id, child_id);
    acta_db_execution_list_free(children, child_count);

    int grandchild_count = 0;
    execution_t *grandchildren = acta_db_execution_list_children(db, child_id, &grandchild_count);
    TEST_ASSERT_EQ_INT(grandchild_count, 1);
    TEST_ASSERT(grandchildren != NULL);
    TEST_ASSERT_EQ_INT(grandchildren[0].id, grandchild_id);
    acta_db_execution_list_free(grandchildren, grandchild_count);

    /* Leaf has no children */
    int leaf_count = 0;
    execution_t *leaf_children = acta_db_execution_list_children(db, grandchild_id, &leaf_count);
    TEST_ASSERT_EQ_INT(leaf_count, 0);
    acta_db_execution_list_free(leaf_children, leaf_count);

    acta_db_model_revision_free(mrev);
    acta_db_skill_revision_free(srev);
    test_db_teardown(db, path);
}


/* ---------- 11.7: WAL mode ---------- */
static void test_integration_wal_mode(void) {
    const char *path = "test/acta_test_int_wal.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Verify WAL mode is active by checking the pragma result */
    int rc = acta_db_exec(db, "PRAGMA journal_mode=WAL;");
    /* PRAGMA journal_mode=WAL returns the new mode; exec should succeed */
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Verify -wal and -shm files exist alongside the main db */
    char wal_path[512];
    char shm_path[512];
    snprintf(wal_path, sizeof(wal_path), "%s-wal", path);
    snprintf(shm_path, sizeof(shm_path), "%s-shm", path);

    /* These files should exist while the DB is open in WAL mode */
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

    /* Verify PRAGMA foreign_keys is ON */
    /* We can't easily read pragma results through acta_db_exec,
       so we test by attempting a FK violation. */

    /* Try to create an execution with a non-existent context_id */
    /* First we need valid skill_revision and model_revision ids */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = 0;
    m.name = "FKModel";
    m.description = NULL;
    m.backend = "openai";
    m.base_url = NULL;
    m.model_identifier = "gpt-4";
    m.configuration = NULL;

    int model_id = 0;
    acta_db_model_create(db, &m, &model_id);

    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = 0;
    s.name = "FKSkill";
    s.description = NULL;
    s.prompt_template = "p";
    s.output_schema = NULL;

    int skill_id = 0;
    acta_db_skill_create(db, &s, &skill_id);

    model_revision_t *mrev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, NULL);
    skill_revision_t *srev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, NULL);

    /* Execution with invalid context_id (999999) */
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id = 999999;
    e.skill_revision_id = srev->id;
    e.model_revision_id = mrev->id;
    e.prompt = "test";
    e.status = "pending";

    int exec_id = 0;
    int rc = acta_db_execution_create(db, &e, &exec_id);
    TEST_ASSERT(rc < 0);  /* FK violation should fail */

    /* Execution with invalid skill_revision_id */
    e.context_id = 0; /* will also fail, but let's isolate */
    /* Create a valid context first */
    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "fk_test";
    ctx.content = "x";
    ctx.content_hash = "fk_hash";
    ctx.metadata = NULL;
    int context_id = 0;
    acta_db_context_create(db, &ctx, &context_id);

    e.context_id = context_id;
    e.skill_revision_id = 999999;
    e.model_revision_id = mrev->id;
    rc = acta_db_execution_create(db, &e, &exec_id);
    TEST_ASSERT(rc < 0);

    /* Execution with invalid model_revision_id */
    e.context_id = context_id;
    e.skill_revision_id = srev->id;
    e.model_revision_id = 999999;
    rc = acta_db_execution_create(db, &e, &exec_id);
    TEST_ASSERT(rc < 0);

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

    /* Both connections should be able to write independently */
    int id1 = 0, id2 = 0;

    context_t ctx1;
    memset(&ctx1, 0, sizeof(ctx1));
    ctx1.type         = "conn1";
    ctx1.content      = "from_conn1";
    ctx1.content_hash = "conc_hash_1";
    ctx1.metadata     = NULL;

    context_t ctx2;
    memset(&ctx2, 0, sizeof(ctx2));
    ctx2.type         = "conn2";
    ctx2.content      = "from_conn2";
    ctx2.content_hash = "conc_hash_2";
    ctx2.metadata     = NULL;

    int rc1 = acta_db_context_create(db1, &ctx1, &id1);
    int rc2 = acta_db_context_create(db2, &ctx2, &id2);

    TEST_ASSERT_EQ_INT(rc1, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rc2, ACTA_DB_OK);

    /* Verify both rows are visible from both connections */
    int get_err = 0;
    context_t *got1          = acta_db_context_get(db1, id1, &get_err);
    context_t *got1_from_db2 = acta_db_context_get(db2, id1, &get_err);
    context_t *got2          = acta_db_context_get(db2, id2, &get_err);
    context_t *got2_from_db1 = acta_db_context_get(db1, id2, &get_err);

    TEST_ASSERT_EQ_INT(get_err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got1);
    TEST_ASSERT_NOT_NULL(got1_from_db2);
    TEST_ASSERT_NOT_NULL(got2);
    TEST_ASSERT_NOT_NULL(got2_from_db1);

    TEST_ASSERT_EQ_STR(got1->content, "from_conn1");
    TEST_ASSERT_EQ_STR(got2->content, "from_conn2");

    acta_db_context_free(got1);
    acta_db_context_free(got1_from_db2);
    acta_db_context_free(got2);
    acta_db_context_free(got2_from_db1);

    acta_db_close(db2);
    test_db_teardown(db1, path);
}


/* ---------- 11.10: Large content ---------- */
static void test_integration_large_content(void) {
    const char *path = "test/acta_test_int_large.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Build a 1MB string */
    size_t size = 1024 * 1024;
    char *big = malloc(size + 1);
    TEST_ASSERT_NOT_NULL(big);
    for (size_t i = 0; i < size; i++) {
        big[i] = (char)('a' + (i % 26));
    }
    big[size] = '\0';

    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "large";
    ctx.content = big;
    ctx.content_hash = "large_hash";
    ctx.metadata = NULL;

    int context_id = 0;
    int rc = acta_db_context_create(db, &ctx, &context_id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(context_id > 0);

    /* Round-trip: get it back and verify content matches */
    int get_err = 0;
    context_t *got = acta_db_context_get(db, context_id, &get_err);
    TEST_ASSERT_EQ_INT(get_err, ACTA_DB_OK);
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

    const char *utf8_content = "caf\xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xf0\x9f\x98\x80";
    /* "café 日本語 😀" */

    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "unicode";
    ctx.content = (char *)utf8_content;
    ctx.content_hash = "unicode_hash";
    ctx.metadata = NULL;

    int context_id = 0;
    int rc = acta_db_context_create(db, &ctx, &context_id);
    TEST_ASSERT_EQ_INT(rc, 0);

    int get_err = 0;
    context_t *got = acta_db_context_get(db, context_id, &get_err);
    TEST_ASSERT_EQ_INT(get_err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT(got->content != NULL);
    TEST_ASSERT(strcmp(got->content, utf8_content) == 0);

    acta_db_context_free(got);
    test_db_teardown(db, path);
}

/* ---------- 11.12: Memory leak sweep ---------- */
static void test_integration_memory_leak_sweep(void) {
    const char *path = "test/acta_test_int_leak.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create and free a model folder */
    int fid = 0;
    acta_db_model_folder_create(db, "LeakFolder", 0, &fid);
    model_folder_t *mf = acta_db_model_folder_get(db, fid);
    acta_db_model_folder_free(mf);

    /* Create and free a model */
    model_t m;
    memset(&m, 0, sizeof(m));
    m.folder_id = 0;
    m.name = "LeakModel";
    m.description = "d";
    m.backend = "openai";
    m.base_url = NULL;
    m.model_identifier = "gpt-4";
    m.configuration = NULL;
    int mid = 0;
    acta_db_model_create(db, &m, &mid);
    model_t *mg = acta_db_model_get(db, mid);
    acta_db_model_free(mg);

    /* Create and free a model revision */
    model_revision_t *mr = acta_db_model_revision_get_by_model_and_rev(db, mid, 1, NULL);
    acta_db_model_revision_free(mr);

    /* Create and free a skill */
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.folder_id = 0;
    s.name = "LeakSkill";
    s.description = NULL;
    s.prompt_template = "t";
    s.output_schema = NULL;
    int sid = 0;
    acta_db_skill_create(db, &s, &sid);
    skill_t *sg = acta_db_skill_get(db, sid);
    acta_db_skill_free(sg);

    /* Create and free a skill revision */
    skill_revision_t *sr = acta_db_skill_revision_get_by_skill_and_rev(db, sid, 1, NULL);
    acta_db_skill_revision_free(sr);

    /* Create and free a context */
    context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = "leak";
    ctx.content = "c";
    ctx.content_hash = "leak_hash";
    ctx.metadata = NULL;
    int cid = 0;
    acta_db_context_create(db, &ctx, &cid);
    context_t *cg = acta_db_context_get(db, cid, NULL);
    acta_db_context_free(cg);

    /* Create and free an execution */
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id = cid;
    /* Need valid revision ids — re-fetch since sr/mr were freed above */
    model_revision_t *mr2 = acta_db_model_revision_get_by_model_and_rev(db, mid, 1, NULL);
    skill_revision_t *sr2 = acta_db_skill_revision_get_by_skill_and_rev(db, sid, 1, NULL);
    e.skill_revision_id = sr2->id;
    e.model_revision_id = mr2->id;
    e.prompt = "p";
    e.status = "pending";
    int eid = 0;
    acta_db_execution_create(db, &e, &eid);
    execution_t *eg = acta_db_execution_get(db, eid);
    acta_db_execution_free(eg);

    /* Create and free an execution log */
    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = eid;
    log.level = "info";
    log.event = "test_event";
    log.message = "msg";
    log.metadata = NULL;
    int log_id = 0;
    acta_db_execution_log_create(db, &log, &log_id);

    int log_count = 0;
    execution_log_t *logs = NULL;
    int log_rc = acta_db_execution_log_list_by_execution(db, eid, &logs, &log_count, NULL);
    TEST_ASSERT_EQ_INT(log_rc, 0);
    acta_db_execution_log_list_free(logs, log_count);

    /* List and free model revisions */
    int mrev_count = 0;
    model_revision_t *mrevs = acta_db_model_revision_list_by_model(db, mid, &mrev_count, NULL);
    acta_db_model_revision_list_free(mrevs, mrev_count);

    /* List and free skill revisions */
    int srev_count = 0;
    skill_revision_t *srevs = acta_db_skill_revision_list_by_skill(db, sid, &srev_count, NULL);
    acta_db_skill_revision_list_free(srevs, srev_count);

    /* List and free model folders */
    int mf_count = 0;
    model_folder_t *mfs = acta_db_model_folder_list_all(db, &mf_count);
    acta_db_model_folder_list_free(mfs, mf_count);

    /* List and free skill folders */
    int sf_count = 0;
    skill_folder_t *sfs = acta_db_skill_folder_list_all(db, &sf_count);
    acta_db_skill_folder_list_free(sfs, sf_count);

    /* List and free models */
    int m_count = 0;
    model_t *ms = acta_db_model_list_all(db, &m_count);
    acta_db_model_list_free(ms, m_count);

    /* List and free skills */
    int s_count = 0;
    skill_t *ss = acta_db_skill_list_all(db, &s_count);
    acta_db_skill_list_free(ss, s_count);

    /* List and free executions by status */
    int e_count = 0;
    execution_t *es = acta_db_execution_list_by_status(db, "pending", &e_count);
    acta_db_execution_list_free(es, e_count);

    acta_db_model_revision_free(mr2);
    acta_db_skill_revision_free(sr2);

    /* If running under valgrind or ASan, this test passing means no leaks.
       We just assert we got here without crashing. */
    TEST_ASSERT(1);

    test_db_teardown(db, path);
}

/* ---------- 11.13: NULL safety — all functions ---------- */
static void test_integration_null_safety(void) {
    /* Test that passing NULL db to any function doesn't crash.
       We don't need an open DB for this. */

    /* context */
    context_t *c = acta_db_context_get(NULL, 1, NULL);
    TEST_ASSERT_NULL(c);

    context_t **cl = acta_db_context_list_by_hash(NULL, "hash", (int *)NULL, NULL);
    TEST_ASSERT_NULL(cl);

    acta_db_context_free(NULL);
    acta_db_context_list_free(NULL, 0);

    /* model */
    model_t *m = acta_db_model_get(NULL, 1);
    TEST_ASSERT_NULL(m);

    m = acta_db_model_get_live(NULL, 1);
    TEST_ASSERT_NULL(m);

    model_t *ml = acta_db_model_list_all(NULL, (int *)NULL);
    TEST_ASSERT_NULL(ml);

    acta_db_model_free(NULL);
    acta_db_model_list_free(NULL, 0);

    /* model folder */
    model_folder_t *mf = acta_db_model_folder_get(NULL, 1);
    TEST_ASSERT_NULL(mf);

    model_folder_t *mfl = acta_db_model_folder_list_all(NULL, (int *)NULL);
    TEST_ASSERT_NULL(mfl);

    acta_db_model_folder_free(NULL);
    acta_db_model_folder_list_free(NULL, 0);

    /* model revision */
    model_revision_t *mr = acta_db_model_revision_get(NULL, 1, NULL);
    TEST_ASSERT_NULL(mr);

    mr = acta_db_model_revision_get_by_model_and_rev(NULL, 1, 1, NULL);
    TEST_ASSERT_NULL(mr);

    model_revision_t *mrl = acta_db_model_revision_list_by_model(NULL, 1, (int *)NULL, NULL);
    TEST_ASSERT_NULL(mrl);

    acta_db_model_revision_free(NULL);
    acta_db_model_revision_list_free(NULL, 0);

    /* skill */
    skill_t *s = acta_db_skill_get(NULL, 1);
    TEST_ASSERT_NULL(s);

    s = acta_db_skill_get_live(NULL, 1);
    TEST_ASSERT_NULL(s);

    skill_t *sl = acta_db_skill_list_all(NULL, (int *)NULL);
    TEST_ASSERT_NULL(sl);

    acta_db_skill_free(NULL);
    acta_db_skill_list_free(NULL, 0);

    /* skill folder */
    skill_folder_t *sf = acta_db_skill_folder_get(NULL, 1);
    TEST_ASSERT_NULL(sf);

    skill_folder_t *sfl = acta_db_skill_folder_list_all(NULL, (int *)NULL);
    TEST_ASSERT_NULL(sfl);

    acta_db_skill_folder_free(NULL);
    acta_db_skill_folder_list_free(NULL, 0);

    /* skill revision */
    skill_revision_t *sr = acta_db_skill_revision_get(NULL, 1, NULL);
    TEST_ASSERT_NULL(sr);

    sr = acta_db_skill_revision_get_by_skill_and_rev(NULL, 1, 1, NULL);
    TEST_ASSERT_NULL(sr);

    skill_revision_t *srl = acta_db_skill_revision_list_by_skill(NULL, 1, (int *)NULL, NULL);
    TEST_ASSERT_NULL(srl);

    acta_db_skill_revision_free(NULL);
    acta_db_skill_revision_list_free(NULL, 0);

    /* execution */
    execution_t *e = acta_db_execution_get(NULL, 1);
    TEST_ASSERT_NULL(e);

    execution_t *el = acta_db_execution_list_by_status(NULL, "pending", (int *)NULL);
    TEST_ASSERT_NULL(el);

    execution_t *ecl = acta_db_execution_list_children(NULL, 1, (int *)NULL);
    TEST_ASSERT_NULL(ecl);

    acta_db_execution_free(NULL);
    acta_db_execution_list_free(NULL, 0);

    /* execution log */
    execution_log_t *log_items = NULL;
    int log_rc = acta_db_execution_log_list_by_execution(NULL, 1, &log_items, (int *)NULL, NULL);
    TEST_ASSERT(log_rc < 0);
    TEST_ASSERT_NULL(log_items);

    acta_db_execution_log_free(NULL);
    acta_db_execution_log_list_free(NULL, 0);

    /* db functions */
    acta_db_close(NULL);
    TEST_ASSERT(1); /* survived */
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

/* ---------- Runner ---------- */
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
}
