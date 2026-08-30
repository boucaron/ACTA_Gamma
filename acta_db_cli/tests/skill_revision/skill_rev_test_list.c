/* ── skill_rev_test_list.c ─────────────────────────────────────────── */
#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers ───────────────────────────────────────────────────────── */

static int do_rev(stest_ctx_t *ctx, const char *action, cmd_args_t *args,
                 global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_rev(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── list <skill_id> tests ─────────────────────────────────────────── */

static void test_list_multi_revision(stest_ctx_t *ctx)
{
    /* skill 1 has 3 revisions */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    TEST_CONTAINS(ctx, out, "\"revision\":3");
    targs_free(a, &g);
}

static void test_list_single_revision(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":5");
    targs_free(a, &g);
}

static void test_list_offset(stest_ctx_t *ctx)
{
    /* offset 1 on skill 1 → skip rev 1, return rev 2 and 3 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    TEST_CONTAINS(ctx, out, "\"revision\":3");
    TEST(ctx, !strstr(out, "\"revision\":1,"));
    targs_free(a, &g);
}

static void test_list_limit(stest_ctx_t *ctx)
{
    /* limit 2 on skill 1 → return rev 1 and 2 only */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "2", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    TEST(ctx, !strstr(out, "\"revision\":3"));
    targs_free(a, &g);
}

static void test_list_offset_and_limit(stest_ctx_t *ctx)
{
    /* offset 1, limit 1 on skill 1 → return only rev 2 (id 8) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "1", &g);
    targs_flag(a, "limit", "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":8");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    targs_free(a, &g);
}

static void test_list_limit_zero_unlimited(stest_ctx_t *ctx)
{
    /* --limit 0 means "no limit" per the code comment */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "0", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    TEST_CONTAINS(ctx, out, "\"revision\":3");
    targs_free(a, &g);
}

static void test_list_empty_skill(stest_ctx_t *ctx)
{
    /* no revisions for skill 99999 → "[]" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
    targs_free(a, &g);
}

static void test_list_offset_beyond_end(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "10", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
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

static void test_list_invalid_skill_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_skill_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-5", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_offset_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "-1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_limit_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "-2", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_offset_non_numeric(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "abc", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_limit_non_numeric(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "xyz", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    /* --count → prints just the number, no rows
     * (--count is a global flag; parse_globals strips it from ga) */
    global_opts_t g = gopts_default();
    g.count = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "3\n");
    targs_free(a, &g);
}

static void test_list_count_flag_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.count = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0\n");
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_skill_rev_test_list(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_multi_revision(&ctx);
    test_list_single_revision(&ctx);
    test_list_offset(&ctx);
    test_list_limit(&ctx);
    test_list_offset_and_limit(&ctx);
    test_list_limit_zero_unlimited(&ctx);
    test_list_empty_skill(&ctx);
    test_list_offset_beyond_end(&ctx);
    test_list_missing_positional(&ctx);
    test_list_invalid_skill_zero(&ctx);
    test_list_invalid_skill_negative(&ctx);
    test_list_offset_negative(&ctx);
    test_list_limit_negative(&ctx);
    test_list_offset_non_numeric(&ctx);
    test_list_limit_non_numeric(&ctx);
    test_list_count_flag(&ctx);
    test_list_count_flag_nonexistent(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
