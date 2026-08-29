#include "skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static void test_get_existing(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"name\":\"summarize_v2\"");
    TEST_CONTAINS(ctx, out, "\"prompt_template\":\"Summarize concisely: {{input}}\"");
    targs_free(a, &g);
}

static void test_get_root_skill(stest_ctx_t *ctx)
{
    /* id=2 "tata" at root */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"name\":\"tata\"");
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);  /* not-found is not an error, just empty */
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "1\n");
    targs_free(a, &g);
}

static void test_get_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "ID");
    TEST_CONTAINS(ctx, out, "FOLDER_ID");
    TEST_CONTAINS(ctx, out, "NAME");
    targs_free(a, &g);
}

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("id,name");
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\"");
    TEST_CONTAINS(ctx, out, "\"name\"");
    /* should NOT contain description or prompt_template */
    TEST(ctx, !strstr(out, "prompt_template"));
    TEST(ctx, !strstr(out, "description"));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* skill id=2 has description=NULL, output_schema=NULL */
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* null fields should be omitted */
    TEST(ctx, !strstr(out, "\"description\":null"));
    TEST(ctx, !strstr(out, "\"output_schema\":null"));
    targs_free(a, &g);
}

static void test_get_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_id_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-3", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_non_numeric_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_get(ctx, a, g);
    /* "abc" is rejected by parse_positive_id (endptr) → EXIT_INVALID */
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_include_deleted(stest_ctx_t *ctx)
{
    /* First delete skill 5 (root, "summarize2"), then get with/without flag */
    global_opts_t g = gopts_default();
    cmd_args_t *del = targs_new();
    targs_pos(del, "5", &g);
    (void)cmd_skill("delete", del, &g, ctx->db);
    targs_free(del, &g);

    /* without --include_deleted: should be "not found" (empty output) */
    global_opts_t g1 = gopts_default();
    cmd_args_t *a1 = targs_new();
    targs_pos(a1, "5", &g1);
    int rc1 = do_get(ctx, a1, g1);
    TEST_EQ(ctx, rc1, EXIT_OK);
    /* output should be empty or not contain "summarize2" */
    TEST(ctx, !strstr(stest_stdout(ctx), "summarize2"));
    targs_free(a1, &g1);

    /* with --include_deleted: should return the row */
    global_opts_t g2 = gopts_default();
    cmd_args_t *a2 = targs_new();
    targs_pos(a2, "5", &g2);
    targs_flag_bool(a2, "include_deleted", &g2);
    int rc2 = do_get(ctx, a2, g2);
    TEST_EQ(ctx, rc2, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "summarize2");
    targs_free(a2, &g2);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_existing(&ctx);
    test_get_root_skill(&ctx);
    test_get_nonexistent(&ctx);
    test_get_id_only(&ctx);
    test_get_table(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);
    test_get_missing_positional(&ctx);
    test_get_invalid_id_zero(&ctx);
    test_get_invalid_id_negative(&ctx);
    test_get_non_numeric_id(&ctx);
    test_get_include_deleted(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
