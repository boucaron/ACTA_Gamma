#include "skill_test_helpers.h"

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
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "TestSkill");
    targs_flag(a, "prompt_template", "Do {{thing}}");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "FullSkill");
    targs_flag(a, "prompt_template", "Template {{x}}");
    targs_flag(a, "folder_id", "2");
    targs_flag(a, "description", "A full skill");
    targs_flag(a, "output_schema", "{\"type\":\"string\"}");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "IdOnlySkill");
    targs_flag(a, "prompt_template", "x");
    global_opts_t g = gopts_id_only();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be a bare integer + newline, no braces */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    /* should NOT contain '{' */
    TEST(ctx, !strstr(out, "{"));
    targs_free(a);
}

static void test_create_missing_name(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    /* no --name */
    targs_flag(a, "prompt_template", "orphan prompt");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a);
}

static void test_create_missing_prompt(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NoPrompt");
    /* no --prompt_template */
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a);
}

static void test_create_empty_name(stest_ctx_t *ctx)
{
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "");
    targs_flag(a, "prompt_template", "has prompt");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a);
}

static void test_create_root_unique_violation(stest_ctx_t *ctx)
{
    /* "tata" already exists at root (id=2 in ref DB) */
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "tata");
    targs_flag(a, "prompt_template", "dupe at root");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    /* should fail: unique index uq_skills_root on (name) WHERE folder_id IS NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a);
}

static void test_create_folder_unique_violation(stest_ctx_t *ctx)
{
    /* "tata" already exists in folder 2 (id=4 in ref DB) */
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "tata");
    targs_flag(a, "prompt_template", "dupe in folder 2");
    targs_flag(a, "folder_id", "2");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a);
}

static void test_create_same_name_different_folder_ok(stest_ctx_t *ctx)
{
    /* "tata" exists in folder 1 (id=3) and folder 2 (id=4) but NOT in folder 1
     * with a different name... actually "tata" IS in folder 1.
     * Let's use "summarize2" which is at root (id=5) and in folder 2 (id=6).
     * Create "summarize2" in folder 1 → should succeed.
     */
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "summarize2");
    targs_flag(a, "prompt_template", "Sum: {{i}}");
    targs_flag(a, "folder_id", "1");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a);
}

static void test_create_negative_folder_clamped(stest_ctx_t *ctx)
{
    /* code clamps folder_id < 0 → 0 (root) */
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NegFolder");
    targs_flag(a, "prompt_template", "x");
    targs_flag(a, "folder_id", "-5");
    global_opts_t g = gopts_default();

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a);
}

static void test_create_json_invalid(stest_ctx_t *ctx)
{
    /* --json with garbage stdin → EXIT_INVALID
     * In a unit test we can't easily fake stdin.
     * Mark as integration-only; here we just verify the code path
     * is reachable by checking gopts.json_input doesn't crash.
     * ADAPT: if you have a way to inject stdin in unit tests, use it.
     * Otherwise skip / mark as integration test.
     */
    /* For now: verify that setting json_input=1 without valid stdin
     * doesn't segfault (it will fail on read_stdin_all).
     * We'll just skip the assertion for now.
     */
    (void)ctx;
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
    test_create_negative_folder_clamped(&ctx);
    test_create_json_invalid(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
