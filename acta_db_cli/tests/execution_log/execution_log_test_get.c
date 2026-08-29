/* ── execution_log_test_get.c ─────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_execution_log("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_get_basic(stest_ctx_t *ctx)
{
    /* log id=1 exists: execution_id=1, level='error', event='' */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"level\"");
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "1");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_get_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "LEVEL");
    targs_free(a, &g);
}

static void test_get_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_invalid_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_negative_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    /* id=9999 does not exist → EXIT_NOT_FOUND, empty stdout, JSON error on stderr (P3) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    const char *out = stest_stdout(ctx);
    TEST(ctx, out[0] == '\0');  /* empty stdout */
    targs_free(a, &g);
}

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    /* --fields "id,level" → only those fields in JSON */
    global_opts_t g = gopts_fields("id,level");
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\"");
    TEST_CONTAINS(ctx, out, "\"level\"");
    TEST(ctx, !strstr(out, "\"event\""));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* id=1 has message=NULL, metadata=NULL → should be omitted */
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "\"message\""));
    TEST(ctx, !strstr(out, "\"metadata\""));
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_execution_log_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_basic(&ctx);
    test_get_id_only(&ctx);
    test_get_table(&ctx);
    test_get_missing_id(&ctx);
    test_get_invalid_id(&ctx);
    test_get_negative_id(&ctx);
    test_get_nonexistent(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
