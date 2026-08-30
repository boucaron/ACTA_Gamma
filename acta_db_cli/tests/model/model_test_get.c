#include "test_helpers.h"

#include <sqlite3.h>
#include "internal.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_get(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("get", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_get_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "test-model");
    targs_free(a, &g);
}

static void test_get_with_folder(stest_ctx_t *ctx)
{
    /* id=4 is llama-70b in folder 1 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "llama-70b");
    targs_free(a, &g);
}

static void test_get_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "98999", &g);

    int rc = do_get(ctx, a, g);
    /* not found → EXIT_NOT_FOUND + JSON error on stderr (P3) */
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    targs_free(a, &g);
}

static void test_get_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_get_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("name,backend");
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "test-model");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "llamacpp");
    /* should NOT contain other columns */
    TEST(ctx, !strstr(stest_stdout(ctx), "base_url"));
    targs_free(a, &g);
}

static void test_get_deleted_model(stest_ctx_t *ctx)
{
    /* seed: soft-delete id=5, then try to get it */
    sqlite3_exec(ctx->db->handle,
        "UPDATE models SET deleted_at = datetime('now') WHERE id = 5",
        NULL, NULL, NULL);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);

    int rc = do_get(ctx, a, g);
    /* plain get has no deleted filter (only --live does), so the deleted
     * row is returned → EXIT_OK */
    TEST(ctx, rc == EXIT_OK);
    targs_free(a, &g);
}

static void test_get_json_output(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_json();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_get(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "llama-70b");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_get(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_get_basic(&ctx);
    test_get_with_folder(&ctx);
    test_get_nonexistent(&ctx);
    test_get_id_only(&ctx);
    test_get_fields(&ctx);
    test_get_deleted_model(&ctx);
    test_get_json_output(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
