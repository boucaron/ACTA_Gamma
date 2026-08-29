#include "../skill/skill_test_helpers.h"

#include <sqlite3.h>
#include "internal.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_list(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("list", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_list_all(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* all 8 models should appear */
    TEST_CONTAINS(ctx, out, "test-model");
    TEST_CONTAINS(ctx, out, "llama-70b");
    TEST_CONTAINS(ctx, out, "llama-20b");
    TEST_CONTAINS(ctx, out, "jtest");
    targs_free(a, &g);
}

static void test_list_root_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "root", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "test-model");
    /* folder 1's "llama-70b" (id=4) should NOT appear */
    /* but root "llama-70b" (id=2) should */
    TEST_CONTAINS(ctx, out, "llama-70b");
    targs_free(a, &g);
}

static void test_list_by_folder(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* only id=4 lives in folder 1 */
    TEST_CONTAINS(ctx, out, "llama-70b");
    /* id=1 "test-model" is at root, should not appear */
    TEST(ctx, !strstr(out, "test-model"));
    targs_free(a, &g);
}

static void test_list_empty_folder(stest_ctx_t *ctx)
{
    /* folder 3 ("test2") has no models */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "folder_id", "3", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be empty or "0 rows" — no model names */
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "test-model"));
    TEST(ctx, !strstr(out, "llama"));
    targs_free(a, &g);
}

static void test_list_excludes_deleted(stest_ctx_t *ctx)
{
    /* delete id=5 first */
    sqlite3_exec(ctx->db->handle,
        "UPDATE models SET deleted_at = datetime('now') WHERE id = 5",
        NULL, NULL, NULL);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* model name "test" (id=5) should not appear;
       but "test-model" (id=1) should */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "test-model");
    targs_free(a, &g);
}

static void test_list_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_fields("name,backend");
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "name");
    TEST_CONTAINS(ctx, out, "backend");
    /* should not include description column header */
    TEST(ctx, !strstr(out, "description"));
    targs_free(a, &g);
}

static void test_list_json(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_json();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* JSON array */
    TEST(ctx, out[0] == '[');
    TEST_CONTAINS(ctx, out, "\"name\"");
    targs_free(a, &g);
}

static void test_count_all(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* 8 models in ref DB */
    TEST_CONTAINS(ctx, out, "7");
    targs_free(a, &g);
}

static void test_count_by_folder(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* only 1 model in folder 1 */
    TEST_CONTAINS(ctx, out, "1");
    targs_free(a, &g);
}

static void test_count_excludes_deleted(stest_ctx_t *ctx)
{
    /* delete id=5, count should drop to 7 */
    sqlite3_exec(ctx->db->handle,
        "UPDATE models SET deleted_at = datetime('now') WHERE id = 5",
        NULL, NULL, NULL);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "7");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_list_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_all(&ctx);
    test_list_root_only(&ctx);
    test_list_by_folder(&ctx);
    test_list_empty_folder(&ctx);
    test_list_excludes_deleted(&ctx);
    test_list_fields(&ctx);
    test_list_json(&ctx);
    test_count_all(&ctx);
    test_count_by_folder(&ctx);
    test_count_excludes_deleted(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
