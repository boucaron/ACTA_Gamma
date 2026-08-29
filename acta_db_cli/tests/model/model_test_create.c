#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_model("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    /* minimal required: name, backend, model_identifier */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "CreateBasic", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "org/model-x", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "FullModel", &g);
    targs_flag(a, "backend", "llamacpp", &g);
    targs_flag(a, "model_identifier", "local/full-model", &g);
    targs_flag(a, "folder_id", "2", &g);
    targs_flag(a, "description", "A full model", &g);
    targs_flag(a, "base_url", "http://localhost:9999/v1", &g);
    targs_flag(a, "configuration", "{\"temp\":0.7}", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "IdOnlyModel", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "org/idonly", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be a bare integer + newline, no braces */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "org/x", &g);
    /* no --name */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_backend(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NoBackend", &g);
    targs_flag(a, "model_identifier", "org/x", &g);
    /* no --backend */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_identifier(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NoIdentifier", &g);
    targs_flag(a, "backend", "vllm", &g);
    /* no --model_identifier */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "org/x", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_root_unique_violation(stest_ctx_t *ctx)
{
   
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "test-model", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "dupe", &g);

    int rc = do_create(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    
    targs_free(a, &g);
}

static void test_create_folder_unique_violation(stest_ctx_t *ctx)
{
    /* "llama-70b" already exists in folder 1 (id=4 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "llama-70b", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "meta/llama-70b", &g);
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_same_name_different_folder_ok(stest_ctx_t *ctx)
{
    /* "llama-70b" exists at root (id=2) and in folder 1 (id=4).
     * Create "llama-70b" in folder 2 → should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "llama-70b", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "meta/llama-70b", &g);
    targs_flag(a, "folder_id", "2", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_negative_folder_clamped(stest_ctx_t *ctx)
{
    /* code clamps folder_id < 0 → root (NULL) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NegFolderModel", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "org/neg", &g);
    targs_flag(a, "folder_id", "-3", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID); // Fix invalid folder_id : {"error":"ACTA_DB_ERR_INVALID","code":-4,"message":"'folder_id' must be a valid integer"}

    targs_free(a, &g);
}

static void test_create_invalid_folder(stest_ctx_t *ctx)
{
    /* folder_id 999 does not exist → FK violation */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "BadFolder", &g);
    targs_flag(a, "backend", "vllm", &g);
    targs_flag(a, "model_identifier", "org/badfk", &g);
    targs_flag(a, "folder_id", "999", &g);

    int rc = do_create(ctx, a, g);
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
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--json", "this is not json" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_space(stest_ctx_t *ctx)
{
    /* --json <blob> (space form): the flag value must be honoured
     * (P2: it used to be ignored and stdin read instead). */
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--json",
                      "{\"name\":\"SrcJsonSpace\",\"backend\":\"vllm\","
                      "\"model_identifier\":\"org/src-space\"}" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_json_equals(stest_ctx_t *ctx)
{
    /* --json=<blob> (equals form) */
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--json={\"name\":\"SrcJsonEquals\","
                      "\"backend\":\"vllm\","
                      "\"model_identifier\":\"org/src-equals\"}" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_present(stest_ctx_t *ctx)
{
    const char *path = stest_write_input(ctx,
        "{\"name\":\"SrcFromFile\",\"backend\":\"vllm\","
        "\"model_identifier\":\"org/src-file\"}");
    TEST_NOT_NULL(ctx, path);
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--from_file", (char *)path };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_missing(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--from_file", "./tmp/acta_no_such_input.json" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "model", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0,
        "{\"name\":\"SrcStdin\",\"backend\":\"vllm\","
        "\"model_identifier\":\"org/src-stdin\"}");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_conflict_json_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--json", "{}", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_json_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--json", "{}", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_model, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_stdin_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "model", "create",
                      "--stdin", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_name(&ctx);
    test_create_missing_backend(&ctx);
    test_create_missing_identifier(&ctx);
    test_create_empty_name(&ctx);
    test_create_root_unique_violation(&ctx);
    test_create_folder_unique_violation(&ctx);
    test_create_same_name_different_folder_ok(&ctx);
    test_create_negative_folder_clamped(&ctx);
    test_create_invalid_folder(&ctx);
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
