#include "test_helpers.h"

#include "internal.h"
#include "model_test_helpers.h"
#include <sqlite3.h>

#define REF_DB "acta_test_ref.db"



/* ── helpers local to this file ──────────────────────────────────── */

static int do_update(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("update", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
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

static void test_update_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "RenamedModel", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* revision count for model 1 was 3 (revs 1,2,3); should now be 5 */
    TEST_EQ(ctx, count_revisions(ctx, 1), 4);
    targs_free(a, &g);
}

static void test_update_backend(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "backend", "openai", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, count_revisions(ctx, 1), 5);
    targs_free(a, &g);
}

static void test_update_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "UpdatedAll", &g);
    targs_flag(a, "description", "full update", &g);
    targs_flag(a, "backend", "openai", &g);
    targs_flag(a, "base_url", "https://api.example.org/v1", &g);
    targs_flag(a, "model_identifier", "example-org/model", &g);
    targs_flag(a, "configuration", "{\"max_tokens\":512}", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* single new revision even though multiple fields changed */
    TEST_EQ(ctx, count_revisions(ctx, 1), 6);
    targs_free(a, &g);
}

static void test_update_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);
    targs_flag(a, "name", "Ghost", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

static void test_update_no_change(stest_ctx_t *ctx)
{
    /* updating with identical values should NOT create a new revision
     * (trigger WHEN guard) */
    int before = count_revisions(ctx, 1);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "test-model", &g);  /* same as current */
    targs_flag(a, "backend", "llamacpp", &g); /* same as current */
    targs_flag(a, "model_identifier", "llama-3-8b-v2", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, count_revisions(ctx, 1), before+1);
    targs_free(a, &g);
}

static void test_update_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_update_folder_id(stest_ctx_t *ctx)
{
    /* move model 5 (root, "test") into folder 1 via update */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, count_revisions(ctx, 5), 2);
    targs_free(a, &g);
}

static void test_update_unique_violation(stest_ctx_t *ctx)
{
    /* rename model 1 to "llama-70b" at root → hits uq_models_root (id 2) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "name", "llama-70b", &g);

    int rc = do_update(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_update_invalid_folder(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "folder_id", "999", &g);

    int rc = do_update(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── closed backend vocabulary (docs/plans/backend_dialects.md §9 #2) ──
 * Checked after "at least one field" and BEFORE the row is fetched, so a
 * rejected update leaves both `models` and `model_revisions` untouched. */

static void test_update_backend_rejected(stest_ctx_t *ctx)
{
    int before = count_revisions(ctx, 1);
    char *argv0[] = { "acta_cli", "model", "update", "1",
                      "--backend", "vllm" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'vllm' (expected llamacpp or openai)");
    TEST(ctx, stest_stdout(ctx)[0] == '\0');
    TEST_EQ(ctx, count_revisions(ctx, 1), before);
}

static void test_update_backend_llama_not_alias(stest_ctx_t *ctx)
{
    int before = count_revisions(ctx, 1);
    char *argv0[] = { "acta_cli", "model", "update", "1",
                      "--backend", "llama" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'llama' (expected llamacpp or openai)");
    TEST_EQ(ctx, count_revisions(ctx, 1), before);
}

static void test_update_backend_openai_ok(stest_ctx_t *ctx)
{
    /* `openai` is the second legal value, and changing it is a model
     * revision: model 3 is seeded 'llamacpp' in the ref fixture and is
     * otherwise untouched by this suite, so exactly one new revision. */
    int before = count_revisions(ctx, 3);
    char *argv0[] = { "acta_cli", "model", "update", "3",
                      "--backend", "openai" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, count_revisions(ctx, 3), before + 1);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_update(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_update_name(&ctx);
    test_update_backend(&ctx);
    test_update_all_fields(&ctx);
    test_update_nonexistent(&ctx);
    test_update_no_change(&ctx);
    test_update_empty_name(&ctx);
    test_update_folder_id(&ctx);
    test_update_unique_violation(&ctx);
    test_update_invalid_folder(&ctx);
    test_update_backend_rejected(&ctx);
    test_update_backend_llama_not_alias(&ctx);
    test_update_backend_openai_ok(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
