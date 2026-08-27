/* ── execution_log_test_count.c ───────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_execution_log("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_count_basic(stest_ctx_t *ctx)
{
    /* execution_id=1 has 2 log entries */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "2");
    targs_free(a, &g);
}

static void test_count_missing_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_invalid_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "foo", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_negative_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-2", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_level_filter(stest_ctx_t *ctx)
{
    /* only id=1 has level='error' → count should be 1 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "level", "error", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

static void test_count_level_filter_none(stest_ctx_t *ctx)
{
    /* no entries with level='info' for execution 1 → 0 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "level", "info", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0");
    targs_free(a, &g);
}

static void test_count_nonexistent_execution(stest_ctx_t *ctx)
{
    /* execution_id=9999 → 0 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_execution_log_test_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_count_basic(&ctx);
    test_count_missing_execution_id(&ctx);
    test_count_invalid_execution_id(&ctx);
    test_count_negative_execution_id(&ctx);
    test_count_level_filter(&ctx);
    test_count_level_filter_none(&ctx);
    test_count_nonexistent_execution(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
