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

/* ── help ─────────────────────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "Usage:");
    TEST_CONTAINS(ctx, out, "get-latest");
    TEST_CONTAINS(ctx, out, "count");
    targs_free(a, &g);
}

/* ── unknown action ───────────────────────────────────────────────── */

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "frobnicate", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "listt" is close to "list" → closest_action should fire */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_rev(ctx, "listt", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    /* suggestion text goes to stderr; can only assert rc here */
    targs_free(a, &g);
}

/* ── flag-rejection edge cases ────────────────────────────────────── */

/* KI-6 (fixed, regression pin): --id_only used to be silently ignored
 * on list actions (full JSON printed); model_revision list now
 * rejects it with exit 4. */
static void test_list_id_only_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_fields_no_match(stest_ctx_t *ctx)
{
    /* --fields with a name that matches no struct field → bare {} */
    global_opts_t g = gopts_fields("nonexistent");
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_rev(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, strstr(out, "{}") != NULL);
    targs_free(a, &g);
}

static void test_list_table_empty(stest_ctx_t *ctx)
{
    /* --table + 0 rows → header printed, no data rows */
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "REV");    /* header present */
    /* no "}" and no "[" — it's not JSON */
    TEST(ctx, !strstr(out, "["));
    targs_free(a, &g);
}

static void test_list_offset_exceeds(stest_ctx_t *ctx)
{
    /* model 1 has ≤3 revs; offset 10 → empty */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "offset", "10", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "[]\n");
    targs_free(a, &g);
}

static void test_list_limit_zero_unlimited(stest_ctx_t *ctx)
{
    /* limit=0 means "no limit" per the code comment */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "limit", "0", &g);

    int rc = do_rev(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* should have all 3 (or 2 if deleted-filtered) — at least 2 */
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_revision_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);
    test_list_id_only_rejected(&ctx);
    test_get_fields_no_match(&ctx);
    test_list_table_empty(&ctx);
    test_list_offset_exceeds(&ctx);
    test_list_limit_zero_unlimited(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
