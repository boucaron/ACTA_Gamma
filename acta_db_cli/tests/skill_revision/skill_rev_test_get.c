/* ── skill_rev_test_get.c ──────────────────────────────────────────── */
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

/* ── get <id> tests ────────────────────────────────────────────────── */

static void test_get_existing(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"skill_id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":1");
    targs_free(a, &g);
}

static void test_get_all_fields(stest_ctx_t *ctx)
{
    /* id=9 has folder_id=2, name="summarize_v2", revision=3 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":9");
    TEST_CONTAINS(ctx, out, "\"folder_id\":2");
    TEST_CONTAINS(ctx, out, "\"name\":\"summarize_v2\"");
    TEST_CONTAINS(ctx, out, "\"revision\":3");
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
    TEST_CONTAINS(ctx, out, "1\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_rev(ctx, "get", a, g);
    /* ADAPT: depends on library semantics.
     * If library returns NULL with ACTA_DB_OK → EXIT_OK, empty output.
     * If library returns ACTA_DB_ERR_NOT_FOUND → mapped exit. */
    (void)rc;
    targs_free(a, &g);
}

static void test_get_invalid_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── get-latest <skill_id> tests ──────────────────────────────────── */

static void test_get_latest_multi_revision(stest_ctx_t *ctx)
{
    /* skill 1 has revisions 1, 2, 3 (ids 1, 8, 9). Latest = id 9. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":9");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":3");
    targs_free(a, &g);
}

static void test_get_latest_single_revision(stest_ctx_t *ctx)
{
    /* skill 5 has only revision 1 (id 5) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":5");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":1");
    targs_free(a, &g);
}

static void test_get_latest_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "9\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_get_latest_nonexistent_skill(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    /* ADAPT: same as test_get_nonexistent */
    (void)rc;
    targs_free(a, &g);
}

static void test_get_latest_invalid_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_rev(ctx, "get-latest", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_latest_invalid_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-3", &g);

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

/* ── runner ────────────────────────────────────────────────────────── */

int run_skill_rev_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_existing(&ctx);
    test_get_all_fields(&ctx);
    test_get_id_only(&ctx);
    test_get_nonexistent(&ctx);
    test_get_invalid_zero(&ctx);
    test_get_invalid_negative(&ctx);
    test_get_missing_positional(&ctx);
    test_get_latest_multi_revision(&ctx);
    test_get_latest_single_revision(&ctx);
    test_get_latest_id_only(&ctx);
    test_get_latest_nonexistent_skill(&ctx);
    test_get_latest_invalid_zero(&ctx);
    test_get_latest_invalid_negative(&ctx);
    test_get_latest_missing_positional(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
