/* ─────────────────────────────────────────────────────────────────────
 * execution_test_misc.c
 * Unit tests for:  exec help / unknown action / closest match
 * ───────────────────────────────────────────────────────────────────── */
#include "test_helpers.h"

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

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "create");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "get");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "list");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "bogus", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_closest_match_suggestion(stest_ctx_t *ctx)
{
    /* "creat" is close to "create" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "creat", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_empty_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_execution_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_closest_match_suggestion(&ctx);
    test_empty_action(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
