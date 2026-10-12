#include "test_helpers.h"

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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    targs_flag(a, "backend", "llamacpp", &g);
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
    char *argv0[] = { "acta_cli", "model", "create",
                      "--json", "this is not json" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_space(stest_ctx_t *ctx)
{
    /* --json <blob> (space form): the flag value must be honoured
     * (P2: it used to be ignored and stdin read instead). */
    char *argv0[] = { "acta_cli", "model", "create",
                      "--json",
                      "{\"name\":\"SrcJsonSpace\",\"backend\":\"llamacpp\","
                      "\"model_identifier\":\"org/src-space\"}" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_json_equals(stest_ctx_t *ctx)
{
    /* --json=<blob> (equals form) */
    char *argv0[] = { "acta_cli", "model", "create",
                      "--json={\"name\":\"SrcJsonEquals\","
                      "\"backend\":\"llamacpp\","
                      "\"model_identifier\":\"org/src-equals\"}" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_present(stest_ctx_t *ctx)
{
    const char *path = stest_write_input(ctx,
        "{\"name\":\"SrcFromFile\",\"backend\":\"llamacpp\","
        "\"model_identifier\":\"org/src-file\"}");
    TEST_NOT_NULL(ctx, path);
    char *argv0[] = { "acta_cli", "model", "create",
                      "--from_file", (char *)path };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_missing(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create",
                      "--from_file", "./tmp/acta_no_such_input.json" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0,
        "{\"name\":\"SrcStdin\",\"backend\":\"llamacpp\","
        "\"model_identifier\":\"org/src-stdin\"}");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_conflict_json_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create",
                      "--json", "{}", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_json_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create",
                      "--json", "{}", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_model, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_stdin_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create",
                      "--stdin", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_model, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ── closed backend vocabulary (docs/plans/backend_dialects.md §9 #2) ──
 * `backend` selects the HTTP dialect acta_runner will speak, so it is no
 * longer free text: only llamacpp and openai are accepted, the check runs
 * before anything is written, and the refusal names the vocabulary. */

static void test_create_backend_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create",
                      "--name", "BadBackend",
                      "--backend", "vllm",
                      "--model_identifier", "org/bad" };
    int rc = stest_run_argv(ctx, cmd_model, 9, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    /* the refusal is a stderr JSON line that states the vocabulary */
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'vllm' (expected llamacpp or openai)");
    /* nothing was created: stdout stays clean */
    TEST(ctx, stest_stdout(ctx)[0] == '\0');
}

static void test_create_backend_llama_not_alias(stest_ctx_t *ctx)
{
    /* §9 #1: `llama` is NOT an alias of `llamacpp` — dialect_from_string
     * matches the canonical names exactly, so this must fail loudly. */
    char *argv0[] = { "acta_cli", "model", "create",
                      "--name", "AliasModel",
                      "--backend", "llama",
                      "--model_identifier", "org/alias" };
    int rc = stest_run_argv(ctx, cmd_model, 9, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'llama' (expected llamacpp or openai)");
}

static void test_create_backend_case_sensitive(stest_ctx_t *ctx)
{
    /* "OpenAI" is not "openai": no guessing, no case folding. */
    char *argv0[] = { "acta_cli", "model", "create",
                      "--name", "CaseModel",
                      "--backend", "OpenAI",
                      "--model_identifier", "org/case" };
    int rc = stest_run_argv(ctx, cmd_model, 9, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'OpenAI' (expected llamacpp or openai)");
}

static void test_create_backend_openai_ok(stest_ctx_t *ctx)
{
    /* The other half of the vocabulary: `openai` is accepted (it is the
     * Strata / OpenAI-compatible dialect). */
    char *argv0[] = { "acta_cli", "model", "create",
                      "--name", "OpenAiDialect",
                      "--backend", "openai",
                      "--model_identifier", "org/openai-ok" };
    int rc = stest_run_argv(ctx, cmd_model, 9, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_backend_json_rejected(stest_ctx_t *ctx)
{
    /* The same gate applies to the JSON input paths, not just flags. */
    char *argv0[] = { "acta_cli", "model", "create",
                      "--json",
                      "{\"name\":\"JsonBadBackend\",\"backend\":\"together\","
                      "\"model_identifier\":\"org/json-bad\"}" };
    int rc = stest_run_argv(ctx, cmd_model, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'together' (expected llamacpp or openai)");
    TEST(ctx, stest_stdout(ctx)[0] == '\0');
}

static void test_create_backend_stdin_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "model", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_model, 4, argv0,
        "{\"name\":\"StdinBadBackend\",\"backend\":\"ollama\","
        "\"model_identifier\":\"org/stdin-bad\"}");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, stest_stderr(ctx),
                  "invalid backend 'ollama' (expected llamacpp or openai)");
    TEST(ctx, stest_stdout(ctx)[0] == '\0');
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
    test_create_backend_rejected(&ctx);
    test_create_backend_llama_not_alias(&ctx);
    test_create_backend_case_sensitive(&ctx);
    test_create_backend_openai_ok(&ctx);
    test_create_backend_json_rejected(&ctx);
    test_create_backend_stdin_rejected(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
