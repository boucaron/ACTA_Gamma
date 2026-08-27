/* ── execution_log_test_create.c ──────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_execution_log("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "1", &g);
    targs_flag(a, "level", "info", &g);
    targs_flag(a, "event", "stage_started", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "1", &g);
    targs_flag(a, "level", "warn", &g);
    targs_flag(a, "event", "retry", &g);
    targs_flag(a, "message", "attempt 2", &g);
    targs_flag(a, "metadata", "{\"n\":2}", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "1", &g);
    targs_flag(a, "level", "debug", &g);
    targs_flag(a, "event", "x", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --execution_id */
    targs_flag(a, "level", "info", &g);
    targs_flag(a, "event", "e", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_level(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "1", &g);
    targs_flag(a, "event", "e", &g);
    /* no --level */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_event(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "1", &g);
    targs_flag(a, "level", "info", &g);
    /* no --event */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_invalid_level(stest_ctx_t *ctx)
{
    /* "warning" is not a valid level (must be debug/info/warn/error) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "1", &g);
    targs_flag(a, "level", "warning", &g);
    targs_flag(a, "event", "e", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_zero_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "0", &g);
    targs_flag(a, "level", "info", &g);
    targs_flag(a, "event", "e", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_negative_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "execution_id", "-3", &g);
    targs_flag(a, "level", "info", &g);
    targs_flag(a, "event", "e", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
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

int run_execution_log_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_execution_id(&ctx);
    test_create_missing_level(&ctx);
    test_create_missing_event(&ctx);
    test_create_invalid_level(&ctx);
    test_create_zero_execution_id(&ctx);
    test_create_negative_execution_id(&ctx);
    test_create_json_invalid(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
