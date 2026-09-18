#include "test_helpers.h"


#define REF_DB "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_action(stest_ctx_t *ctx, const char *action,
                    cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "create");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "list");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "bogus", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "creat" is close to "create" → closest_action should suggest it */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_action(ctx, "creat", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    /* The suggestion goes to stderr; in this harness stderr is not
     * captured, so we only verify the exit code.
     * If you add stderr capture, assert:
     *   TEST_CONTAINS(ctx, stest_stderr(ctx), "create"); */
    targs_free(a, &g);
}

/* ═══════════════════════════════════════════════════════════════════
 *  known issues (KI-n regression pins)
 * ═══════════════════════════════════════════════════════════════════ */

/* KI-3 (RED until fixed): --raw_out on a NULL field returns rc 10
 * "unknown --raw_out field" while listing that field as supported;
 * help says "null values produce no output". context.c/execution.c
 * conflate a NULL value with an unknown field (else if(v)). Desired:
 * rc 0 with empty output. */
static void test_get_raw_out_null_field(stest_ctx_t *ctx)
{
    /* create without --metadata → metadata is NULL; parse the id back */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "type",    "ki3", &g);
    targs_flag(a, "content", "raw-out-null-test", &g);
    stest_capture_begin(ctx);
    int crc = cmd_context("create", a, &g, ctx->db);
    stest_capture_end(ctx);
    targs_free(a, &g);
    TEST_EQ(ctx, crc, EXIT_OK);
    const char *p = strstr(stest_stdout(ctx), "\"id\":");
    TEST_NOT_NULL(ctx, p);
    int id = atoi(p + 5);

    char idstr[16];
    snprintf(idstr, sizeof(idstr), "%d", id);

    global_opts_t g2 = gopts_default();
    g2.raw_out = "metadata";
    cmd_args_t   *b = targs_new();
    targs_pos(b, idstr, &g2);
    stest_capture_begin(ctx);
    int rc = cmd_context("get", b, &g2, ctx->db);
    stest_capture_end(ctx);
    targs_free(b, &g2);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_EQ(ctx, (int)strlen(stest_stdout(ctx)), 0);
}

/* KI-4 (fixed): trailing "help" used to be swallowed as an unconsumed
 * positional and SILENTLY EXECUTE the action (`context list help`
 * printed the full list, exit 0). commands_dispatch now rejects any
 * surplus positional (beyond the tools-table count for the action)
 * BEFORE the handler runs — exit 10, no payload on stdout
 * (documented decision, see docs/cli_spec.md). Routed through
 * stest_run_dispatch (the real main.c path, not the bare handler). */
static void test_trailing_help_rejected(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "list", "help" };
    int rc = stest_run_dispatch(ctx, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_CLI);
}

/* KI-2 (RED until fixed): unknown JSON keys are silently accepted on
 * create — no key allow-list. Desired: rc 4 rejecting the unknown key. */
static void test_create_unknown_json_key(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "create",
                      "--json",
                      "{\"type\":\"ki2\",\"content\":\"c\","
                      "\"bogus_key\":1}" };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* KI-6 (fixed, now a regression pin): --id_only was silently ignored
 * on list actions (full JSON printed). Every list action now rejects
 * it with exit 4; the flag below must fail, not print rows. */
static void test_list_id_only_pinned(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "list", "--id_only" };
    int rc = stest_run_argv(ctx, cmd_context, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);

    /* known issues (KI-n regression pins) */
    test_get_raw_out_null_field(&ctx);
    test_trailing_help_rejected(&ctx);
    test_create_unknown_json_key(&ctx);
    test_list_id_only_pinned(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
