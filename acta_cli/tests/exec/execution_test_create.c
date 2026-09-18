/* ─────────────────────────────────────────────────────────────────────
 * execution_test_create.c
 * Unit tests for:  acta_cli exec create
 * ───────────────────────────────────────────────────────────────────── */
#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── local helper ──────────────────────────────────────────────────── */

static int do_exec(stest_ctx_t *ctx, const char *action,
                  cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_exec(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ─────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "What is the capital of France?", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "Full exec", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "parent_execution_id", "4", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "id only", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

/* prompt is optional: a context-only execution is valid (the runner
 * fails at run time only if prompt and context are both empty). */
static void test_create_without_prompt(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --prompt */
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_create_missing_context_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "no ctx", &g);
    /* no --context_id */
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_skill_revision_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "no skill", &g);
    targs_flag(a, "context_id", "1", &g);
    /* no --skill_revision_id */
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_model_revision_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "no model", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    /* no --model_revision_id */

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_zero_context_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "zero ctx", &g);
    targs_flag(a, "context_id", "0", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_negative_skill_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "neg skill", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "-3", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* W3: --status is dead on create (the lib forces "pending"), so it is
 * rejected for ANY value — "pending" included. */

static void test_create_status_pending_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "dead status", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "status", "pending", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_status_running_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "run me", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "status", "running", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_status_bogus_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad status", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "status", "bogus", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_valid_parent(stest_ctx_t *ctx)
{
    /* parent_execution_id=4 exists in ref DB */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "child exec", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "parent_execution_id", "4", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_context(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad ctx fk", &g);
    targs_flag(a, "context_id", "9999", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_skill_rev(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad skill fk", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "9999", &g);
    targs_flag(a, "model_revision_id", "2", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_model_rev(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad model fk", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "9999", &g);

    int rc = do_exec(ctx, "create", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_fk_violation_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "prompt", "bad parent fk", &g);
    targs_flag(a, "context_id", "1", &g);
    targs_flag(a, "skill_revision_id", "1", &g);
    targs_flag(a, "model_revision_id", "2", &g);
    targs_flag(a, "parent_execution_id", "9999", &g);

    int rc = do_exec(ctx, "create", a, g);
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
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json", "this is not json" };
    int rc = stest_run_argv(ctx, cmd_exec, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_space(stest_ctx_t *ctx)
{
    /* --json <blob> (space form): the flag value must be honoured
     * (P2: it used to be ignored and stdin read instead). */
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json",
                      "{\"prompt\":\"src json space\",\"context_id\":1,"
                      "\"skill_revision_id\":1,"
                      "\"model_revision_id\":2}" };
    int rc = stest_run_argv(ctx, cmd_exec, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_json_equals(stest_ctx_t *ctx)
{
    /* --json=<blob> (equals form) */
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json={\"prompt\":\"src json equals\","
                      "\"context_id\":1,\"skill_revision_id\":1,"
                      "\"model_revision_id\":2}" };
    int rc = stest_run_argv(ctx, cmd_exec, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_present(stest_ctx_t *ctx)
{
    const char *path = stest_write_input(ctx,
        "{\"prompt\":\"src from_file\",\"context_id\":1,"
        "\"skill_revision_id\":1,\"model_revision_id\":2}");
    TEST_NOT_NULL(ctx, path);
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--from_file", (char *)path };
    int rc = stest_run_argv(ctx, cmd_exec, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_missing(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--from_file", "./tmp/acta_no_such_input.json" };
    int rc = stest_run_argv(ctx, cmd_exec, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_exec, 4, argv0,
        "{\"prompt\":\"src stdin\",\"context_id\":1,"
        "\"skill_revision_id\":1,\"model_revision_id\":2}");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_conflict_json_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json", "{}", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_exec, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_json_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json", "{}", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_exec, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_stdin_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--stdin", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_exec, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_status_rejected(stest_ctx_t *ctx)
{
    /* "status" in the JSON body is rejected just like the flag (W3) */
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json",
                      "{\"prompt\":\"json status\",\"context_id\":1,"
                      "\"skill_revision_id\":1,"
                      "\"model_revision_id\":2,\"status\":\"completed\"}" };
    int rc = stest_run_argv(ctx, cmd_exec, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════
 *  known issues (docs/known_issues.md)
 * ═══════════════════════════════════════════════════════════════════ */

/* KI-7 (fixed): an FK-violation failure must surface sqlite3_errmsg —
 * the error JSON goes to stderr, which the harness now captures
 * (stest_stderr), so the message itself is assertable: it contains
 * "FOREIGN KEY" and no longer degenerates to "(no detail)". */
static void test_create_fk_error_has_detail(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "exec", "create",
                      "--json",
                      "{\"prompt\":\"ki7\",\"context_id\":999999,"
                      "\"skill_revision_id\":1,\"model_revision_id\":1}" };
    int rc = stest_run_argv(ctx, cmd_exec, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    const char *err = stest_stderr(ctx);
    TEST_CONTAINS(ctx, err, "FOREIGN KEY");
    TEST(ctx, err && !strstr(err, "(no detail)"));
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_execution_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_without_prompt(&ctx);
    test_create_missing_context_id(&ctx);
    test_create_missing_skill_revision_id(&ctx);
    test_create_missing_model_revision_id(&ctx);
    test_create_zero_context_id(&ctx);
    test_create_negative_skill_id(&ctx);
    test_create_status_pending_rejected(&ctx);
    test_create_status_running_rejected(&ctx);
    test_create_status_bogus_rejected(&ctx);
    test_create_valid_parent(&ctx);
    test_create_fk_violation_context(&ctx);
    test_create_fk_violation_skill_rev(&ctx);
    test_create_fk_violation_model_rev(&ctx);
    test_create_fk_violation_parent(&ctx);
    test_create_json_invalid(&ctx);
    test_create_src_json_space(&ctx);
    test_create_src_json_equals(&ctx);
    test_create_src_from_file_present(&ctx);
    test_create_src_from_file_missing(&ctx);
    test_create_src_stdin(&ctx);
    test_create_src_conflict_json_stdin(&ctx);
    test_create_src_conflict_json_file(&ctx);
    test_create_src_conflict_stdin_file(&ctx);
    test_create_src_json_status_rejected(&ctx);

    /* known issues (docs/known_issues.md) */
    test_create_fk_error_has_detail(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
