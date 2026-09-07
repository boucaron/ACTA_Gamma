#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_rename(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("rename", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_rename_existing(stest_ctx_t *ctx)
{
    /* rename folder 1 "authSkill" → "AuthSkillRenamed" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "AuthSkillRenamed", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    targs_free(a, &g);
}

static void test_rename_same_name_noop(stest_ctx_t *ctx)
{
    /* folder 1 is "AuthSkillRenamed" now (from test_rename_existing);
     * renaming it to its current name is an idempotent success */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "AuthSkillRenamed", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_rename_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "Something", &g);
    /* no positional */

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_rename_invalid_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);
    targs_flag(a, "name", "X", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_rename_missing_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    /* no --name */

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_rename_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_rename_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);
    targs_flag(a, "name", "Ghost", &g);

    int rc = do_rename(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_rename_root_unique_violation(stest_ctx_t *ctx)
{
    /* seed a second root folder, then rename folder 1 onto its name
     * → violates uq_skill_folders_root (name) WHERE parent_id IS NULL */
    int fid = stest_seed_folder(ctx, "RootB", 0);
    if (fid <= 0) return;  /* skip if folder creation failed */

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "RootB", &g);

    int rc = do_rename(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_rename_child_unique_violation(stest_ctx_t *ctx)
{
    /* seed a child under parent 1, then rename folder 2 (also a child
     * of 1) onto its name
     * → violates uq_skill_folders_child (parent_id, name) */
    int fid = stest_seed_folder(ctx, "DupChild", 1);
    if (fid <= 0) return;  /* skip if folder creation failed */

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "name", "DupChild", &g);

    int rc = do_rename(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_rename(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_rename_existing(&ctx);
    test_rename_same_name_noop(&ctx);
    test_rename_missing_id(&ctx);
    test_rename_invalid_id_zero(&ctx);
    test_rename_missing_name(&ctx);
    test_rename_empty_name(&ctx);
    test_rename_nonexistent(&ctx);
    test_rename_root_unique_violation(&ctx);
    test_rename_child_unique_violation(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
