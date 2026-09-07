/*
 * model_test_deleted.c — tests for the soft-delete flag surface:
 *
 *  - `--include_deleted` on model get / list / count
 *  - `--deleted` as a documented alias of `--include_deleted`
 *    (normalised to the canonical name in apply_flag_aliases BEFORE
 *    dispatch, so it works uniformly for get / list / count)
 *  - strict pass-2 flag validation: unknown long options must fail
 *    with EXIT_INVALID instead of being silently ignored (the old
 *    behaviour let a typo'd filter flag through and produced a
 *    plausible-looking but wrong result set).
 *
 * All cases run through parse_globals + validation + handler exactly
 * like main.c does (stest_run_argv).
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
             "UPDATE models SET deleted_at = datetime('now') WHERE id = %d",
             id);
    sqlite3_exec(ctx->db->handle, sql, NULL, NULL, NULL);
}

/* ── list ─────────────────────────────────────────────────────────── */

static void test_list_default_excludes_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "model", "list" };
    int rc = stest_run_argv(ctx, cmd_model, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "test-model");   /* id=1, live */
    TEST(ctx, !strstr(out, "\"id\":5,"));    /* id=5, deleted → hidden */
}

static void test_list_include_deleted(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "model", "list", "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":5,");
}

static void test_list_deleted_alias(stest_ctx_t *ctx)
{
    /* Regression: `model list --deleted` used to be silently ignored,
     * so the list exited 0 but dropped the soft-deleted rows. */
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "model", "list", "--deleted" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":5,");
}

static void test_list_deleted_alias_equals(stest_ctx_t *ctx)
{
    /* `=` form of the alias: the rewrite must still apply. */
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "model", "list", "--deleted=1" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":5,");
}

static void test_list_deleted_alias_with_count_short_circuit(stest_ctx_t *ctx)
{
    /* --count (global) + --deleted: count_all_with_deleted */
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "model", "list", "--deleted", "--count" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 8);  /* 7 live + 1 deleted */
}

/* ── count ────────────────────────────────────────────────────────── */

static void test_count_deleted_alias(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    char *argv0[] = { "acta_cli", "model", "count" };
    int rc = stest_run_argv(ctx, cmd_model, 3, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 7);  /* live only */

    char *argv1[] = { "acta_cli", "model", "count", "--deleted" };
    rc = stest_run_argv(ctx, cmd_model, 4, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, atoi(stest_stdout(ctx)), 8);  /* live + deleted */
}

/* ── get ──────────────────────────────────────────────────────────── */

static void test_get_deleted_alias(stest_ctx_t *ctx)
{
    seed_deleted(ctx, 5);

    /* Without the flag: deleted rows are invisible */
    char *argv0[] = { "acta_cli", "model", "get", "5" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST(ctx, rc != EXIT_OK);

    /* With the alias: the row comes back */
    char *argv1[] = { "acta_cli", "model", "get", "5", "--deleted" };
    rc = stest_run_argv(ctx, cmd_model, 5, argv1, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":5");
}

/* ── strict unknown-flag validation ───────────────────────────────── */

static void test_unknown_flag_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "list", "--deletd" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_unknown_flag_typo_close_to_real(stest_ctx_t *ctx)
{
    /* One missing char: the exact class of bug that used to silently
     * filter. */
    char *argv0[] = { "acta_cli", "model", "list", "--include_delete" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_unknown_flag_equals_form_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "list", "--no_such_opt=1" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_known_flags_still_accepted(stest_ctx_t *ctx)
{
    /* Positive control: the validation must not reject the real
     * vocabulary. */
    char *argv0[] = { "acta_cli", "model", "list",
                      "--folder_id", "1", "--offset", "0",
                      "--limit", "5" };
    int rc = stest_run_argv(ctx, cmd_model, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_deleted(void)
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
    test_unknown_flag_rejected(&ctx);
    test_unknown_flag_typo_close_to_real(&ctx);
    test_unknown_flag_equals_form_rejected(&ctx);
    test_known_flags_still_accepted(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
