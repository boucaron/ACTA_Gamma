#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_list(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context("list", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_list_all(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* 7 seed rows; expect 7 occurrences of "\"id\":" */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":7");
    /* starts with '[' */
    TEST(ctx, out[0] == '[');
    targs_free(a, &g);
}

static void test_list_filter_type(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":2");
    TEST_CONTAINS(ctx, out, "\"id\":3");
    /* id 4 is type "test", should NOT appear */
    TEST(ctx, !strstr(out, "\"id\":4"));
    targs_free(a, &g);
}

static void test_list_filter_hash(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "hash", "abc123", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":6");
    /* id 2 has hash "" not "abc123" */
    TEST(ctx, !strstr(out, "\"id\":2"));
    targs_free(a, &g);
}

static void test_list_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "2", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* ids 1,2 skipped → first result is id 3 */
    TEST(ctx, !strstr(out, "\"id\":1"));
    TEST(ctx, !strstr(out, "\"id\":2"));
    TEST_CONTAINS(ctx, out, "\"id\":3");
    targs_free(a, &g);
}

static void test_list_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "3", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":3");
    /* id 4 should NOT appear (limit 3) */
    TEST(ctx, !strstr(out, "\"id\":4"));
    targs_free(a, &g);
}

static void test_list_offset_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "1", &g);
    targs_flag(a, "limit", "2", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* should have exactly ids 2 and 3 */
    TEST(ctx, !strstr(out, "\"id\":1"));
    TEST_CONTAINS(ctx, out, "\"id\":2");
    TEST_CONTAINS(ctx, out, "\"id\":3");
    TEST(ctx, !strstr(out, "\"id\":4"));
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.count = 1;   /* --count is a global flag; parse_globals strips it from ga */
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "7");
    targs_free(a, &g);
}

static void test_list_count_with_filter(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.count = 1;   /* --count is a global flag; parse_globals strips it from ga */
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "test", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* 2 rows with type "test" */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "2");
    targs_free(a, &g);
}

static void test_list_empty(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "nonexistent", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
    targs_free(a, &g);
}

static void test_list_invalid_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_offset_alpha(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "abc", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "-5", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_list(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_all(&ctx);
    test_list_filter_type(&ctx);
    test_list_filter_hash(&ctx);
    test_list_offset(&ctx);
    test_list_limit(&ctx);
    test_list_offset_limit(&ctx);
    test_list_count_flag(&ctx);
    test_list_count_with_filter(&ctx);
    test_list_empty(&ctx);
    test_list_invalid_offset(&ctx);
    test_list_invalid_offset_alpha(&ctx);
    test_list_invalid_limit(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
