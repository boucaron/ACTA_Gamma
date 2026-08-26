#include "skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static void test_help(stest_ctx_t *ctx)
{
    stest_capture_begin(ctx);
    global_opts_t g = gopts_default();
    int rc = cmd_skill("help", NULL, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "Usage: actagamma_db skill");
    TEST_CONTAINS(ctx, out, "create");
    TEST_CONTAINS(ctx, out, "get");
    TEST_CONTAINS(ctx, out, "update");
    TEST_CONTAINS(ctx, out, "delete");
    TEST_CONTAINS(ctx, out, "restore");
    TEST_CONTAINS(ctx, out, "move");
    TEST_CONTAINS(ctx, out, "list");
    TEST_CONTAINS(ctx, out, "count");
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    stest_capture_begin(ctx);
    global_opts_t g = gopts_default();
    int rc = cmd_skill("frobnicate", NULL, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_typo_suggests_closest(stest_ctx_t *ctx)
{
    /* "creat" should suggest "create" */
    stest_capture_begin(ctx);
    global_opts_t g = gopts_default();
    int rc = cmd_skill("creat", NULL, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    /* stderr should contain "Did you mean 'create'?"
     * ADAPT: if you capture stderr too, assert here.
     * For now just verify the exit code.
     */
}

static void test_empty_action(stest_ctx_t *ctx)
{
    stest_capture_begin(ctx);
    global_opts_t g = gopts_default();
    int rc = cmd_skill("", NULL, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_verbose_does_not_corrupt_stdout(stest_ctx_t *ctx)
{
    /* run get with verbose=3 → stdout should still be clean JSON */
    global_opts_t g = gopts_default();
    g.verbose = 3;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    stest_capture_begin(ctx);
    int rc = cmd_skill("get", a, &g, ctx->db);
    stest_capture_end(ctx);
    targs_free(a, &g);

    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* stdout must be valid JSON (starts with '{') */
    TEST(ctx, out[0] == '{');
    /* must NOT contain "[v1]" or "[v2]" or "[v3]" */
    TEST(ctx, !strstr(out, "[v1]"));
    TEST(ctx, !strstr(out, "[v2]"));
    TEST(ctx, !strstr(out, "[v3]"));
}

static void test_json_error_envelope_shape(stest_ctx_t *ctx)
{
    /* on invalid input, stderr should contain a JSON error envelope:
     *   {"error":"ACTA_DB_ERR_INVALID","code":-4,"message":"..."}
     * Verify by checking the pattern in stderr.
     * ADAPT: if you capture stderr in the test ctx, assert here.
     *
     * For now: just verify exit code is EXIT_INVALID for a known-bad input.
     */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);  /* invalid id */

    stest_capture_begin(ctx);
    int rc = cmd_skill("get", a, &g, ctx->db);
    stest_capture_end(ctx);
    targs_free(a, &g);

    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_id_only_get(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);

    stest_capture_begin(ctx);
    int rc = cmd_skill("get", a, &g, ctx->db);
    stest_capture_end(ctx);
    targs_free(a, &g);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "3\n");
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_typo_suggests_closest(&ctx);
    test_empty_action(&ctx);
    test_verbose_does_not_corrupt_stdout(&ctx);
    test_json_error_envelope_shape(&ctx);
    test_id_only_get(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
