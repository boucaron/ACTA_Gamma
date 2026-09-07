#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_get_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "1");
    targs_free(a, &g);
}

static void test_get_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* table output has a header row with "TYPE" */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "TYPE");
    targs_free(a, &g);
}

static void test_get_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("id,type");
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"type\"");
    /* content should NOT appear */
    TEST(ctx, !strstr(stest_stdout(ctx), "\"content\""));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* row 1 has metadata=NULL → should be omitted with --no_nulls */
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST(ctx, !strstr(stest_stdout(ctx), "\"metadata\""));
    targs_free(a, &g);
}

static void test_get_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional arg */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_alpha(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-3", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_not_found(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_get(ctx, a, g);
    /* not found → EXIT_NOT_FOUND + JSON error on stderr (P3) */
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_basic(&ctx);
    test_get_id_only(&ctx);
    test_get_table(&ctx);
    test_get_fields(&ctx);
    test_get_no_nulls(&ctx);
    test_get_missing_id(&ctx);
    test_get_invalid_alpha(&ctx);
    test_get_invalid_negative(&ctx);
    test_get_invalid_zero(&ctx);
    test_get_not_found(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
