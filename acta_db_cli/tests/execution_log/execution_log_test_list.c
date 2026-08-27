/* ── execution_log_test_list.c ────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_list(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_execution_log("list", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_list_basic(stest_ctx_t *ctx)
{
    /* execution_id=1 has two log entries: id=1 (error) and id=2 (debug) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "[");
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":2");
    targs_free(a, &g);
}

static void test_list_missing_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "xyz", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_negative_execution_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_level_filter_error(stest_ctx_t *ctx)
{
    /* only id=1 has level='error' */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "level", "error", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST(ctx, !strstr(out, "\"id\":2"));
    targs_free(a, &g);
}

static void test_list_level_filter_none(stest_ctx_t *ctx)
{
    /* no entries with level='warn' for execution 1 → empty array */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "level", "warn", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
    targs_free(a, &g);
}

static void test_list_offset(stest_ctx_t *ctx)
{
    /* offset=1 → skip id=1, return id=2 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":2");
    TEST(ctx, !strstr(out, "\"id\":1"));
    targs_free(a, &g);
}

static void test_list_limit(stest_ctx_t *ctx)
{
    /* limit=1 → only first row (id=1) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST(ctx, !strstr(out, "\"id\":2"));
    targs_free(a, &g);
}

static void test_list_offset_and_limit(stest_ctx_t *ctx)
{
    /* offset=1 limit=1 → skip id=1, take 1 → only id=2 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "1", &g);
    targs_flag(a, "limit", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":2");
    TEST(ctx, !strstr(out, "\"id\":1"));
    targs_free(a, &g);
}

static void test_list_offset_invalid(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "-5", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_limit_invalid(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "-3", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_nonexistent_execution(stest_ctx_t *ctx)
{
    /* execution_id=9999 → no rows → empty array */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    /* --count → print only the number, no JSON array */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag_bool(a, "count", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "2");   /* two log entries for exec 1 */
    TEST(ctx, !strstr(out, "["));   /* no JSON array */
    targs_free(a, &g);
}

static void test_list_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "LEVEL");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_execution_log_test_list(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_basic(&ctx);
    test_list_missing_execution_id(&ctx);
    test_list_invalid_execution_id(&ctx);
    test_list_negative_execution_id(&ctx);
    test_list_level_filter_error(&ctx);
    test_list_level_filter_none(&ctx);
    test_list_offset(&ctx);
    test_list_limit(&ctx);
    test_list_offset_and_limit(&ctx);
    test_list_offset_invalid(&ctx);
    test_list_limit_invalid(&ctx);
    test_list_nonexistent_execution(&ctx);
    test_list_count_flag(&ctx);
    test_list_table(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
