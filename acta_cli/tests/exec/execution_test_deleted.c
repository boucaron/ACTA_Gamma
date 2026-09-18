/*
 * execution_test_deleted.c — tests for execution soft delete:
 *
 *  - `delete` from pending / completed / failed / cancelled →
 *    exactly {"deleted":true}; from `running` → exit 4 (INVALID);
 *    on an already-deleted row → exit 1 (NOT_FOUND)
 *  - `restore` leaves the status untouched (a deleted `failed` row
 *    restores to `failed`, and `reset` then succeeds)
 *  - `reset` on a deleted row → exit 1 (DB-layer NOT_FOUND)
 *  - `create --context_id <deleted>` → exit 1
 *  - `list` / `count` are live-only by default; --include_deleted
 *    (and the --deleted alias, normalised in apply_flag_aliases
 *    before dispatch) includes soft-deleted rows
 *
 * Reference-DB state at file start: executions 1..5 all `pending`;
 * contexts 1..7 live.  Every test file runs on its own fresh copy of
 * the ref DB, so the sequencing below is the only shared state.
 */
#include "test_helpers.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "internal.h"   /* complete db_t (sqlite3 *handle) */

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_exec(stest_ctx_t *ctx, const char *action,
                   cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_exec(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int is_deleted(stest_ctx_t *ctx, int id)
{
    char sql[96];
    snprintf(sql, sizeof sql,
             "SELECT deleted_at IS NOT NULL FROM executions WHERE id = %d",
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

/* ── delete per status ────────────────────────────────────────────── */

static void test_delete_from_pending(stest_ctx_t *ctx)
{
    /* id=5 is still 'pending' at this point. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_exec(ctx, "delete", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "{\"deleted\":true}\n");
    TEST_EQ(ctx, is_deleted(ctx, 5), 1);
    targs_free(a, &g);
}

static void test_delete_from_cancelled(stest_ctx_t *ctx)
{
    /* id=2: pending -> cancelled -> deleted */
    global_opts_t g = gopts_default();

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "2", &g);
        int rc = do_exec(ctx, "cancel", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "2", &g);
        int rc = do_exec(ctx, "delete", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_STREQ(ctx, stest_stdout(ctx), "{\"deleted\":true}\n");
        targs_free(a, &g);
    }
}

static void test_delete_from_failed_and_restore_keeps_status(stest_ctx_t *ctx)
{
    /* id=3: pending -> running -> failed -> deleted -> restored.
     * Restore must leave the status `failed` (DB-layer contract), and
     * `reset` must then succeed (failed -> pending). */
    global_opts_t g = gopts_default();

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        int rc = do_exec(ctx, "start", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        targs_flag(a, "error", "boom", &g);
        int rc = do_exec(ctx, "fail", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        int rc = do_exec(ctx, "delete", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_STREQ(ctx, stest_stdout(ctx), "{\"deleted\":true}\n");
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        int rc = do_exec(ctx, "restore", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_STREQ(ctx, stest_stdout(ctx), "{\"id\":3,\"restored\":true}\n");
        TEST_EQ(ctx, is_deleted(ctx, 3), 0);
        targs_free(a, &g);
    }
    {
        /* status survived the delete/restore round trip: still failed */
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        int rc = do_exec(ctx, "get", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"failed\"");
        targs_free(a, &g);
    }
    {
        /* and reset now works (failed -> pending) */
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        int rc = do_exec(ctx, "reset", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_STREQ(ctx, stest_stdout(ctx), "{\"id\":3,\"status\":\"pending\"}\n");
        targs_free(a, &g);
    }
}

static void test_delete_from_completed(stest_ctx_t *ctx)
{
    /* id=4: pending -> running -> completed -> deleted */
    global_opts_t g = gopts_default();

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "4", &g);
        int rc = do_exec(ctx, "start", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "4", &g);
        targs_flag(a, "result", "done", &g);
        int rc = do_exec(ctx, "complete", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "4", &g);
        int rc = do_exec(ctx, "delete", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_STREQ(ctx, stest_stdout(ctx), "{\"deleted\":true}\n");
        targs_free(a, &g);
    }
}

static void test_delete_from_running_rejected(stest_ctx_t *ctx)
{
    /* id=1: pending -> running; delete refused (INVALID → exit 4)
     * and the row must stay LIVE. */
    global_opts_t g = gopts_default();

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "1", &g);
        int rc = do_exec(ctx, "start", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "1", &g);
        int rc = do_exec(ctx, "delete", a, g);
        TEST_EQ(ctx, rc, EXIT_INVALID);
        TEST_EQ(ctx, is_deleted(ctx, 1), 0);
        targs_free(a, &g);
    }
}

static void test_delete_deleted_row_not_found(stest_ctx_t *ctx)
{
    /* id=5 was deleted by test_delete_from_pending. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_exec(ctx, "delete", a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    TEST_EQ(ctx, is_deleted(ctx, 5), 1);
    targs_free(a, &g);
}

/* ── reset / create on deleted rows ───────────────────────────────── */

static void test_reset_deleted_row_not_found(stest_ctx_t *ctx)
{
    /* id=5 is deleted at this point: reset must fail with NOT_FOUND,
     * then restore puts the row back with its status untouched
     * (still `pending`). */
    global_opts_t g = gopts_default();

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "5", &g);
        int rc = do_exec(ctx, "reset", a, g);
        TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "5", &g);
        int rc = do_exec(ctx, "restore", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_STREQ(ctx, stest_stdout(ctx), "{\"id\":5,\"restored\":true}\n");
        targs_free(a, &g);
    }
    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "5", &g);
        int rc = do_exec(ctx, "get", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"pending\"");
        targs_free(a, &g);
    }
}

static void test_create_deleted_context_rejected(stest_ctx_t *ctx)
{
    /* Seed context 7 as soft-deleted, then create an execution
     * pointing at it → NOT_FOUND (exit 1), per the DB-layer contract. */
    char sql[96];
    snprintf(sql, sizeof sql,
             "UPDATE contexts SET deleted_at = datetime('now') "
             "WHERE id = 7");
    sqlite3_exec(ctx->db->handle, sql, NULL, NULL, NULL);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "context_id", "7", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "1", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

/* ── list / count ─────────────────────────────────────────────────── */

/* Deleted at this point: 2 (cancelled) and 4 (completed); live:
 * 1 (running), 3 (pending after reset), 5 (pending after restore). */

static void test_list_default_excludes_deleted(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "list" };
    int rc = stest_run_argv(ctx, cmd_exec, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, strstr(out, "{\"id\":2,") == NULL);  /* deleted → hidden */
    TEST(ctx, strstr(out, "{\"id\":4,") == NULL);  /* deleted → hidden */
    TEST_CONTAINS(ctx, out, "{\"id\":1,");
    TEST_CONTAINS(ctx, out, "{\"id\":3,");
    TEST_CONTAINS(ctx, out, "{\"id\":5,");
}

static void test_list_include_deleted(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "list", "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_exec, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "{\"id\":2,");
    TEST_CONTAINS(ctx, out, "{\"id\":4,");
    TEST_CONTAINS(ctx, out, "\"deleted_at\":\"");
}

static void test_list_deleted_alias(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "list", "--deleted" };
    int rc = stest_run_argv(ctx, cmd_exec, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "{\"id\":2,");
}

static void test_count_live_vs_included(stest_ctx_t *ctx)
{
    /* 5 rows total; 2 and 4 deleted → 3 live */
    char *argv0[] = { "acta_cli", "exec", "count" };
    int rc = stest_run_argv(ctx, cmd_exec, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 3);

    char *argv1[] = { "acta_cli", "exec", "count", "--include_deleted" };
    rc = stest_run_argv(ctx, cmd_exec, 4, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 5);

    char *argv2[] = { "acta_cli", "exec", "count", "--deleted" };
    rc = stest_run_argv(ctx, cmd_exec, 4, argv2, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 5);
}

/* ══════════════════════════════════════════════════════════════════
 *  refusal messages (KI-7)
 * ══════════════════════════════════════════════════════════════════
 * The C-level delete/reset/restore decisions record their refusal
 * reason in last_error (db_set_error), so the JSON error line on
 * stderr carries `<op> failed: <reason>` instead of `(no detail)`.  Pinned
 * through stest_run_argv, which captures stderr (stest_stderr).
 *
 * Suite state at this point: 1 running, 2 deleted (cancelled),
 * 3 pending, 4 deleted (completed), 5 pending.
 */

static void pin_refusal(stest_ctx_t *ctx, const char *action, const char *id,
                        int want_rc, const char *needle)
{
    char *argv0[] = { "acta_cli", "exec", action, id };
    int rc = stest_run_argv(ctx, cmd_exec, 4, argv0, "");
    TEST_EQ(ctx, rc, want_rc);
    const char *err = stest_stderr(ctx);
    TEST_CONTAINS(ctx, err, needle);
    TEST(ctx, err && !strstr(err, "(no detail)"));
}

static void test_refusal_msgs(stest_ctx_t *ctx)
{
    pin_refusal(ctx, "delete", "1", EXIT_INVALID,
                "cannot delete execution 1 while its status is 'running'");
    pin_refusal(ctx, "delete", "2", EXIT_NOT_FOUND,
                "execution 2 is already deleted");
    pin_refusal(ctx, "reset", "2", EXIT_NOT_FOUND,
                "execution 2 is soft-deleted; restore it before reset");
    pin_refusal(ctx, "restore", "1", EXIT_NOT_FOUND,
                "execution 1 is not deleted (nothing to restore)");
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_execution_test_deleted(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_delete_from_pending(&ctx);
    test_delete_from_cancelled(&ctx);
    test_delete_from_failed_and_restore_keeps_status(&ctx);
    test_delete_from_completed(&ctx);
    test_delete_from_running_rejected(&ctx);
    test_delete_deleted_row_not_found(&ctx);
    test_reset_deleted_row_not_found(&ctx);
    test_create_deleted_context_rejected(&ctx);
    test_list_default_excludes_deleted(&ctx);
    test_list_include_deleted(&ctx);
    test_list_deleted_alias(&ctx);
    test_count_live_vs_included(&ctx);

    /* refusal messages (KI-7) */
    test_refusal_msgs(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
