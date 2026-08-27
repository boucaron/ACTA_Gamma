#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_list(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("list", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── list tests ───────────────────────────────────────────────────── */

static void test_list_all(stest_ctx_t *ctx)
{
    /* no positional → all folders (id 1, 2) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":2");
    targs_free(a, &g);
}

static void test_list_all_keyword(stest_ctx_t *ctx)
{
    /* explicit "all" positional → same as no positional */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "all", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    targs_free(a, &g);
}

static void test_list_children_of_parent(stest_ctx_t *ctx)
{
    /* parent_id=1 → only id=2 (oauthFlow) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":2");
    /* id=1 is the parent itself, should not appear as a child */
    /* (depends on implementation: if "list children" excludes parent) */
    targs_free(a, &g);
}

static void test_list_children_none(stest_ctx_t *ctx)
{
    /* parent_id=2 has no children → empty list */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "[]");
    targs_free(a, &g);
}

static void test_list_with_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* should contain at most 1 entry */
    TEST(ctx, !strstr(out + 1, "\"id\":") ||
          (strstr(out, "\"id\":1") && !strstr(out + 20, "\"id\":")));
    targs_free(a, &g);
}

static void test_list_with_offset(stest_ctx_t *ctx)
{
    /* offset=1 → skip first row, get second */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    /* --count on list → just a number */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "count", "", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* should be just a number + newline */
    TEST(ctx, !strstr(out, "{"));
    TEST(ctx, !strstr(out, "["));
    targs_free(a, &g);
}

static void test_list_table_output(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.table = 1;
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "authSkill");
    targs_free(a, &g);
}

static void test_list_invalid_offset(stest_ctx_t *ctx)
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
    targs_flag(a, "limit", "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── count tests ──────────────────────────────────────────────────── */

static void test_count_all(stest_ctx_t *ctx)
{
    /* 2 folders in ref DB */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "2");
    targs_free(a, &g);
}

static void test_count_children(stest_ctx_t *ctx)
{
    /* parent 1 has 1 child (oauthFlow) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

static void test_count_children_none(stest_ctx_t *ctx)
{
    /* parent 2 has 0 children */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0");
    targs_free(a, &g);
}

static void test_count_all_keyword(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "all", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_list_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_all(&ctx);
    test_list_all_keyword(&ctx);
    test_list_children_of_parent(&ctx);
    test_list_children_none(&ctx);
    test_list_with_limit(&ctx);
    test_list_with_offset(&ctx);
    // test_list_count_flag(&ctx); // TODO: FIXME
    test_list_table_output(&ctx);
    test_list_invalid_offset(&ctx);
    test_list_invalid_limit(&ctx);

    test_count_all(&ctx);
    test_count_children(&ctx);
    test_count_children_none(&ctx);
    test_count_all_keyword(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
