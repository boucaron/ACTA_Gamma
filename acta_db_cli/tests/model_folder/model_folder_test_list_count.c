#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers ──────────────────────────────────────────────────────── */

static int do_list(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("list", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── list tests ───────────────────────────────────────────────────── */

static void test_list_all(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, out[0] == '[');
    TEST_CONTAINS(ctx, out, "\"id\":1");
    targs_free(a, &g);
}

static void test_list_by_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* id=6 is childTest under parent 1 */
    TEST_CONTAINS(ctx, out, "\"id\":6");
    /* id=1 is a root folder, should NOT appear */
    TEST(ctx, !strstr(out, "\"id\":1,"));
    targs_free(a, &g);
}

static void test_list_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "2", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* first two (id=1, id=2) skipped; id=3 should be first result */
    TEST_CONTAINS(ctx, out, "\"id\":3");
    targs_free(a, &g);
}

static void test_list_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "3", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* count occurrences of '"id":' should be exactly 3 */
    int count = 0;
    const char *p = out;
    while ((p = strstr(p, "\"id\":")) != NULL) { count++; p += 5; }
    TEST_EQ(ctx, count, 3);
    targs_free(a, &g);
}

static void test_list_offset_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "1", &g);
    targs_flag(a, "limit", "2", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* skip id=1, return id=2 and id=3 */
    TEST_CONTAINS(ctx, out, "\"id\":2");
    TEST_CONTAINS(ctx, out, "\"id\":3");
    int count = 0;
    const char *p = out;
    while ((p = strstr(p, "\"id\":")) != NULL) { count++; p += 5; }
    TEST_EQ(ctx, count, 2);
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.count = 1;   /* --count is a global flag; parse_globals strips it from ga */
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* should be a bare integer, no '[' */
    TEST(ctx, out[0] != '[');
    TEST(ctx, out[0] >= '0' && out[0] <= '9');
    targs_free(a, &g);
}

static void test_list_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "ID");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "NAME");
    targs_free(a, &g);
}

static void test_list_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("id,name");
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":");
    TEST_CONTAINS(ctx, out, "\"name\":");
    TEST(ctx, !strstr(out, "\"created_at\""));
    targs_free(a, &g);
}

static void test_list_no_nulls(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_no_nulls();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* all 8 ref rows have updated_at=NULL and deleted_at=NULL
     * → no_nulls omits those fields entirely from every object */
    TEST(ctx, !strstr(out, "\"updated_at\""));
    TEST(ctx, !strstr(out, "\"deleted_at\""));
    /* parent_id:null still appears for root rows (id 1,2,3,5) */
    TEST_CONTAINS(ctx, out, "\"parent_id\":null");
    targs_free(a, &g);
}


static void test_list_invalid_offset_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_offset_nonnum(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "abc", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_limit_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "-3", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_negative_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_empty_result(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "999", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[]");
    targs_free(a, &g);
}

/* ── count tests ──────────────────────────────────────────────────── */

static void test_count_all(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* 8 model_folders in ref DB */
    TEST_CONTAINS(ctx, out, "8");
    targs_free(a, &g);
}

static void test_count_by_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* only id=6 under parent 1 */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

static void test_count_by_parent_3(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "3", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* only id=4 under parent 3 */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "1");
    targs_free(a, &g);
}

static void test_count_negative_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "-1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_nonexistent_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "999", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "0");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_list_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_all(&ctx);
    test_list_by_parent(&ctx);
    test_list_offset(&ctx);
    test_list_limit(&ctx);
    test_list_offset_limit(&ctx);
    test_list_count_flag(&ctx);
    test_list_table(&ctx);
    test_list_fields(&ctx);
    test_list_no_nulls(&ctx);
    test_list_invalid_offset_negative(&ctx);
    test_list_invalid_offset_nonnum(&ctx);
    test_list_invalid_limit_negative(&ctx);
    test_list_negative_parent(&ctx);
    test_list_empty_result(&ctx);

    test_count_all(&ctx);
    test_count_by_parent(&ctx);
    test_count_by_parent_3(&ctx);
    test_count_negative_parent(&ctx);
    test_count_nonexistent_parent(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
