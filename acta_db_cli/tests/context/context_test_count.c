#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_count_all(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "7");
    targs_free(a, &g);
}

static void test_count_type_text(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "3");
    targs_free(a, &g);
}

static void test_count_hash_abc123(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "hash", "abc123", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "2");
    targs_free(a, &g);
}

static void test_count_no_match(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "bogus", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0");
    targs_free(a, &g);
}

static void test_count_user(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "user", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_count_all(&ctx);
    test_count_type_text(&ctx);
    test_count_hash_abc123(&ctx);
    test_count_no_match(&ctx);
    test_count_user(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
