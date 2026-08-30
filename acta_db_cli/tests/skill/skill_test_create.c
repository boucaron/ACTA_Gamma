#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    /* seed a new skill with minimal required fields */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "TestSkill", &g);
    targs_flag(a, "prompt_template", "Do {{thing}}", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "FullSkill", &g);
    targs_flag(a, "prompt_template", "Template {{x}}", &g);
    targs_flag(a, "folder_id", "2", &g);
    targs_flag(a, "description", "A full skill", &g);
    targs_flag(a, "output_schema", "{\"type\":\"string\"}", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "IdOnlySkill", &g);
    targs_flag(a, "prompt_template", "x", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be a bare integer + newline, no braces */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    /* should NOT contain '{' */
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --name */
    targs_flag(a, "prompt_template", "orphan prompt", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_prompt(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NoPrompt", &g);
    /* no --prompt_template */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "", &g);
    targs_flag(a, "prompt_template", "has prompt", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_root_unique_violation(stest_ctx_t *ctx)
{
    /* "tata" already exists at root (id=2 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "tata", &g);
    targs_flag(a, "prompt_template", "dupe at root", &g);

    int rc = do_create(ctx, a, g);
    /* should fail: unique index uq_skills_root on (name) WHERE folder_id IS NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_folder_unique_violation(stest_ctx_t *ctx)
{
    /* "tata" already exists in folder 2 (id=4 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "tata", &g);
    targs_flag(a, "prompt_template", "dupe in folder 2", &g);
    targs_flag(a, "folder_id", "2", &g);

    int rc = do_create(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_same_name_different_folder_ok(stest_ctx_t *ctx)
{
    /* "summarize2" exists at root (id=5) and in folder 2 (id=6).
     * Create "summarize2" in folder 1 → should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "summarize2", &g);
    targs_flag(a, "prompt_template", "Sum: {{i}}", &g);
    targs_flag(a, "folder_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_negative_folder_rejected(stest_ctx_t *ctx)
{
    /* folder_id -5 → EXIT_INVALID (negatives are rejected, not clamped to root) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NegFolder", &g);
    targs_flag(a, "prompt_template", "x", &g);
    targs_flag(a, "folder_id", "-5", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_invalid_folder(stest_ctx_t *ctx)
{
    /* folder_id "abc" → EXIT_INVALID (atoi would have silently made it 0 = root) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "BadFolder", &g);
    targs_flag(a, "prompt_template", "x", &g);
    targs_flag(a, "folder_id", "abc", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
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
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--json", "this is not json" };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_space(stest_ctx_t *ctx)
{
    /* --json <blob> (space form): the flag value must be honoured
     * (P2: it used to be ignored and stdin read instead). */
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--json",
                      "{\"name\":\"SrcJsonSpace\",\"prompt_template\":"
                      "\"src space {{input}}\"}" };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_json_equals(stest_ctx_t *ctx)
{
    /* --json=<blob> (equals form) */
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--json={\"name\":\"SrcJsonEquals\","
                      "\"prompt_template\":\"src equals {{input}}\"}" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_present(stest_ctx_t *ctx)
{
    const char *path = stest_write_input(ctx,
        "{\"name\":\"SrcFromFile\",\"prompt_template\":"
        "\"src file {{input}}\"}");
    TEST_NOT_NULL(ctx, path);
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--from_file", (char *)path };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_missing(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--from_file", "./tmp/acta_no_such_input.json" };
    int rc = stest_run_argv(ctx, cmd_skill, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "skill", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_skill, 4, argv0,
        "{\"name\":\"SrcStdin\",\"prompt_template\":"
        "\"src stdin {{input}}\"}");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_conflict_json_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--json", "{}", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_skill, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_json_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--json", "{}", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_skill, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_stdin_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "actagamma_db", "skill", "create",
                      "--stdin", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_skill, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_name(&ctx);
    test_create_missing_prompt(&ctx);
    test_create_empty_name(&ctx);
    test_create_root_unique_violation(&ctx);
    test_create_folder_unique_violation(&ctx);
    test_create_same_name_different_folder_ok(&ctx);
    test_create_negative_folder_rejected(&ctx);
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
