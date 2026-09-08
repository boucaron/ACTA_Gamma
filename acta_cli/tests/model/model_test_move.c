#include "test_helpers.h"

#include "internal.h"
#include "model_test_helpers.h"
#include <sqlite3.h>


#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_move(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("move", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int get_folder_id(stest_ctx_t *ctx, int model_id)
{
    char sql[64];
    snprintf(sql, sizeof(sql),
             "SELECT folder_id FROM models WHERE id = %d", model_id);
    int fid = -1;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ctx->db->handle, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW)
            fid = sqlite3_column_type(st, 0) == SQLITE_NULL
                ? -1
                : sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return fid;
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

static void test_move_to_folder(stest_ctx_t *ctx)
{
    /* move id=3 (llama-20b, root) → folder 2 */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    targs_flag(a, "folder_id", "2", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, get_folder_id(ctx, 3), 2);
    targs_free(a, &g);
}

static void test_move_to_root(stest_ctx_t *ctx)
{
    /* move id=4 (llama-70b, folder 1) → root.
       The ref DB already holds model 2 (name 'llama-70b') at the root and
       the schema enforces a unique name per scope (uq_models_root), so
       rename model 2 first — otherwise the move is bound to violate the
       unique index. */
    sqlite3_exec(ctx->db->handle,
                 "UPDATE models SET name='llama-70b-root' WHERE id = 2",
                 NULL, NULL, NULL);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);
    targs_flag(a, "folder_id", "0", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* folder_id should be NULL (root) */
    TEST_EQ(ctx, get_folder_id(ctx, 4), -1); /* -1 means NULL */
    TEST_STREQ(ctx, stest_stdout(ctx), "{\"id\":4,\"folder_id\":null}\n");
    targs_free(a, &g);
}

static void test_move_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_move(ctx, a, g);
    /* Contract: missing id is an error (library: ACTA_DB_ERR_NOT_FOUND →
       exit 1), i.e. definitely not EXIT_OK. */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_unique_violation(stest_ctx_t *ctx)
{
    /* seed: create a model named "llama-20b" in folder 1 */
    mtest_seed_model(ctx, 1, "llama-20b", "vllm", "meta/llama-20b",
                     NULL, NULL, NULL);

    /* now try to move id=3 (root "llama-20b") into folder 1 → conflict */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_to_nonexistent_folder(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "folder_id", "999", &g);

    int rc = do_move(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_move_creates_revision(stest_ctx_t *ctx)
{
    /* Model 3 is already in folder 2 (test_move_to_folder), so pick a
       different target (folder 3, empty in the ref DB) to guarantee a
       real column change → the models_update_revision trigger records
       exactly one new revision.  (An idempotent move to the same folder
       is filtered out by the trigger's WHEN guard → no revision.) */
    int before = count_revisions(ctx, 3);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);
    targs_flag(a, "folder_id", "3", &g);

    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, count_revisions(ctx, 3), before + 1);
    targs_free(a, &g);
}

static void test_move_already_in_folder(stest_ctx_t *ctx)
{
    /* id=4 is at the root after test_move_to_root; move it into folder 1,
       then to folder 1 again.  The second (idempotent) move changes no
       value but SQLite still reports the row as changed, so the library
       returns EXIT_OK for both. */
    global_opts_t g = gopts_default();

    cmd_args_t *a = targs_new();
    targs_pos(a, "4", &g);
    targs_flag(a, "folder_id", "1", &g);
    int rc = do_move(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);

    a = targs_new();
    targs_pos(a, "4", &g);
    targs_flag(a, "folder_id", "1", &g);
    rc = do_move(ctx, a, g);
    /* idempotent move: still EXIT_OK */
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_move(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_move_to_folder(&ctx);
    test_move_to_root(&ctx);
    test_move_nonexistent(&ctx);
    test_move_unique_violation(&ctx);
    test_move_to_nonexistent_folder(&ctx);
    test_move_creates_revision(&ctx);
    test_move_already_in_folder(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
