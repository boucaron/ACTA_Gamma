#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helper ───────────────────────────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model_folder("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "MyNewFolder", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_with_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "SubFolder", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_parent_zero_means_root(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "RootViaZero", &g);
    targs_flag(a, "parent_id", "0", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "IdOnlyMF", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --name */
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_negative_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NegParent", &g);
    targs_flag(a, "parent_id", "-1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_root_unique_violation(stest_ctx_t *ctx)
{
    /* "test" already exists at root (id=2 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "test", &g);

    int rc = do_create(ctx, a, g);
    /* uq_model_folders_root on (name) WHERE parent_id IS NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_child_unique_violation(stest_ctx_t *ctx)
{
    /* "childTest" already exists under parent 3 (id=4 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "childTest", &g);
    targs_flag(a, "parent_id", "3", &g);

    int rc = do_create(ctx, a, g);
    /* uq_model_folders_child on (parent_id, name) WHERE parent_id IS NOT NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_same_name_different_parent_ok(stest_ctx_t *ctx)
{
    /* "childTest" exists under parents 1, 2, 3, 6.
     * Create "childTest" under parent 7 → should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "childTest", &g);
    targs_flag(a, "parent_id", "7", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_nonexistent_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "Orphan", &g);
    targs_flag(a, "parent_id", "999", &g);

    int rc = do_create(ctx, a, g);
    /* FK violation: parent_id 999 does not exist */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── input-source regression (P2) ───────────────────────────────────
 *  --json <blob> (space + '=' form), --from_file (present + missing),
 *  --stdin, and conflicting sources — run through parse_globals +
 *  handler exactly like main.c does (stest_run_argv). */

static void test_create_json_invalid(stest_ctx_t *ctx)
{
    /* --json <garbage> → EXIT_INVALID (the blob is used verbatim;
     * stdin is not read). */
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--json", "this is not json" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_space(stest_ctx_t *ctx)
{
    /* --json <blob> (space form): the flag value must be honoured
     * (P2: it used to be ignored and stdin read instead). */
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--json", "{\"name\":\"SrcJsonSpace\"}" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_json_equals(stest_ctx_t *ctx)
{
    /* --json=<blob> (equals form) */
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--json={\"name\":\"SrcJsonEquals\"}" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_present(stest_ctx_t *ctx)
{
    const char *path = stest_write_input(ctx, "{\"name\":\"SrcFromFile\"}");
    TEST_NOT_NULL(ctx, path);
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--from_file", (char *)path };
    int rc = stest_run_argv(ctx, cmd_model_folder, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_missing(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--from_file", "./tmp/acta_no_such_input.json" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_folder", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 4, argv0,
        "{\"name\":\"SrcStdin\"}");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_conflict_json_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--json", "{}", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_json_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--json", "{}", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_stdin_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model_folder", "create",
                      "--stdin", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_model_folder, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_folder_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_with_parent(&ctx);
    test_create_parent_zero_means_root(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_name(&ctx);
    test_create_empty_name(&ctx);
    test_create_negative_parent(&ctx);
    test_create_root_unique_violation(&ctx);
    test_create_child_unique_violation(&ctx);
    test_create_same_name_different_parent_ok(&ctx);
    test_create_nonexistent_parent(&ctx);
    test_create_json_invalid(&ctx);
    test_create_src_json_space(&ctx);
    test_create_src_json_equals(&ctx);
    test_create_src_from_file_present(&ctx);
    test_create_src_from_file_missing(&ctx);
    test_create_src_stdin(&ctx);
    test_create_src_conflict_json_stdin(&ctx);
    test_create_src_conflict_json_file(&ctx);
    test_create_src_conflict_stdin_file(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
