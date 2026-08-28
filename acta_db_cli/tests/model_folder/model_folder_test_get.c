#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers ──────────────────────────────────────────────────────── */

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_delete(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("delete", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_get_existing(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"name\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "default");
    targs_free(a, &g);
}

static void test_get_with_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "6", &g);  /* childTest under parent 1 */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":1");
    targs_free(a, &g);
}

static void test_get_root_null_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);  /* default, parent_id IS NULL */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":null");
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "2");
    targs_free(a, &g);
}

static void test_get_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "ID");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "NAME");
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
    TEST_CONTAINS(ctx, out, "\"id\":");
    TEST_CONTAINS(ctx, out, "\"name\":");
    TEST(ctx, !strstr(out, "\"created_at\""));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    /* id=1 has updated_at=NULL, deleted_at=NULL */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* no_nulls should omit "updated_at":null */
    TEST(ctx, !strstr(out, "\"updated_at\":null"));
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_get(ctx, a, g);
    /* code returns EXIT_OK with no output when not found */
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_get_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

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

static void test_get_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional arg */

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_after_delete(stest_ctx_t *ctx)
{
    /* Delete id 3 first, then verify get still returns it with deleted_at set */
    global_opts_t g = gopts_default();
    cmd_args_t *da = targs_new();
    targs_pos(da, "3", &g);
    do_delete(ctx, da, g);
    targs_free(da, &g);

    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted_at\":");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_existing(&ctx);
    test_get_with_parent(&ctx);
    test_get_root_null_parent(&ctx);
    test_get_id_only(&ctx);
    test_get_table(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);
    test_get_nonexistent(&ctx);
    test_get_zero_id(&ctx);
    test_get_negative_id(&ctx);
    test_get_missing_positional(&ctx);
    test_get_after_delete(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
