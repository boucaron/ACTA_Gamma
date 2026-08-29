/* ── skill_rev_test_count.c ────────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

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

/* ── count <skill_id> tests ────────────────────────────────────────── */

static void test_count_multi(stest_ctx_t *ctx)
{
    /* skill 1 has 3 revisions */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "3\n");
    targs_free(a, &g);
}

static void test_count_single(stest_ctx_t *ctx)
{
    /* skill 5 has 1 revision */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1\n");
    targs_free(a, &g);
}

static void test_count_nonexistent_skill(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0\n");
    targs_free(a, &g);
}

static void test_count_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_invalid_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_invalid_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_non_numeric(stest_ctx_t *ctx)
{
    /* "abc" rejected by parse_positive_id → invalid */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_rev(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_skill_rev_test_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_count_multi(&ctx);
    test_count_single(&ctx);
    test_count_nonexistent_skill(&ctx);
    test_count_missing_positional(&ctx);
    test_count_invalid_zero(&ctx);
    test_count_invalid_negative(&ctx);
    test_count_non_numeric(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
