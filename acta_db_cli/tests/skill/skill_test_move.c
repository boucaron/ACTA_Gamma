#include "skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_move(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill("move", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static void test_move_to_folder(stest_ctx_t *ctx)
{
    /* move skill 5 (root, "summarize2") into folder 1 */
    cmd_args_t *a = targs_new();
    targs_pos(a, "5");
    targs_flag(a, "folder_id", "1");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":5");
    TEST_CONTAINS(ctx, out, "\"folder_id\":1");
    targs_free(a);
}

static void test_move_to_root(stest_ctx_t *ctx)
{
    /* move skill 3 (folder 1, "tata") to root (folder_id=0) */
    cmd_args_t *a = targs_new();
    targs_pos(a, "3");
    targs_flag(a, "folder_id", "0");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"folder_id\":0");
    targs_free(a);
}

static void test_move_missing_folder_flag(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_pos(a, "1");
    /* no --folder_id */
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a);
}

static void test_move_missing_positional(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_flag(a, "folder_id", "1");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a);
}

static void test_move_invalid_skill_id(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_pos(a, "0");
    targs_flag(a, "folder_id", "1");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a);
}

static void test_move_nonexistent_skill(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999");
    targs_flag(a, "folder_id", "1");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a);
}

static void test_move_nonexistent_folder(stest_ctx_t *ctx)
{
    /* folder 99 doesn't exist → FK violation */
    cmd_args_t *a = targs_new();
    targs_pos(a, "2");
    targs_flag(a, "folder_id", "99");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a);
}

static void test_move_negative_folder_clamped(stest_ctx_t *ctx)
{
    /* folder_id=-1 clamped to 0 → root */
    cmd_args_t *a = targs_new();
    targs_pos(a, "4");  /* in folder 2 */
    targs_flag(a, "folder_id", "-1");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a);
}

static void test_move_unique_violation(stest_ctx_t *ctx)
{
    /* "tata" exists in folder 2 (id=4). Move "tata" from folder 1 (id=3) to folder 2
     * → unique violation uq_skills_child(folder_id, name)
     */
    cmd_args_t *a = targs_new();
    targs_pos(a, "3");
    targs_flag(a, "folder_id", "2");
    global_opts_t g = gopts_default();

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a);
}

static void test_move_verify_folder_changed(stest_ctx_t *ctx)
{
    /* move skill 5 to folder 1, then get it and verify folder_id=1 */
    cmd_args_t *a = targs_new();
    targs_pos(a, "5");
    targs_flag(a, "folder_id", "1");
    global_opts_t g = gopts_default();
    (void)do_move(ctx, a, g);
    targs_free(a);

    /* verify */
    cmd_args_t *ga = targs_new();
    targs_pos(ga, "5");
    global_opts_t gg = gopts_fields("id,folder_id");
    stest_capture_begin(ctx);
    (void)cmd_skill("get", ga, &gg, ctx->db);
    stest_capture_end(ctx);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"folder_id\":1");
    targs_free(ga);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_test_move(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_move_to_folder(&ctx);
    test_move_to_root(&ctx);
    test_move_missing_folder_flag(&ctx);
    test_move_missing_positional(&ctx);
    test_move_invalid_skill_id(&ctx);
    test_move_nonexistent_skill(&ctx);
    test_move_nonexistent_folder(&ctx);
    test_move_negative_folder_clamped(&ctx);
    test_move_unique_violation(&ctx);
    test_move_verify_folder_changed(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
