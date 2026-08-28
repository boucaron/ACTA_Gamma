/* ─────────────────────────────────────────────────────────────────────
 * execution_test_create.c
 * Unit tests for:  actagamma_db exec create
 * ───────────────────────────────────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── local helper ──────────────────────────────────────────────────── */

static int do_exec(stest_ctx_t *ctx, const char *action,
                  cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_exec(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ─────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "What is the capital of France?", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "Full exec", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "parent_execution_id", "4", &g);
    targs_flag(a, "status", "pending", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "id only", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_prompt(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --prompt */
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_context_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "no ctx", &g);
    /* no --context_id */
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_skill_revision_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "no skill", &g);
    targs_flag(a, "context_id", "1", &g);
    /* no --skill_revision_id */
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_model_revision_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "no model", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    /* no --model_revision_id */

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_zero_context_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "zero ctx", &g);
    targs_flag(a, "context_id", "0", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_negative_skill_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "neg skill", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "-3", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_invalid_status(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad status", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "status", "bogus", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_status_running(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "run me", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "status", "running", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_status_cancelled(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "cancel me", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "status", "cancelled", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_valid_parent(stest_ctx_t *ctx)
{
    /* parent_execution_id=4 exists in ref DB */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "child exec", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "parent_execution_id", "4", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_context(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad ctx fk", &g);
    targs_flag(a, "context_id", "9999", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_skill_rev(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad skill fk", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "9999", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_model_rev(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad model fk", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "9999", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad parent fk", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "parent_execution_id", "9999", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_json_invalid(stest_ctx_t *ctx)
{
    /* --json with garbage stdin → EXIT_INVALID.
     * In a unit test we can't easily fake stdin.
     * ADAPT: if you have a way to inject stdin in unit tests, use it.
     * Otherwise skip / mark as integration test. */
    (void)ctx;
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_execution_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_prompt(&ctx);
    test_create_missing_context_id(&ctx);
    test_create_missing_skill_revision_id(&ctx);
    test_create_missing_model_revision_id(&ctx);
    test_create_zero_context_id(&ctx);
    test_create_negative_skill_id(&ctx);
    test_create_invalid_status(&ctx);
    test_create_status_running(&ctx);
    test_create_status_cancelled(&ctx);
    test_create_valid_parent(&ctx);
    test_create_fk_violation_context(&ctx);
    test_create_fk_violation_skill_rev(&ctx);
    test_create_fk_violation_model_rev(&ctx);
    test_create_fk_violation_parent(&ctx);
    test_create_json_invalid(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
