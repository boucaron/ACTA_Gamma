/*
 * context_test_deleted.c — tests for context soft delete:
 *
 *  - `delete` / `restore` round trip with the exact wire bytes
 *    ({"deleted":true} / {"id":N,"restored":true})
 *  - delete of a deleted context → exit 1; restore of a LIVE
 *    context → exit 1 (strict restore, DB-layer contract)
 *  - `get` on a deleted context → exit 1 by default;
 *    --include_deleted (alias --deleted) → JSON with deleted_at set
 *  - `list` / `count` are live-only by default; --include_deleted
 *    (and the --deleted alias, normalised in apply_flag_aliases
 *    before dispatch) includes soft-deleted rows
 *
 * All flag cases run through parse_globals + validation + handler
 * exactly like main.c does (stest_run_argv); the delete/restore
 * cases go through the handler directly, like the other per-module
 * tests.
 */
#include "test_helpers.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "internal.h"   /* complete db_t (sqlite3 *handle) */

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_ctx(stest_ctx_t *ctx, const char *action,
                  cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int is_deleted(stest_ctx_t *ctx, int id)
{
    char sql[96];
    snprintf(sql, sizeof sql,
             "SELECT deleted_at IS NOT NULL FROM contexts WHERE id = %d",
             id);
    int del = 0;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ctx->db->handle, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW)
            del = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return del;
}

/* Soft-delete a reference-DB row so list/count/get have a deleted row
 * to include. Idempotent: re-seeding an already-deleted row is a
 * no-op for the assertions. */
static void seed_deleted(stest_ctx_t *ctx, int id)
{
    char sql[96];
    snprintf(sql, sizeof sql,
             "UPDATE contexts SET deleted_at = datetime('now') "
             "WHERE id = %d",
             id);
    sqlite3_exec(ctx->db->handle, sql, NULL, NULL, NULL);
}

/* ── delete ───────────────────────────────────────────────────────── */

static void test_delete_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);

    int rc = do_ctx(ctx, "delete", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "{\"deleted\":true}\n");
    TEST_EQ(ctx, is_deleted(ctx, 3), 1);
    targs_free(a, &g);
}

static void test_delete_already_deleted(stest_ctx_t *ctx)
{
    /* id=3 was deleted by test_delete_basic. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);

    int rc = do_ctx(ctx, "delete", a, g);
    /* Contract: delete-of-deleted is NOT_FOUND at the DB layer → exit 1,
     * and the row stays deleted. */
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    TEST_EQ(ctx, is_deleted(ctx, 3), 1);
    targs_free(a, &g);
}

static void test_delete_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_ctx(ctx, "delete", a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

/* ── restore ──────────────────────────────────────────────────────── */

static void test_restore_round_trip(stest_ctx_t *ctx)
{
    /* id=3 was deleted by the delete tests above. */
    TEST_EQ(ctx, is_deleted(ctx, 3), 1);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);

    int rc = do_ctx(ctx, "restore", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "{\"id\":3,\"restored\":true}\n");
    TEST_EQ(ctx, is_deleted(ctx, 3), 0);
    targs_free(a, &g);
}

static void test_restore_live_row_rejected(stest_ctx_t *ctx)
{
    /* id=1 was never deleted: strict restore → NOT_FOUND → exit 1. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_ctx(ctx, "restore", a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

static void test_restore_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_ctx(ctx, "restore", a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

/* ── get ──────────────────────────────────────────────────────────── */

static void test_get_deleted_hidden_by_default(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "get", "3" };
    int rc = stest_run_argv(ctx, cmd_context, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
}

static void test_get_deleted_with_include_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "get", "3",
                      "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":3");
    TEST_CONTAINS(ctx, out, "\"deleted_at\":\"");
}

static void test_get_deleted_with_alias(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "get", "3", "--deleted" };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted_at\":\"");
}

/* ── list / count ─────────────────────────────────────────────────── */

static void test_list_default_excludes_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "list" };
    int rc = stest_run_argv(ctx, cmd_context, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, strstr(out, "{\"id\":3,") == NULL);   /* id=3, deleted → hidden */
}

static void test_list_include_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "list", "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_context, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "{\"id\":3,");
    TEST_CONTAINS(ctx, out, "\"deleted_at\":\"");
}

static void test_list_deleted_alias(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "list", "--deleted" };
    int rc = stest_run_argv(ctx, cmd_context, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "{\"id\":3,");
}

static void test_count_live_vs_included(stest_ctx_t *ctx)
{
    /* ref DB: 7 contexts; id=3 seeded deleted → 6 live, 7 total */
    seed_deleted(ctx, 3);

    char *argv0[] = { "acta_cli", "context", "count" };
    int rc = stest_run_argv(ctx, cmd_context, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 6);

    char *argv1[] = { "acta_cli", "context", "count", "--include_deleted" };
    rc = stest_run_argv(ctx, cmd_context, 4, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 7);

    char *argv2[] = { "acta_cli", "context", "count", "--deleted" };
    rc = stest_run_argv(ctx, cmd_context, 4, argv2, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 7);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_deleted(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_delete_basic(&ctx);
    test_delete_already_deleted(&ctx);
    test_delete_nonexistent(&ctx);
    test_restore_round_trip(&ctx);
    test_restore_live_row_rejected(&ctx);
    test_restore_nonexistent(&ctx);
    test_get_deleted_hidden_by_default(&ctx);
    test_get_deleted_with_include_deleted(&ctx);
    test_get_deleted_with_alias(&ctx);
    test_list_default_excludes_deleted(&ctx);
    test_list_include_deleted(&ctx);
    test_list_deleted_alias(&ctx);
    test_count_live_vs_included(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
