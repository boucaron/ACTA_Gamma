/* ── skill_rev_test_misc.c ─────────────────────────────────────────── */
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

/* ── help / unknown action ─────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "get-latest");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "list");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "count");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "bogus", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    /* error goes to stderr; capture may or may not include stderr.
     * If not, just assert the return code. */
    targs_free(a, &g);
}

static void test_unknown_action_suggestion_list(stest_ctx_t *ctx)
{
    /* "lst" is close to "list" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "lst", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion_get(stest_ctx_t *ctx)
{
    /* "gt" is close to "get" */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "gt", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── output format: --table ────────────────────────────────────────── */

static void test_get_table_output(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.table = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "ID");
    TEST_CONTAINS(ctx, out, "SKILL");
    TEST_CONTAINS(ctx, out, "REV");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_list_table_output(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.table = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "ID");
    TEST_CONTAINS(ctx, out, "REV");
    TEST_CONTAINS(ctx, out, "FOLDER");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

/* ── output format: --fields ───────────────────────────────────────── */

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.fields = "id,name";
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\"");
    TEST_CONTAINS(ctx, out, "\"name\"");
    TEST(ctx, !strstr(out, "\"prompt_template\""));
    TEST(ctx, !strstr(out, "\"skill_id\""));
    targs_free(a, &g);
}

/* ── output format: --no_nulls ─────────────────────────────────────── */

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* id=1 has description=NULL → should be omitted with --no_nulls */
    global_opts_t g = gopts_default();
    g.no_nulls = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "\"description\""));
    TEST_CONTAINS(ctx, out, "\"name\"");
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_skill_rev_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggestion_list(&ctx);
    test_unknown_action_suggestion_get(&ctx);
    test_get_table_output(&ctx);
    test_list_table_output(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
