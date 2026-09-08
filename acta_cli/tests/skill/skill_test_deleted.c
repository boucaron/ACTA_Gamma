/*
 * skill_test_deleted.c — tests for the soft-delete flag surface on
 * skill get / list / count:
 *
 *  - `--include_deleted` on skill get / list / count
 *  - `--deleted` as a documented alias of `--include_deleted`
 *    (normalised to the canonical name in apply_flag_aliases BEFORE
 *    dispatch, so it works uniformly for get / list / count)
 *  - strict pass-2 flag validation: unknown long options must fail
 *    with EXIT_CLI (10; T2: CLI-usage error, ACTA_CLI_ERR / code -10)
 *    instead of being silently ignored (the old behaviour let a typo'd
 *    filter flag through and produced a plausible-looking but wrong
 *    result set).
 *
 * Reference-DB layout (acta_test_ref.db):
 *   skills: id=1 (folder 2), id=2 (root, "tata"), id=3 (folder 1),
 *           id=4 (folder 2), id=5 (root, "summarize2"),
 *           id=6 (folder 2), id=7 (folder 2) — all live.
 *
 * Note: skill list defaults to root-level rows; --all widens to every
 * folder.  All cases run through parse_globals + validation + handler
 * exactly like main.c does (stest_run_argv).
 */
#include "test_helpers.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "internal.h"   /* complete db_t (sqlite3 *handle) */

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

/* Soft-delete a reference-DB row so list/count/get have a deleted row
 * to include. Idempotent: re-seeding an already-deleted row is a
 * no-op for the assertions. */
static void seed_deleted(stest_ctx_t *ctx, int id)
{
    char sql[96];
    snprintf(sql, sizeof sql,
             "UPDATE skills SET deleted_at = datetime('now') WHERE id = %d",
             id);
    sqlite3_exec(ctx->db->handle, sql, NULL, NULL, NULL);
}

/* ── list ─────────────────────────────────────────────────────────── */

static void test_list_default_excludes_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    /* Root list (default scope): id=2 live, id=5 deleted → hidden */
    char *argv0[] = { "acta_cli", "skill", "list" };
    int rc = stest_run_argv(ctx, cmd_skill, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":2,");     /* id=2, live (root) */
    TEST(ctx, !strstr(out, "\"id\":5,"));    /* id=5, deleted → hidden */

    /* --all scope: the deleted root row is hidden too */
    char *argv1[] = { "acta_cli", "skill", "list", "--all" };
    rc = stest_run_argv(ctx, cmd_skill, 4, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "\"id\":5,"));
}

static void test_list_include_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "skill", "list", "--all",
                      "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":5,");
}

static void test_list_folder_include_deleted(stest_ctx_t *ctx)
{
    /* Folder-scoped with-deleted lister: deleted row inside folder 2 */
    seed_deleted(ctx, 1);   /* summarize_v2, folder 2 */

    char *argv0[] = { "acta_cli", "skill", "list",
                      "--folder_id", "2", "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_skill, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1,");
    TEST_CONTAINS(ctx, out, "deleted_at");

    /* Without the flag the folder list hides the deleted row */
    char *argv1[] = { "acta_cli", "skill", "list",
                      "--folder_id", "2" };
    rc = stest_run_argv(ctx, cmd_skill, 5, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST(ctx, !strstr(stest_stdout(ctx), "\"id\":1,"));
}

static void test_list_deleted_alias(stest_ctx_t *ctx)
{
    /* Regression: `skill list --deleted` used to be silently ignored,
     * so the list exited 0 but dropped the soft-deleted rows. */
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "skill", "list", "--all",
                      "--deleted" };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":5,");
}

static void test_list_deleted_alias_equals(stest_ctx_t *ctx)
{
    /* `=` form of the alias: the rewrite must still apply. */
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "skill", "list", "--all",
                      "--deleted=1" };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":5,");
}

static void test_list_deleted_alias_with_count_short_circuit(stest_ctx_t *ctx)
{
    /* --count (global) + --deleted: count_all_with_deleted */
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "skill", "list", "--all",
                      "--deleted", "--count" };
    int rc = stest_run_argv(ctx, cmd_skill, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 7);  /* 6 live + 1 deleted */
}

/* ── count ────────────────────────────────────────────────────────── */

static void test_count_deleted_alias(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "skill", "count", "--all" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 6);  /* live only */

    char *argv1[] = { "acta_cli", "skill", "count", "--all",
                      "--deleted" };
    rc = stest_run_argv(ctx, cmd_skill, 5, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 7);  /* live + deleted */
}

/* ── get ──────────────────────────────────────────────────────────── */

static void test_get_deleted_alias(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    /* Without the flag: deleted rows are invisible */
    char *argv0[] = { "acta_cli", "skill", "get", "5" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0, "");
    TEST(ctx, rc != EXIT_OK);

    /* With the alias: the row comes back */
    char *argv1[] = { "acta_cli", "skill", "get", "5", "--deleted" };
    rc = stest_run_argv(ctx, cmd_skill, 5, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "summarize2");
}

/* ── strict unknown-flag validation ───────────────────────────────── */

static void test_unknown_flag_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "skill", "list", "--deletd" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_CLI);
}

static void test_unknown_flag_typo_close_to_real(stest_ctx_t *ctx)
{
    /* One missing char: the exact class of bug that used to silently
     * filter. */
    char *argv0[] = { "acta_cli", "skill", "list", "--include_delete" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_CLI);
}

static void test_unknown_flag_equals_form_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "skill", "list", "--no_such_opt=1" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_CLI);
}

static void test_known_flags_still_accepted(stest_ctx_t *ctx)
{
    /* Positive control: the validation must not reject the real
     * vocabulary. */
    char *argv0[] = { "acta_cli", "skill", "list",
                      "--folder_id", "1", "--offset", "0",
                      "--limit", "5" };
    int rc = stest_run_argv(ctx, cmd_skill, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_test_deleted(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_default_excludes_deleted(&ctx);
    test_list_include_deleted(&ctx);
    test_list_deleted_alias(&ctx);
    test_list_deleted_alias_equals(&ctx);
    test_list_deleted_alias_with_count_short_circuit(&ctx);
    test_count_deleted_alias(&ctx);
    test_get_deleted_alias(&ctx);

    /* seeds a second deleted row (id=1) — keep after count/get tests,
     * whose 6/7 expectations assume only id=5 is deleted */
    test_list_folder_include_deleted(&ctx);

    test_unknown_flag_rejected(&ctx);
    test_unknown_flag_typo_close_to_real(&ctx);
    test_unknown_flag_equals_form_rejected(&ctx);
    test_known_flags_still_accepted(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
