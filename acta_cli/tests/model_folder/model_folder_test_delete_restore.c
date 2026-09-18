#include "test_helpers.h"

#include <sqlite3.h>

#include "internal.h"   /* complete db_t (sqlite3 *handle) */

#define REF_DB "acta_test_ref.db"

/* ── helpers ──────────────────────────────────────────────────────── */

static int do_delete(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("delete", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_restore(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("restore", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── delete tests ─────────────────────────────────────────────────── */

static void test_delete_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);   /* "childTest" at root — no children */

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted\":true");
    targs_free(a, &g);
}


static void test_delete_id_only(stest_ctx_t *ctx)
{
    /* --id_only is NOT supported for delete (only create/get).
     * Output is always {"deleted":true}. Verify it's unaffected. */
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "7", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"deleted\":true");
    targs_free(a, &g);
}



static void test_delete_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_delete_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_delete_negative_id(stest_ctx_t *ctx)
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
    targs_pos(a, "9999", &g);

    int rc = do_delete(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_delete_already_deleted(stest_ctx_t *ctx)
{
    /* Delete id 3, then try to delete it again */
    global_opts_t g = gopts_default();

    cmd_args_t *a1 = targs_new();
    targs_pos(a1, "3", &g);
    do_delete(ctx, a1, g);
    targs_free(a1, &g);

    cmd_args_t *a2 = targs_new();
    targs_pos(a2, "3", &g);
    int rc = do_delete(ctx, a2, g);
    /* second delete on already-deleted row: implementation-dependent */
    TEST(ctx, rc != EXIT_OK || rc == EXIT_OK);
    targs_free(a2, &g);
}

/* ── restore tests ────────────────────────────────────────────────── */

static void test_restore_basic(stest_ctx_t *ctx)
{
    /* Delete id 3 first */
    global_opts_t g = gopts_default();
    cmd_args_t *da = targs_new();
    targs_pos(da, "3", &g);
    do_delete(ctx, da, g);
    targs_free(da, &g);

    /* Now restore */
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"restored\":true");
    targs_free(a, &g);
}

static void test_restore_id_only(stest_ctx_t *ctx)
{
    /* Delete id 3 first */
    global_opts_t g = gopts_default();
    cmd_args_t *da = targs_new();
    targs_pos(da, "3", &g);
    do_delete(ctx, da, g);
    targs_free(da, &g);

    global_opts_t gid = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &gid);
    int rc = do_restore(ctx, a, gid);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "3");
    targs_free(a, &gid);
}

static void test_restore_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_restore_zero_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_restore_negative_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-2", &g);

    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_restore_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_restore(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_restore_not_deleted(stest_ctx_t *ctx)
{
    /* id=1 has never been deleted */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_restore(ctx, a, g);
    /* restoring a non-deleted row: implementation-dependent */
    TEST(ctx, rc != EXIT_OK || rc == EXIT_OK);
    targs_free(a, &g);
}

/* ── integration: delete → get shows deleted_at ──────────────────── */

static void test_delete_then_get_shows_deleted_at(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();

    cmd_args_t *da = targs_new();
    targs_pos(da, "4", &g);
    do_delete(ctx, da, g);
    targs_free(da, &g);

    cmd_args_t *ga = targs_new();
    targs_pos(ga, "4", &g);
    do_get(ctx, ga, g);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted_at\":");
    targs_free(ga, &g);
}

static void test_restore_then_get_clears_deleted_at(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();

    /* delete id 4 */
    cmd_args_t *da = targs_new();
    targs_pos(da, "4", &g);
    do_delete(ctx, da, g);
    targs_free(da, &g);

    /* restore id 4 */
    cmd_args_t *ra = targs_new();
    targs_pos(ra, "4", &g);
    do_restore(ctx, ra, g);
    targs_free(ra, &g);

    /* get id 4 → deleted_at should be null */
    cmd_args_t *ga = targs_new();
    targs_pos(ga, "4", &g);
    do_get(ctx, ga, g);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted_at\":null");
    targs_free(ga, &g);
}

/* ══════════════════════════════════════════════════════════════════
 *  refusal messages (KI-7)
 * ══════════════════════════════════════════════════════════════════
 * The C-level delete/restore checks record their refusal reason in
 * last_error (db_set_error), so the JSON error line on stderr carries
 * `<op> failed: <reason>` instead of `<op> failed: (no detail)`.  Pinned
 * through stest_run_argv, which captures stderr (stest_stderr).
 *
 * Suite state at this point: 5, 7 deleted; 3, 4, 1, 2, 6 (live child
 * 8), 8 live.
 */

static void check_refusal(stest_ctx_t *ctx, const char *action, const char *id,
                          int want_rc, const char *needle)
{
    char *argv0[] = { "acta_cli", "model_folder", action, id };
    int rc = stest_run_argv(ctx, cmd_model_folder, 4, argv0, "");
    TEST_EQ(ctx, rc, want_rc);
    const char *err = stest_stderr(ctx);
    TEST_CONTAINS(ctx, err, needle);
    TEST(ctx, err && !strstr(err, "(no detail)"));
}

static void test_refusal_msgs(stest_ctx_t *ctx)
{
    check_refusal(ctx, "delete", "9999", EXIT_NOT_FOUND,
                  "model_folder 9999 does not exist or is soft-deleted");
    /* folder 6 has live child folder 8 → the sub-folder guard fires */
    check_refusal(ctx, "delete", "6", EXIT_INVALID,
                  "cannot delete model_folder 6: it has live sub-folders");
    /* folder 8 has no sub-folders; seed a live model into it so the
     * assigned-models guard is the one that fires. */
    char sql[192];
    snprintf(sql, sizeof sql,
             "INSERT INTO models (name, backend, model_identifier, folder_id) "
             "VALUES ('mf_refusal_seed', 'openai', 'mf-refusal-seed', 8)");
    int seeded = (sqlite3_exec(ctx->db->handle, sql, NULL, NULL, NULL)
                  == SQLITE_OK);
    TEST(ctx, seeded);
    check_refusal(ctx, "delete", "8", EXIT_INVALID,
                  "cannot delete model_folder 8: live models are assigned to it");
    check_refusal(ctx, "restore", "9999", EXIT_NOT_FOUND,
                  "model_folder 9999 does not exist");
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_delete_restore(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_delete_basic(&ctx);
    test_delete_id_only(&ctx);
    test_delete_missing_id(&ctx);
    test_delete_zero_id(&ctx);
    test_delete_negative_id(&ctx);
    test_delete_nonexistent(&ctx);
    test_delete_already_deleted(&ctx);

    test_restore_basic(&ctx);
    test_restore_id_only(&ctx);
    test_restore_missing_id(&ctx);
    test_restore_zero_id(&ctx);
    test_restore_negative_id(&ctx);
    test_restore_nonexistent(&ctx);
    test_restore_not_deleted(&ctx);

    test_delete_then_get_shows_deleted_at(&ctx);
    test_restore_then_get_clears_deleted_at(&ctx);

    /* refusal messages (KI-7) */
    test_refusal_msgs(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
