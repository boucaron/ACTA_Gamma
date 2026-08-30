#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helper ───────────────────────────────────────────────────────── */

static int do_action(stest_ctx_t *ctx, const char *action,
                    cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder(action, args, &gopts, ctx->db);
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
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "Usage:");
    TEST_CONTAINS(ctx, out, "create");
    TEST_CONTAINS(ctx, out, "get");
    TEST_CONTAINS(ctx, out, "list");
    TEST_CONTAINS(ctx, out, "count");
    TEST_CONTAINS(ctx, out, "rename");
    TEST_CONTAINS(ctx, out, "delete");
    TEST_CONTAINS(ctx, out, "restore");
    TEST_CONTAINS(ctx, out, "move");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "frobnicate", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "cretae" is close to "create" — closest_action should suggest it */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "cretae", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_empty_string_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_help_no_args_needed(stest_ctx_t *ctx)
{
    /* help works with empty args set */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);
    test_empty_string_action(&ctx);
    test_help_no_args_needed(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
