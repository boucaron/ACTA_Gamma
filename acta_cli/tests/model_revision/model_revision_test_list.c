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

static void test_list_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);   /* 2 live revisions (rev 3 is soft-deleted) */

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, out[0] == '[');
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    targs_free(a, &g);
}

static void test_list_single_item(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);   /* model 5 has 1 rev */

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"name\":\"test\"");
    /* exactly one object */
    const char *first = strchr(out, '{');
    const char *second = first ? strchr(first + 1, '{') : NULL;
    TEST(ctx, first && !second);
    targs_free(a, &g);
}

static void test_list_with_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* 2 live revs - 1 offset = 1 remaining (rev 2) */
    const char *out = stest_stdout(ctx);
    const char *first = strchr(out, '{');
    const char *second = first ? strchr(first + 1, '{') : NULL;
    TEST(ctx, first && !second);
    targs_free(a, &g);
}

static void test_list_with_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    const char *first = strchr(out, '{');
    const char *second = first ? strchr(first + 1, '{') : NULL;
    TEST(ctx, first && !second);
    targs_free(a, &g);
}

static void test_list_offset_and_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "1", &g);
    targs_flag(a, "limit", "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    /* --count is a global flag; parse_globals strips it from ga, so set it
     * on the global opts like the real dispatch path does */
    global_opts_t g = gopts_default();
    g.count = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");  /* live only */
    targs_free(a, &g);
}

static void test_list_count_global(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.count = 1;   /* set via global opts */
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");  /* live only */
    targs_free(a, &g);
}

static void test_list_empty_result(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "[]\n");
    targs_free(a, &g);
}

static void test_list_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "REV");
    TEST_CONTAINS(ctx, out, "test-model");
    targs_free(a, &g);
}

static void test_list_fields_filter(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("name,revision");
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"name\"");
    TEST_CONTAINS(ctx, out, "\"revision\"");
    TEST(ctx, !strstr(out, "\"backend\""));
    targs_free(a, &g);
}

static void test_list_no_nulls(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "\"description\":null"));
    targs_free(a, &g);
}

static void test_list_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_zero_model_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_negative_model_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-3", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_non_numeric_model_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "abc", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_negative_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "-1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "xyz", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_negative_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "-2", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_revision_test_list(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_basic(&ctx);
    test_list_single_item(&ctx);
    test_list_with_offset(&ctx);
    test_list_with_limit(&ctx);
    test_list_offset_and_limit(&ctx);
    test_list_count_flag(&ctx);
    test_list_count_global(&ctx);
    test_list_empty_result(&ctx);
    test_list_table(&ctx);
    test_list_fields_filter(&ctx);
    test_list_no_nulls(&ctx);
    test_list_missing_positional(&ctx);
    test_list_zero_model_id(&ctx);
    test_list_negative_model_id(&ctx);
    test_list_non_numeric_model_id(&ctx);
    test_list_invalid_offset(&ctx);
    test_list_negative_offset(&ctx);
    test_list_invalid_limit(&ctx);
    test_list_negative_limit(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
