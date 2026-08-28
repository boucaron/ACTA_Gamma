#include "skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers ──────────────────────────────────────────────────────── */

static int do_move(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("move", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_move_to_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);         /* move id=1 (default) */
    targs_flag(a, "parent_id", "3", &g);  /* to under id=3 (test2) */

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":1");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":3");
    targs_free(a, &g);
}

static void test_move_to_root_conflict(stest_ctx_t *ctx)
{
    /* id=6 is "childTest" under parent 1.
     * Moving to root would violate uq_model_folders_root
     * because id=5 ("childTest") already lives at root. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "6", &g);
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}


static void test_move_to_root(stest_ctx_t *ctx)
{
    /* Create a uniquely-named child under parent 3, then move it to root. */
    global_opts_t g = gopts_default();
    cmd_args_t *ca = targs_new();
    targs_flag(ca, "name", "uniqueMoveMe", &g);
    targs_flag(ca, "parent_id", "3", &g);
    do_create(ctx, ca, g);
    targs_free(ca, &g);

    /* Now move it (id will be the next autoincrement, grab via get or
     * just move by name isn't supported — use the id we know is new.
     * In the ref DB max id=8, so new folder is id=9. */
    cmd_args_t *a = targs_new();
    targs_pos(a, "9", &g);
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":null");
    targs_free(a, &g);
}


static void test_move_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "7", &g);         /* childTest under parent 2 */
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "7");
    targs_free(a, &g);
}

static void test_move_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "parent_id", "1", &g);
    /* no positional */

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_negative_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_missing_parent_flag(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    /* no --parent_id */

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_negative_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "parent_id", "-1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_nonexistent_folder(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_nonexistent_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "parent_id", "9999", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_to_own_descendant(stest_ctx_t *ctx)
{
    /* id=3 (test2) is parent of id=4 (childTest).
     * Moving id=3 under id=4 would create a cycle.
     * Implementation may guard this; if not, DB FK/trigger may catch it. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    targs_flag(a, "parent_id", "4", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_to_same_parent_noop(stest_ctx_t *ctx)
{
    /* id=6 is already under parent 1; move to parent 1 again */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "6", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_move_child_unique_conflict(stest_ctx_t *ctx)
{
    /* id=7 is "childTest" under parent 2.
     * id=6 is "childTest" under parent 1.
     * Move id=7 to parent 1 → (1, "childTest") already exists as id=6
     * → uq_model_folders_child violation */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "7", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_move(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_move_to_parent(&ctx);
    test_move_to_root_conflict(&ctx);
    test_move_to_root(&ctx);
    test_move_id_only(&ctx);
    test_move_missing_id(&ctx);
    test_move_zero_id(&ctx);
    test_move_negative_id(&ctx);
    test_move_missing_parent_flag(&ctx);
    test_move_negative_parent(&ctx);
    test_move_nonexistent_folder(&ctx);
    test_move_nonexistent_parent(&ctx);
    test_move_to_own_descendant(&ctx);
    test_move_to_same_parent_noop(&ctx);
    test_move_child_unique_conflict(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
