#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_get_existing_root(stest_ctx_t *ctx)
{
    /* folder id=1 "authSkill" at root */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"name\":\"authSkill\"");
    targs_free(a, &g);
}

static void test_get_existing_child(stest_ctx_t *ctx)
{
    /* folder id=2 "oauthFlow" under parent 1 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":2");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"name\":\"oauthFlow\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":1");
    targs_free(a, &g);
}

static void test_get_root_shows_null_parent(stest_ctx_t *ctx)
{
    /* root folder (parent_id NULL) → JSON shows "parent_id":null */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":null");
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
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_get_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

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
    targs_pos(a, "-5", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    /* id=99999 does not exist → returns EXIT_OK with no output (or empty) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_get(ctx, a, g);
    /* not found → EXIT_NOT_FOUND, no stdout, JSON error on stderr (P3) */
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "\"id\":99999"));
    targs_free(a, &g);
}

static void test_get_table_output(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.table = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "authSkill");
    targs_free(a, &g);
}

static void test_get_soft_deleted_folder(stest_ctx_t *ctx)
{
    /* get is by primary key and does NOT filter out soft-deleted rows:
     * seed a folder, soft-delete it, then get it back → deleted_at set */
    int fid = stest_seed_folder(ctx, "GoneMe", 0);
    if (fid <= 0) return;  /* skip if folder creation failed */

    char idbuf[16];
    snprintf(idbuf, sizeof idbuf, "%d", fid);

    global_opts_t dg = gopts_default();
    cmd_args_t *da = targs_new();
    targs_pos(da, idbuf, &dg);
    stest_capture_begin(ctx);
    int drc = cmd_skill_folder("delete", da, &dg, ctx->db);
    stest_capture_end(ctx);
    TEST_EQ(ctx, drc, EXIT_OK);
    targs_free(da, &dg);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, idbuf, &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    /* deleted_at must now be a value, not null */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted_at\":\"");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_existing_root(&ctx);
    test_get_existing_child(&ctx);
    test_get_root_shows_null_parent(&ctx);
    test_get_id_only(&ctx);
    test_get_missing_positional(&ctx);
    test_get_invalid_id_zero(&ctx);
    test_get_invalid_id_negative(&ctx);
    test_get_nonexistent(&ctx);
    test_get_table_output(&ctx);
    test_get_soft_deleted_folder(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
