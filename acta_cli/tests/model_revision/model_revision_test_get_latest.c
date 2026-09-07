#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

static int do_rev(stest_ctx_t *ctx, const char *action,
                 cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_revision(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static void test_get_latest_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);   /* model 2 has exactly 1 revision */

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"model_id\":2");
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    targs_free(a, &g);
}

static void test_get_latest_multiple(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);   /* model 1 has revs 1,2,3 */

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"model_id\":1");
    /* latest should be rev 2 (non-deleted) or rev 3 — assert ≥ 2 */
    TEST(ctx, strstr(out, "\"revision\":2") || strstr(out, "\"revision\":3"));
    targs_free(a, &g);
}

static void test_get_latest_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "\n");
    targs_free(a, &g);
}

static void test_get_latest_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "llama-70b");
    targs_free(a, &g);
}

static void test_get_latest_no_nulls(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "\"description\":null"));
    targs_free(a, &g);
}

static void test_get_latest_not_found(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    /* no revisions → EXIT_NOT_FOUND + JSON error on stderr (P3) */
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

static void test_get_latest_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_latest_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_latest_non_numeric(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "xyz", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_latest_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_revision_test_get_latest(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_latest_basic(&ctx);
    test_get_latest_multiple(&ctx);
    test_get_latest_id_only(&ctx);
    test_get_latest_table(&ctx);
    test_get_latest_no_nulls(&ctx);
    test_get_latest_not_found(&ctx);
    test_get_latest_zero(&ctx);
    test_get_latest_negative(&ctx);
    test_get_latest_non_numeric(&ctx);
    test_get_latest_missing_positional(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
