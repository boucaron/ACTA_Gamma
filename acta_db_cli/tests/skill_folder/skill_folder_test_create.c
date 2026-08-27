#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    /* create a root-level folder with just a name */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "BrandNewFolder", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_with_parent(stest_ctx_t *ctx)
{
    /* create a child under folder 1 (authSkill) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "ChildOfAuth", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "IdOnlySF", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be a bare integer + newline, no braces */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --name */
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_root_unique_violation(stest_ctx_t *ctx)
{
    /* "authSkill" already exists at root (id=1 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "authSkill", &g);

    int rc = do_create(ctx, a, g);
    /* should fail: uq_skill_folders_root on (name) WHERE parent_id IS NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_child_unique_violation(stest_ctx_t *ctx)
{
    /* "oauthFlow" already exists in parent 1 (id=2 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "oauthFlow", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    /* should fail: uq_skill_folders_child on (parent_id, name) WHERE parent_id IS NOT NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_same_name_different_parent_ok(stest_ctx_t *ctx)
{
    /* "oauthFlow" exists under parent 1 (id=2).
     * Create "oauthFlow" under parent 0 (root) → should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "oauthFlow", &g);
    /* no parent_id → defaults to 0 (root) */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_negative_parent_clamped(stest_ctx_t *ctx)
{
    /* code clamps parent_id < 0 → 0 (root) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NegParent", &g);
    targs_flag(a, "parent_id", "-3", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_nonexistent_parent(stest_ctx_t *ctx)
{
    /* parent_id=99999 does not exist → FK violation */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "OrphanChild", &g);
    targs_flag(a, "parent_id", "99999", &g);

    int rc = do_create(ctx, a, g);
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

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_with_parent(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_name(&ctx);
    test_create_empty_name(&ctx);
    test_create_root_unique_violation(&ctx);
    test_create_child_unique_violation(&ctx);
    test_create_same_name_different_parent_ok(&ctx);
    test_create_negative_parent_clamped(&ctx);
    test_create_nonexistent_parent(&ctx);
    test_create_json_invalid(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
