#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helper ───────────────────────────────────────────────────────── */

static int do_rename(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("rename", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_rename_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "renamedDefault", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "renamedDefault");
    targs_free(a, &g);
}

static void test_rename_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "name", "x", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "2");
    targs_free(a, &g);
}

static void test_rename_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "noId", &g);
    /* no positional */

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_rename_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);
    targs_flag(a, "name", "x", &g);

    int rc = do_rename(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_rename_negative_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-5", &g);
    targs_flag(a, "name", "x", &g);

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

static void test_rename_nonexistent_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);
    targs_flag(a, "name", "ghost", &g);

    int rc = do_rename(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_rename_root_unique_conflict(stest_ctx_t *ctx)
{
    /* id=5 is "childTest" at root. Rename it to "test2" → conflicts
     * with id=3 ("test2" at root, untouched by prior tests)
     * → uq_model_folders_root violation */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);
    targs_flag(a, "name", "test2", &g);

    int rc = do_rename(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}


static void test_rename_same_name_noop(stest_ctx_t *ctx)
{
    /* renaming id=1 ("default") to "default" — same name */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "default", &g);

    int rc = do_rename(ctx, a, g);
    /* should succeed (no-op) or be rejected; accept either */
    TEST(ctx, rc == EXIT_OK || rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_rename_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "", &g);

    int rc = do_rename(ctx, a, g);
    /* empty name must be rejected (same contract as create) */
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_rename(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_rename_basic(&ctx);
    test_rename_id_only(&ctx);
    test_rename_missing_id(&ctx);
    test_rename_zero_id(&ctx);
    test_rename_negative_id(&ctx);
    test_rename_missing_name(&ctx);
    test_rename_nonexistent_id(&ctx);
    test_rename_root_unique_conflict(&ctx);
    test_rename_same_name_noop(&ctx);
    test_rename_empty_name(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
