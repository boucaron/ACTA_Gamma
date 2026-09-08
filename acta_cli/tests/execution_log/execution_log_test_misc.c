/* ── execution_log_test_misc.c ────────────────────────────────────── */
#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_action(stest_ctx_t *ctx, const char *action,
                    cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_execution_log(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    targs_free(a, &g);
}

static void test_help_contains_actions(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "create");
    TEST_CONTAINS(ctx, out, "get");
    TEST_CONTAINS(ctx, out, "list");
    TEST_CONTAINS(ctx, out, "count");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "frobnicate", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "crete" is close to "create" → should get a suggestion */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "crete", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    /* suggestion is printed to stderr; we only verify the return code here.
     * If stderr capture is available, assert on "Did you mean 'create'" */
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_execution_log_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_help_contains_actions(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
