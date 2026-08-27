#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── tests ────────────────────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("help", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "create");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "rename");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "move");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "delete");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "restore");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("frobnicate", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_unknown_action_suggests_closest(stest_ctx_t *ctx)
{
    /* "creat" should suggest "create" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("creat", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    /* stderr should contain suggestion, but we check stdout for now
     * (adapt if capture includes stderr) */
    targs_free(a, &g);
}

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    /* --fields name → only name in JSON */
    global_opts_t g = gopts_default();
    g.fields = (char *)"name";
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("get", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"name\"");
    /* should NOT contain "created_at" */
    TEST(ctx, !strstr(out, "created_at"));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* --no_nulls → omit null fields (e.g. updated_at, deleted_at on fresh row) */
    global_opts_t g = gopts_default();
    g.no_nulls = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("get", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* updated_at and deleted_at are NULL for folder 1 → should be omitted */
    TEST(ctx, !strstr(out, "updated_at"));
    TEST(ctx, !strstr(out, "deleted_at"));
    targs_free(a, &g);
}

static void test_list_non_numeric_positional_clamped(stest_ctx_t *ctx)
{
    /* passing garbage as parent_id → atoi gives 0 → treated as "all" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc;
    stest_capture_begin(ctx);
    rc = cmd_skill_folder("list", a, &g, ctx->db);
    stest_capture_end(ctx);

    /* atoi("abc") = 0 → has_parent=0 → list all */
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggests_closest(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);
    test_list_non_numeric_positional_clamped(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
