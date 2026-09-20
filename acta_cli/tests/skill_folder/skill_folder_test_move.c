#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_move(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("move", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_delete(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("delete", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_move_to_existing_parent(stest_ctx_t *ctx)
{
    /* move folder 2 (child of 1) → root (parent_id 0) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":2");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"parent_id\":null");
    targs_free(a, &g);
}

static void test_move_omitted_parent_defaults_root(stest_ctx_t *ctx)
{
    /* omit --parent_id → defaults to 0 (root) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    /* no --parent_id */

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "{\"id\":2,\"parent_id\":null}\n");
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

static void test_move_invalid_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_invalid_id_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_to_nonexistent_parent(stest_ctx_t *ctx)
{
    /* FK violation: parent doesn't exist */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "parent_id", "99999", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_negative_parent_rejected(stest_ctx_t *ctx)
{
    /* parse_folder_id rejects negatives (typo guard) → EXIT_INVALID;
     * they are NOT clamped to root */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "parent_id", "-5", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_non_numeric_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "parent_id", "abc", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_to_self(stest_ctx_t *ctx)
{
    /* moving a folder under itself would create a cycle → rejected */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_move_to_descendant(stest_ctx_t *ctx)
{
    /* seeded chain: CycleB is a child of CycleA.
     * Moving CycleA under CycleB would create a cycle → rejected.
     * (Seeded locally so the test is independent of ref-DB state.) */
    int a = stest_seed_folder(ctx, "CycleA", 0);
    int b = stest_seed_folder(ctx, "CycleB", a);
    if (a <= 0 || b <= 0) return;  /* skip if seeding failed */

    global_opts_t g = gopts_default();
    cmd_args_t *aargs = targs_new();
    char idbuf[16];
    snprintf(idbuf, sizeof idbuf, "%d", a);
    targs_pos(aargs, idbuf, &g);
    char pbuf[16];
    snprintf(pbuf, sizeof pbuf, "%d", b);
    targs_flag(aargs, "parent_id", pbuf, &g);

    int rc = do_move(ctx, aargs, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(aargs, &g);
}

static void test_move_to_deleted_parent(stest_ctx_t *ctx)
{
    /* target parent must be live: seed a parent, soft-delete it, then
     * try to move another folder under it → NOT_FOUND */
    int p = stest_seed_folder(ctx, "GoneParent", 0);
    int q = stest_seed_folder(ctx, "MoverQ", 0);
    if (p <= 0 || q <= 0) return;  /* skip if seeding failed */

    global_opts_t g = gopts_default();
    cmd_args_t *da = targs_new();
    char pbuf[16];
    snprintf(pbuf, sizeof pbuf, "%d", p);
    targs_pos(da, pbuf, &g);
    int rc = do_delete(ctx, da, g);
    TEST_EQ(ctx, rc, EXIT_OK);   /* no children, no skills → deletable */
    targs_free(da, &g);

    global_opts_t mg = gopts_default();
    cmd_args_t *ma = targs_new();
    char qbuf[16];
    snprintf(qbuf, sizeof qbuf, "%d", q);
    targs_pos(ma, qbuf, &mg);
    targs_flag(ma, "parent_id", pbuf, &mg);

    int mrc = do_move(ctx, ma, mg);
    TEST_EQ(ctx, mrc, EXIT_NOT_FOUND);
    targs_free(ma, &mg);
}

/* ── refusal messages (KI-7) ─────────────────────────────────────── */

/* The C-level move checks record their refusal reason in last_error
 * (db_set_error), so the JSON error line on stderr carries
 * `<op> failed: <reason>` instead of `<op> failed: (no detail)`.  Pinned
 * through stest_run_argv, which captures stderr (stest_stderr).
 *
 * State at this point: 2→NULL (root), 1 live with live child 2; 99999
 * does not exist. */

static void check_move_refusal(stest_ctx_t *ctx, const char *id,
                               const char *parent, int want_rc,
                               const char *needle)
{
    char *argv0[] = { "acta_cli", "skill_folder", "move", (char *)id,
                      "--parent_id", (char *)parent };
    int rc = stest_run_argv(ctx, cmd_skill_folder, 6, argv0, "");
    TEST_EQ(ctx, rc, want_rc);
    const char *err = stest_stderr(ctx);
    TEST_CONTAINS(ctx, err, needle);
    TEST(ctx, err && !strstr(err, "(no detail)"));
}

static void test_move_refusal_msgs(stest_ctx_t *ctx)
{
    check_move_refusal(ctx, "99999", "1", EXIT_NOT_FOUND,
                       "skill_folder 99999 does not exist or is soft-deleted");
    check_move_refusal(ctx, "1", "99999", EXIT_NOT_FOUND,
                       "parent skill_folder 99999 does not exist or is soft-deleted");
    check_move_refusal(ctx, "1", "1", EXIT_INVALID,
                       "cannot move skill_folder 1 into its own subtree: "
                       "parent 1 is a descendant of 1");
}

static void test_move_to_deleted_parent_msg(stest_ctx_t *ctx)
{
    /* message form of test_move_to_deleted_parent: seed a parent,
     * soft-delete it, then move another folder under it. */
    int p = stest_seed_folder(ctx, "RefusalGoneParent", 0);
    int q = stest_seed_folder(ctx, "RefusalMover", 0);
    if (p <= 0 || q <= 0) return;  /* skip if seeding failed */

    char pbuf[16], qbuf[16];
    snprintf(pbuf, sizeof pbuf, "%d", p);
    snprintf(qbuf, sizeof qbuf, "%d", q);

    char *dargv0[] = { "acta_cli", "skill_folder", "delete", pbuf };
    int rc = stest_run_argv(ctx, cmd_skill_folder, 4, dargv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);

    char needle[192];
    snprintf(needle, sizeof needle,
             "parent skill_folder %d does not exist or is soft-deleted", p);
    check_move_refusal(ctx, qbuf, pbuf, EXIT_NOT_FOUND, needle);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_move(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_move_to_existing_parent(&ctx);
    test_move_omitted_parent_defaults_root(&ctx);
    test_move_missing_id(&ctx);
    test_move_invalid_id_zero(&ctx);
    test_move_invalid_id_negative(&ctx);
    test_move_nonexistent(&ctx);
    test_move_to_nonexistent_parent(&ctx);
    test_move_negative_parent_rejected(&ctx);
    test_move_non_numeric_parent(&ctx);
    test_move_to_self(&ctx);
    test_move_to_descendant(&ctx);
    test_move_to_deleted_parent(&ctx);

    /* refusal messages (KI-7) */
    test_move_refusal_msgs(&ctx);
    test_move_to_deleted_parent_msg(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
