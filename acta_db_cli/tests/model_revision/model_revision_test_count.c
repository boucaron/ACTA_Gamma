#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

static int do_rev(stest_ctx_t *ctx, const char *action,
                 cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_revision(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static void test_count_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);   /* model 1 → 3 revisions */

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "3\n");
    targs_free(a, &g);
}

static void test_count_single(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "1\n");
    targs_free(a, &g);
}

static void test_count_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "0\n");
    targs_free(a, &g);
}

static void test_count_folder_model(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);   /* model 4 (in folder 1) → 1 revision */

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "1\n");
    targs_free(a, &g);
}

static void test_count_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_non_numeric(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "hello", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_revision_test_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_count_basic(&ctx);
    test_count_single(&ctx);
    test_count_zero(&ctx);
    test_count_folder_model(&ctx);
    test_count_missing_positional(&ctx);
    test_count_zero_id(&ctx);
    test_count_negative(&ctx);
    test_count_non_numeric(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
