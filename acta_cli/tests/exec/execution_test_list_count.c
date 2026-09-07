/* ─────────────────────────────────────────────────────────────────────
 * execution_test_list_count.c
 * Unit tests for:  exec list / exec count
 * ───────────────────────────────────────────────────────────────────── */
#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── local helper ──────────────────────────────────────────────────── */

static int do_exec(stest_ctx_t *ctx, const char *action,
                  cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_exec(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ══════════════════════════════════════════════════════════════════ */
/*  list                                                             */
/* ══════════════════════════════════════════════════════════════════ */

static void test_list_all(stest_ctx_t *ctx)
{
    /* ref DB has 5 executions */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "]");
    targs_free(a, &g);
}

static void test_list_filter_status_pending(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "status", "pending", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* all 5 are pending in ref DB */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_list_filter_status_completed(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "status", "completed", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
    targs_free(a, &g);
}

static void test_list_filter_context_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "context_id", "1", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_list_filter_skill_revision_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "skill_revision_id", "1", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_list_filter_model_revision_id(stest_ctx_t *ctx)
{
    /* model_revision_id=2 → execs 1,3,4,5 (4 items) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_list_filter_parent(stest_ctx_t *ctx)
{
    /* only exec id=5 has parent_execution_id=4 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_execution_id", "4", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_list_offset_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "1", &g);
    targs_flag(a, "limit", "2", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[");
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.count = 1;
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be a bare number */
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "["));
    TEST(ctx, !strstr(out, "\""));
    targs_free(a, &g);
}

static void test_list_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "ID");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "STATUS");
    targs_free(a, &g);
}

static void test_list_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("id,status");
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\"");
    TEST(ctx, !strstr(stest_stdout(ctx), "\"prompt\""));
    targs_free(a, &g);
}

static void test_list_no_nulls(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* exec id=1 has NULL raw_response; no_nulls should omit it */
    TEST(ctx, !strstr(stest_stdout(ctx), "\"raw_response\""));
    targs_free(a, &g);
}

static void test_list_invalid_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "-1", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_offset_nonnum(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "abc", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "-1", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_limit_nonnum(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "xyz", &g);

    int rc = do_exec(ctx, "list", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  count                                                            */
/* ══════════════════════════════════════════════════════════════════ */

static void test_count_all(stest_ctx_t *ctx)
{
    /* ref DB has 5 executions */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "5");
    targs_free(a, &g);
}

static void test_count_filter_status(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "status", "pending", &g);

    int rc = do_exec(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "5");
    targs_free(a, &g);
}

static void test_count_filter_model_rev(stest_ctx_t *ctx)
{
    /* model_revision_id=1 → only exec id=2 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "model_revision_id", "1", &g);

    int rc = do_exec(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

static void test_count_filter_parent(stest_ctx_t *ctx)
{
    /* parent_execution_id=4 → only exec id=5 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_execution_id", "4", &g);

    int rc = do_exec(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

static void test_count_no_match(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "status", "failed", &g);

    int rc = do_exec(ctx, "count", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0");
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_execution_test_list_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    /* list */
    test_list_all(&ctx);
    test_list_filter_status_pending(&ctx);
    test_list_filter_status_completed(&ctx);
    test_list_filter_context_id(&ctx);
    test_list_filter_skill_revision_id(&ctx);
    test_list_filter_model_revision_id(&ctx);
    test_list_filter_parent(&ctx);
    test_list_offset_limit(&ctx);
    test_list_count_flag(&ctx);
    test_list_table(&ctx);
    test_list_fields(&ctx);
    test_list_no_nulls(&ctx);
    test_list_invalid_offset(&ctx);
    test_list_invalid_offset_nonnum(&ctx);
    test_list_invalid_limit(&ctx);
    test_list_invalid_limit_nonnum(&ctx);

    /* count */
    test_count_all(&ctx);
    test_count_filter_status(&ctx);
    test_count_filter_model_rev(&ctx);
    test_count_filter_parent(&ctx);
    test_count_no_match(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
