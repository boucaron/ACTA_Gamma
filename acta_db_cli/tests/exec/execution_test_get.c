/* ─────────────────────────────────────────────────────────────────────
 * execution_test_get.c
 * Unit tests for:  actagamma_db exec get <id>
 * ───────────────────────────────────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

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

/* ── tests ─────────────────────────────────────────────────────────── */

static void test_get_basic(stest_ctx_t *ctx)
{
    /* ref DB: exec id=1 exists, status='pending', prompt=NULL */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\"");
    targs_free(a, &g);
}

static void test_get_with_prompt(stest_ctx_t *ctx)
{
    /* ref DB: exec id=4 has prompt='Summarize the Q3 revenue report' */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"prompt\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Summarize the Q3");
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "\n");
    targs_free(a, &g);
}

static void test_get_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "ID");
    targs_free(a, &g);
}

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("id,status");
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\"");
    /* should NOT contain other fields */
    TEST(ctx, !strstr(stest_stdout(ctx), "\"prompt\""));
    TEST(ctx, !strstr(stest_stdout(ctx), "\"context_id\""));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* exec id=1 has raw_response=NULL, result=NULL, etc. */
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST(ctx, !strstr(stest_stdout(ctx), "\"raw_response\""));
    TEST(ctx, !strstr(stest_stdout(ctx), "\"result\""));
    targs_free(a, &g);
}

static void test_get_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_id_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-5", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_non_numeric(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_exec(ctx, "get", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_exec(ctx, "get", a, g);
    /* not found → EXIT_NOT_FOUND + JSON error on stderr (P3) */
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_execution_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_basic(&ctx);
    test_get_with_prompt(&ctx);
    test_get_id_only(&ctx);
    test_get_table(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);
    test_get_missing_positional(&ctx);
    test_get_id_zero(&ctx);
    test_get_id_negative(&ctx);
    test_get_non_numeric(&ctx);
    test_get_nonexistent(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
