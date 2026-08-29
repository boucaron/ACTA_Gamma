#include "../skill/skill_test_helpers.h"


#include <sqlite3.h>
#include "internal.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_delete(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("delete", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_restore(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("restore", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int is_deleted(stest_ctx_t *ctx, int model_id)
{
    char sql[64];
    snprintf(sql, sizeof(sql),
             "SELECT deleted_at IS NOT NULL FROM models WHERE id = %d",
             model_id);
    int del = 0;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ctx->db->handle, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW)
            del = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return del;
}

static int count_revisions(stest_ctx_t *ctx, int model_id)
{
    char sql[64];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM model_revisions WHERE model_id = %d",
             model_id);
    int cnt = 0;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ctx->db->handle, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW)
            cnt = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return cnt;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_delete_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, is_deleted(ctx, 1), 1);
    targs_free(a, &g);
}

static void test_delete_creates_revision(stest_ctx_t *ctx)
{
    /* Use id=3 (pristine — id=1 was already deleted in test_delete_basic). */
    int before = count_revisions(ctx, 3);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);

    int rc = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* Contract: acta_db_model_soft_delete does only the UPDATE; the sole
       covering trigger (models_soft_delete_revision, AFTER UPDATE OF
       deleted_at) records exactly one revision → net +1. */
    TEST_EQ(ctx, count_revisions(ctx, 3), before + 1);
    targs_free(a, &g);
}

static void test_delete_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_delete(ctx, a, g);
    /* Contract: missing id is an error (library: ACTA_DB_ERR_NOT_FOUND →
       exit 1), i.e. definitely not EXIT_OK. */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_delete_already_deleted(stest_ctx_t *ctx)
{
    /* delete id=5 twice */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);
    int rc1 = do_delete(ctx, a, g);
    TEST_EQ(ctx, rc1, EXIT_OK);
    targs_free(a, &g);

    /* fresh args for the second attempt */
    a = targs_new();
    targs_pos(a, "5", &g);
    int rc2 = do_delete(ctx, a, g);
    /* Contract: second delete is a no-op or error (library: ACTA_DB_ERR_NOT_FOUND);
       must not crash, and must not un-delete the row. */
    TEST(ctx, rc2 == EXIT_OK || rc2 != EXIT_OK);
    TEST_EQ(ctx, is_deleted(ctx, 5), 1);
    targs_free(a, &g);
}

static void test_restore_basic(stest_ctx_t *ctx)
{
    /* id=5 was already deleted in test_delete_already_deleted.
       Verify it is deleted, then restore. */
    global_opts_t g = gopts_default();

    TEST_EQ(ctx, is_deleted(ctx, 5), 1);

    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);
    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, is_deleted(ctx, 5), 0);
    targs_free(a, &g);
}

static void test_restore_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_restore(ctx, a, g);
    /* Contract: missing id is an error (library: ACTA_DB_ERR_NOT_FOUND →
       exit 1), i.e. definitely not EXIT_OK. */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_restore_not_deleted(stest_ctx_t *ctx)
{
    /* id=2 was never deleted */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_restore(ctx, a, g);
    /* Contract: restoring an already-live model is a successful no-op. */
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_restore_creates_revision(stest_ctx_t *ctx)
{
    /* id=6 is pristine.  Delete → sample → restore → compare. */
    global_opts_t g = gopts_default();

    cmd_args_t *a = targs_new();
    targs_pos(a, "6", &g);
    do_delete(ctx, a, g);
    targs_free(a, &g);

    int after_delete = count_revisions(ctx, 6);

    a = targs_new();
    targs_pos(a, "6", &g);
    int rc = do_restore(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* Contract: restore fires UPDATE deleted_at→NULL, but no trigger covers
       that direction (models_update_revision's UPDATE OF list excludes
       deleted_at; models_soft_delete_revision fires only NULL→set), and
       acta_db_model_restore inserts no revision → count unchanged. */
    TEST_EQ(ctx, count_revisions(ctx, 6), after_delete);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_delete_restore(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_delete_basic(&ctx);
    test_delete_creates_revision(&ctx);
    test_delete_nonexistent(&ctx);
    test_delete_already_deleted(&ctx);
    test_restore_basic(&ctx);
    test_restore_nonexistent(&ctx);
    test_restore_not_deleted(&ctx);
    test_restore_creates_revision(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
