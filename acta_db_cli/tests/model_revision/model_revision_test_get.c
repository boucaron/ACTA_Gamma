#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

static int do_rev(stest_ctx_t *ctx, const char *action,
                 cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_revision(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ─────────────────────────────────────────────────────────── */

static void test_get_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    targs_free(a, &g);
}

static void test_get_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"model_id\":2");
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"name\":\"llama-70b\"");
    TEST_CONTAINS(ctx, out, "\"backend\":\"vllm\"");
    TEST_CONTAINS(ctx, out, "\"model_identifier\":\"meta/llama-70b\"");
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_get_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "llama-70b");
    TEST_CONTAINS(ctx, out, "vllm");
    targs_free(a, &g);
}

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("name,backend");
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"name\"");
    TEST_CONTAINS(ctx, out, "\"backend\"");
    TEST(ctx, !strstr(out, "\"model_id\""));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* description & configuration are NULL in rev id=4 */
    TEST(ctx, !strstr(out, "\"description\":null"));
    TEST(ctx, !strstr(out, "\"configuration\":null"));
    targs_free(a, &g);
}

static void test_get_not_found(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* empty or whitespace-only stdout */
    const char *out = stest_stdout(ctx);
    TEST(ctx, out[0] == '\0' || !strstr(out, "\"id\""));
    targs_free(a, &g);
}

static void test_get_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_negative_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-5", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_non_numeric(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);   /* "abc" rejected by parse_positive_id → invalid */

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional added */

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_deleted_revision(stest_ctx_t *ctx)
{
    /* id=3 has deleted_at set; DB layer decides visibility */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);

    int rc = do_rev(ctx, "get", a, g);
    /* either found (rc=OK, output present) or filtered (rc=OK, empty) */
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_revision_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_basic(&ctx);
    test_get_all_fields(&ctx);
    test_get_id_only(&ctx);
    test_get_table(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);
    test_get_not_found(&ctx);
    test_get_zero_id(&ctx);
    test_get_negative_id(&ctx);
    test_get_non_numeric(&ctx);
    test_get_missing_positional(&ctx);
    test_get_deleted_revision(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
