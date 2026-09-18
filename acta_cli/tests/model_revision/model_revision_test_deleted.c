/*
 * model_revision_test_deleted.c — soft-delete flag surface (KI-4):
 *
 *  - default `model_revision list` / `count` are live-only
 *    (deleted_at IS NULL), consistent with context/exec/model/skill
 *  - `--include_deleted` (and the `--deleted` alias, normalised in
 *    apply_flag_aliases before dispatch) opts back into soft-deleted
 *    rows — works on list, count, `list --count` and `--stream`
 *
 * All cases run through parse_globals + validation + handler exactly
 * like main.c does (stest_run_argv).
 *
 * Ref-DB facts used: model 1 has revisions 1,2,3 where rev 3 (id 3)
 * is soft-deleted → live count = 2, with-deleted count = 3.
 */
#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── list ─────────────────────────────────────────────────────────── */

static void test_list_default_excludes_deleted(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_revision", "list", "1" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"revision\":1");
    TEST_CONTAINS(ctx, out, "\"revision\":2");
    TEST(ctx, !strstr(out, "\"revision\":3"));   /* deleted → hidden */
}

static void test_list_include_deleted(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_revision", "list", "1",
                      "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":3");
}

static void test_list_deleted_alias(stest_ctx_t *ctx)
{
    /* Regression: the flag used to be accepted as a no-op. */
    char *argv0[] = { "acta_cli", "model_revision", "list", "1", "--deleted" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":3");
}

static void test_list_deleted_alias_equals(stest_ctx_t *ctx)
{
    /* `=` form of the alias: the rewrite must still apply. */
    char *argv0[] = { "acta_cli", "model_revision", "list", "1",
                      "--deleted=1" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":3");
}

static void test_list_deleted_alias_with_count_short_circuit(stest_ctx_t *ctx)
{
    /* --count (global) + --deleted: live-only count would say 2. */
    char *argv0[] = { "acta_cli", "model_revision", "list", "1",
                      "--deleted", "--count" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "3\n");   /* 2 live + 1 deleted */
}

static void test_list_stream_include_deleted(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_revision", "list", "1",
                      "--stream", "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"revision\":3");
}

/* ── count ────────────────────────────────────────────────────────── */

static void test_count_default_live_only(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_revision", "count", "1" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");   /* live only */
}

static void test_count_include_deleted(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_revision", "count", "1",
                      "--include_deleted" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "3\n");   /* live + deleted */
}

static void test_count_deleted_alias(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_revision", "count", "1",
                      "--deleted" };
    int rc = stest_run_argv(ctx, cmd_model_revision, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "3\n");
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_revision_test_deleted(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_default_excludes_deleted(&ctx);
    test_list_include_deleted(&ctx);
    test_list_deleted_alias(&ctx);
    test_list_deleted_alias_equals(&ctx);
    test_list_deleted_alias_with_count_short_circuit(&ctx);
    test_list_stream_include_deleted(&ctx);
    test_count_default_live_only(&ctx);
    test_count_include_deleted(&ctx);
    test_count_deleted_alias(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
