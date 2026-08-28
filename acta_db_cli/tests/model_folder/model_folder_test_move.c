#include "../skill/skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helper ───────────────────────────────────────────────────────── */

static int do_move(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("move", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ──────────────────────────────────────────────────────────
 *
 *  Reference-DB state (model_folders):
 *
 *    id  name       parent
 *    ──  ────────   ──────
 *     1  default    NULL
 *     2  test       NULL
 *     3  test2      NULL
 *     4  childTest   3
 *     5  childTest   NULL   ← blocks any 'childTest' → root
 *     6  childTest   1
 *     7  childTest   2
 *     8  childTest   6
 *
 *  Unique constraints:
 *    uq_model_folders_root   (name)         WHERE parent_id IS NULL
 *    uq_model_folders_child  (parent_id,name) WHERE parent_id IS NOT NULL
 *
 *  ── STATE LOG (updated after each test) ──────────────────────────
 *  Start :  1→NULL  2→NULL  3→NULL  4→3  5→NULL  6→1  7→2  8→6
 *  T1    :  3→2
 *  T2    :  3→NULL
 *  T3    :  2→3
 *  T4-T13:  (no state change — errors or no-ops)
 *  ───────────────────────────────────────────────────────────────────
 */

/* T1 ── move root folder under a parent ──────────────────────────── */
static void test_move_to_parent(stest_ctx_t *ctx)
{
    /* id=3 'test2' (root) → parent 2.
     * No 'test2' exists under parent 2 → safe.
     *
     * State after: 3's parent = 2 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    targs_flag(a, "parent_id", "2", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":2");
    targs_free(a, &g);
}

/* T2 ── move back to root ─────────────────────────────────────────── */
static void test_move_to_root(stest_ctx_t *ctx)
{
    /* id=3 'test2' is now under parent 2 (from T1).
     * Move back to root. 'test2' is the only folder with that name
     * anywhere → no uq_model_folders_root conflict.
     *
     * State after: 3's parent = NULL */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":null");
    targs_free(a, &g);
}

/* T3 ── --id_only output ──────────────────────────────────────────── */
static void test_move_id_only(stest_ctx_t *ctx)
{
    /* id=2 'test' (root) → parent 3.
     * 'test' does not exist under parent 3 (only 'childTest'/id=4) → safe.
     *
     * State after: 2's parent = 3 */
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "parent_id", "3", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "2");
    targs_free(a, &g);
}

/* T4 ── missing positional <id> ──────────────────────────────────── */
static void test_move_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* T5 ── id = 0 ────────────────────────────────────────────────────── */
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

/* T6 ── negative id ───────────────────────────────────────────────── */
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

/* T7 ── missing --parent_id flag ──────────────────────────────────── */
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

/* T8 ── negative --parent_id ──────────────────────────────────────── */
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

/* T9 ── folder id does not exist ──────────────────────────────────── */
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

/* T10 ── target parent does not exist (FK) ────────────────────────── */
static void test_move_nonexistent_parent(stest_ctx_t *ctx)
{
    /* id=1 exists; parent 9999 does not → FK violation in DB layer */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "parent_id", "9999", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* T11 ── cycle: move a folder under its own descendant ────────────── */
static void test_move_to_own_descendant(stest_ctx_t *ctx)
{
    /* id=6 (parent=1) has child id=8 (parent=6).
     * Moving 6 under 8 would create cycle 6→8→6.
     * Expect the DB layer or app to reject this.
     *
     * No state change on failure. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "6", &g);
    targs_flag(a, "parent_id", "8", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* T12 ── no-op: move to the parent it already has ─────────────────── */
static void test_move_same_parent_noop(stest_ctx_t *ctx)
{
    /* id=7 'childTest' is under parent 2 (unchanged so far).
     * Moving to parent 2 again → idempotent, should succeed.
     *
     * No effective state change. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "7", &g);
    targs_flag(a, "parent_id", "2", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* T13 ── unique-child conflict ────────────────────────────────────── */
static void test_move_unique_child_conflict(stest_ctx_t *ctx)
{
    /* id=4 'childTest' is under parent 3.
     * id=7 'childTest' is under parent 2.
     * Moving id=4 → parent 2 would give two 'childTest' under 2
     *   → violates uq_model_folders_child.
     *
     * No state change on failure. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);
    targs_flag(a, "parent_id", "2", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* T14 ── unique-root conflict (the original bug) ──────────────────── */
static void test_move_unique_root_conflict(stest_ctx_t *ctx)
{
    /* id=7 'childTest' is under parent 2.
     * id=5 'childTest' is at root (parent NULL).
     * Moving id=7 → root (0) → two 'childTest' at root
     *   → violates uq_model_folders_root.
     *
     * This is the scenario that broke the original test.
     * We now *expect* failure.
     *
     * No state change on failure. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "7", &g);
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_move(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    /* mutating (change parent_id) */
    test_move_to_parent(&ctx);            /* 3 → 2  */
    test_move_to_root(&ctx);              /* 3 → NULL */
    test_move_id_only(&ctx);              /* 2 → 3  */

    /* validation errors (no state change) */
    test_move_missing_id(&ctx);
    test_move_zero_id(&ctx);
    test_move_negative_id(&ctx);
    test_move_missing_parent_flag(&ctx);
    test_move_negative_parent(&ctx);
    test_move_nonexistent_folder(&ctx);
    test_move_nonexistent_parent(&ctx);

    /* constraint / domain errors (no state change) */
    test_move_to_own_descendant(&ctx);
    test_move_same_parent_noop(&ctx);
    test_move_unique_child_conflict(&ctx);
    test_move_unique_root_conflict(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
