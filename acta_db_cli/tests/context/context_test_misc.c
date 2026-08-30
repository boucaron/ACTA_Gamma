#include "test_helpers.h"


#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_action(stest_ctx_t *ctx, const char *action,
                    cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context(action, args, &gopts, ctx->db);
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
    TEST_CONTAINS(ctx, stest_stdout(ctx), "create");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "list");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "bogus", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "creat" is close to "create" → closest_action should suggest it */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "creat", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    /* The suggestion goes to stderr; in this harness stderr is not
     * captured, so we only verify the exit code.
     * If you add stderr capture, assert:
     *   TEST_CONTAINS(ctx, stest_stderr(ctx), "create"); */
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
