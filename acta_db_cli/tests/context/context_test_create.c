#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "hello", &g);
    targs_flag(a, "hash", "testhash1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "session", &g);
    targs_flag(a, "content", "full payload", &g);
    targs_flag(a, "hash", "fullhash", &g);
    targs_flag(a, "metadata", "{\"k\":\"v\"}", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "idonly", &g);
    targs_flag(a, "hash", "idonlyhash", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "\n");
    targs_free(a, &g);
}

static void test_create_missing_type(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --type */
    targs_flag(a, "content", "orphan", &g);
    targs_flag(a, "hash", "orphanhash", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_content(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    /* no --content */
    targs_flag(a, "hash", "nocontenthash", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_hash(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "nohash", &g);
    /* no --hash */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_all_missing(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no flags at all → "misspelled flag" warning path */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_json_invalid(stest_ctx_t *ctx)
{
    /* --json with garbage stdin → EXIT_INVALID.
     * Cannot easily fake stdin in a unit test.
     * ADAPT: mark as integration test or inject a stdin pipe. */
    (void)ctx;
}

static void test_create_duplicate_content_ok(stest_ctx_t *ctx)
{
    /* contexts has no unique index on (type, content, hash),
     * so inserting a duplicate should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "hello world", &g);
    targs_flag(a, "hash", "abc123", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_type(&ctx);
    test_create_missing_content(&ctx);
    test_create_missing_hash(&ctx);
    test_create_all_missing(&ctx);
    test_create_json_invalid(&ctx);
    test_create_duplicate_content_ok(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
