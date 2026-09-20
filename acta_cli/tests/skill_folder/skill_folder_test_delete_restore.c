#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_delete(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("delete", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_restore(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("restore", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── delete tests ─────────────────────────────────────────────────── */

static void test_delete_rejected_when_skills_exist(stest_ctx_t *ctx)
{
    /* folder 2 (oauthFlow) has 4 live skills → delete must be rejected */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_delete_rejected_when_children_exist(stest_ctx_t *ctx)
{
    /* folder 1 (authSkill) has live child folder 2 → delete must be
     * rejected until the child is deleted or re-parented */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}


static void test_delete_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_delete_invalid_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_delete_invalid_id_negative(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_delete_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

static void test_delete_rejected_folder_still_listed(stest_ctx_t *ctx)
{
    /* folder 2 has live skills → delete is rejected, folder remains */
    global_opts_t g = gopts_default();
    cmd_args_t *del = targs_new();
    targs_pos(del, "2", &g);
    int rc = do_delete(ctx, del, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(del, &g);

    /* list children of parent 1 → id 2 must still be present */
    global_opts_t lg = gopts_default();
    cmd_args_t *la = targs_new();
    targs_pos(la, "1", &lg);
    stest_capture_begin(ctx);
    (void)cmd_skill_folder("list", la, &lg, ctx->db);
    stest_capture_end(ctx);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":2");
    targs_free(la, &lg);
}


/* ── restore tests ────────────────────────────────────────────────── */

static void test_restore_deleted(stest_ctx_t *ctx)
{
    /* Full lifecycle on a seeded folder (independent of ref-DB state):
     * seed → delete → excluded from count → second delete fails →
     * restore → live again. */
    int fid = stest_seed_folder(ctx, "RestoreMe", 0);
    if (fid <= 0) return;  /* skip if folder creation failed */

    char idbuf[16];
    snprintf(idbuf, sizeof idbuf, "%d", fid);

    global_opts_t g = gopts_default();
    cmd_args_t *da = targs_new();
    targs_pos(da, idbuf, &g);
    int rc = do_delete(ctx, da, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(da, &g);

    /* deleted → back to the 2 baseline folders */
    global_opts_t cg = gopts_default();
    cmd_args_t *ca = targs_new();
    rc = do_count(ctx, ca, cg);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");
    targs_free(ca, &cg);

    /* second delete of the same folder → NOT_FOUND */
    global_opts_t dg = gopts_default();
    cmd_args_t *dd = targs_new();
    targs_pos(dd, idbuf, &dg);
    rc = do_delete(ctx, dd, dg);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(dd, &dg);

    /* restore → live again */
    global_opts_t rg = gopts_default();
    cmd_args_t *ra = targs_new();
    targs_pos(ra, idbuf, &rg);
    rc = do_restore(ctx, ra, rg);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"restored\":true");
    targs_free(ra, &rg);

    /* back to 3 live folders (baseline 2 + RestoreMe) */
    global_opts_t cg2 = gopts_default();
    cmd_args_t *ca2 = targs_new();
    rc = do_count(ctx, ca2, cg2);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "3\n");
    targs_free(ca2, &cg2);
}

static void test_restore_not_deleted(stest_ctx_t *ctx)
{
    /* Restoring a LIVE folder is a hard NOT_FOUND: restore only unflags
     * soft-deleted rows. The refusal reason is recorded in last_error
     * (db_set_error) and pinned here (KI-7); runs through
     * stest_run_argv so stderr is captured. */
    char *argv0[] = { "acta_cli", "skill_folder", "restore", "1" };
    int rc = stest_run_argv(ctx, cmd_skill_folder, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    const char *err = stest_stderr(ctx);
    TEST_CONTAINS(ctx, err, "skill_folder 1 does not exist");
    TEST(ctx, err && !strstr(err, "(no detail)"));
}

static void test_restore_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);

    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

static void test_restore_invalid_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_restore_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ══════════════════════════════════════════════════════════════════
 *  refusal messages (KI-7)
 * ══════════════════════════════════════════════════════════════════
 * The C-level delete/restore checks record their refusal reason in
 * last_error (db_set_error), so the JSON error line on stderr carries
 * `<op> failed: <reason>` instead of `<op> failed: (no detail)`.  Pinned
 * through stest_run_argv, which captures stderr (stest_stderr).
 *
 * Suite state at this point: 1 live (child 2), 2 live with 4 skills;
 * 99999 does not exist.
 */

static void pin_refusal(stest_ctx_t *ctx, const char *action, const char *id,
                        int want_rc, const char *needle)
{
    char *argv0[] = { "acta_cli", "skill_folder", (char *)action, (char *)id };
    int rc = stest_run_argv(ctx, cmd_skill_folder, 4, argv0, "");
    TEST_EQ(ctx, rc, want_rc);
    const char *err = stest_stderr(ctx);
    TEST_CONTAINS(ctx, err, needle);
    TEST(ctx, err && !strstr(err, "(no detail)"));
}

static void test_refusal_msgs(stest_ctx_t *ctx)
{
    pin_refusal(ctx, "delete", "99999", EXIT_NOT_FOUND,
                "skill_folder 99999 does not exist or is soft-deleted");
    /* folder 1 has live child folder 2 → the sub-folder guard fires */
    pin_refusal(ctx, "delete", "1", EXIT_INVALID,
                "cannot delete skill_folder 1: it has live sub-folders");
    /* folder 2 has 4 live skills and no children → the skills guard */
    pin_refusal(ctx, "delete", "2", EXIT_INVALID,
                "cannot delete skill_folder 2: live skills are assigned to it");
    pin_refusal(ctx, "restore", "99999", EXIT_NOT_FOUND,
                "skill_folder 99999 does not exist");
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_delete_restore(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_delete_rejected_when_skills_exist(&ctx);
    test_delete_rejected_when_children_exist(&ctx);
    test_delete_missing_positional(&ctx);
    test_delete_invalid_id_zero(&ctx);
    test_delete_invalid_id_negative(&ctx);
    test_delete_nonexistent(&ctx);
    test_delete_rejected_folder_still_listed(&ctx);
    test_restore_deleted(&ctx);
    test_restore_not_deleted(&ctx);
    test_restore_nonexistent(&ctx);

    /* refusal messages (KI-7) */
    test_refusal_msgs(&ctx);

    test_restore_invalid_id(&ctx);
    test_restore_missing_positional(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
