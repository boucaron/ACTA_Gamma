/* gparse_test_main.c — S2: dedicated suite for the global-parse layer.
 *
 * Feeds raw argv through the same seam main.c walks —
 *   parse_globals → apply_flag_aliases → cmd_args_init →
 *   cmd_args_validate → handler / commands_dispatch
 * — and pins the P2-class seams the per-entity suites (which build
 * cmd_args_t directly) never touch:
 *   G1  too few positionals (argc 1 / entity without action) → EXIT_CLI
 *   G2  missing flag value (--db, --json, --fields, --from_file
 *       as the last token) → EXIT_CLI
 *   G3  value consumption: `--json --table` consumes "--table" as the
 *       blob and the handler rejects it (EXIT_INVALID, not EXIT_CLI)
 *   G4  unknown long option → EXIT_CLI (T2 contract)
 *   G5  `--verbose=99` clamp → command still runs (EXIT_OK)
 *   G6  the three JSON input sources end-to-end (--json / --stdin /
 *       --from_file) and their mutual exclusion
 *   G7  commands_dispatch: unknown entity / unknown action → EXIT_CLI,
 *       known (entity, action) → EXIT_OK
 *
 * The per-entity *_test_create.c suites already cover the input-source
 * happy paths; this suite owns the *seam* itself, plus the
 * parse_globals / dispatch paths those suites cannot reach.
 */
#include "test_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF_DB "acta_test_ref.db"

/* Minimal valid model blobs for the input-source happy paths
 * (model create requires name, backend and model_identifier).
 * Names differ per source: the test DB is shared across the three
 * creates, and model name is unique. */
static const char *const MODEL_JSON   =
    "{\"name\":\"gparse_json\",\"backend\":\"openai\","
    "\"model_identifier\":\"gpt-x\"}";
static const char *const MODEL_STDIN  =
    "{\"name\":\"gparse_stdin\",\"backend\":\"openai\","
    "\"model_identifier\":\"gpt-x\"}";
static const char *const MODEL_FILE   =
    "{\"name\":\"gparse_file\",\"backend\":\"openai\","
    "\"model_identifier\":\"gpt-x\"}";

/*
 * Replicate main.c's dispatch seam with a pre-opened DB:
 * parse_globals → apply_flag_aliases → cmd_args_init →
 * cmd_args_validate → commands_dispatch(ctx->db).
 * Early exits (--version/--help/--tools) return EXIT_OK without
 * printing (the suite asserts the code, not the prose).
 * Returns the exit code main.c would return.
 */
static int run_dispatch(stest_ctx_t *ctx, int argc, char **argv)
{
    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);
    if (rc != EXIT_OK)
        return rc;
    if (g.show_version || g.show_help || g.show_tools)
        return EXIT_OK;

    apply_flag_aliases(g.argv, g.argc);
    cmd_args_t ga;
    cmd_args_init(&ga, g.argc - 2, g.argv + 2);

    int vrc = cmd_args_validate(&ga);
    if (vrc != EXIT_OK) {
        free(g.argv);
        return vrc;
    }
    int drc = commands_dispatch(g.argv[0], g.argv[1], &ga, &g, ctx->db);
    free(g.argv);
    return drc;
}

/* ── G1–G5: parse_globals + validation rejections (known entity) ── */

static void test_rejections(stest_ctx_t *ctx)
{
    /* G1: too few positionals */
    char *a1[] = { "acta_cli" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 1, a1, NULL), EXIT_CLI);

    char *a2[] = { "acta_cli", "model" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 2, a2, NULL), EXIT_CLI);

    /* G2: missing value for a value-taking global flag, last token */
    char *j[] = { "acta_cli", "model", "create", "--json" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, j, NULL), EXIT_CLI);

    char *db[] = { "acta_cli", "model", "list", "--db" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, db, NULL), EXIT_CLI);

    char *f[] = { "acta_cli", "model", "list", "--fields" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, f, NULL), EXIT_CLI);

    char *ff[] = { "acta_cli", "model", "create", "--from_file" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, ff, NULL), EXIT_CLI);

    /* G3: `--json --table` — "--table" is consumed as the blob value,
     * so the seam accepts it and the handler's JSON parse fails:
     * EXIT_INVALID, not EXIT_CLI. */
    char *v[] = { "acta_cli", "model", "create", "--json", "--table" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 5, v, NULL),
            EXIT_INVALID);

    /* G4: unknown long option is a CLI-usage error (T2) */
    char *b[] = { "acta_cli", "model", "list", "--bogus" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, b, NULL), EXIT_CLI);

    /* G5: --verbose=99 clamps to 3 (VLOG-only) and the command runs */
    char *vb[] = { "acta_cli", "model", "list", "--verbose=99" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, vb, NULL), EXIT_OK);
}

/* ── G6: the three JSON input sources, end-to-end ────────────────── */

static void test_input_sources(stest_ctx_t *ctx)
{
    /* --json <blob> */
    char *j[] = { "acta_cli", "model", "create", "--json", (char *)MODEL_JSON };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 5, j, NULL), EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");

    /* --stdin */
    char *s[] = { "acta_cli", "model", "create", "--stdin" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 4, s, MODEL_STDIN), EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");

    /* --from_file <path> */
    const char *path = stest_write_input(ctx, MODEL_FILE);
    TEST_NOT_NULL(ctx, path);
    if (path) {
        char *f[5] = { "acta_cli", "model", "create", "--from_file",
                       (char *)path };
        TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 5, f, NULL), EXIT_OK);
        TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    }

    /* mutually exclusive: two sources → handler error, EXIT_INVALID */
    char *c[] = { "acta_cli", "model", "create", "--json",
                  (char *)MODEL_JSON, "--stdin" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_model, 6, c, NULL), EXIT_INVALID);
}

/* ── G7: the dispatch seam (unknown entity / action) ─────────────── */

static void test_dispatch(stest_ctx_t *ctx)
{
    /* unknown entity */
    char *e[] = { "acta_cli", "nosuch", "list" };
    stest_capture_begin(ctx);
    int rc = run_dispatch(ctx, 3, e);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_CLI);

    /* unknown action */
    char *a[] = { "acta_cli", "model", "nosuch" };
    stest_capture_begin(ctx);
    rc = run_dispatch(ctx, 3, a);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_CLI);

    /* known (entity, action) → handler runs */
    char *d[] = { "acta_cli", "model", "list" };
    stest_capture_begin(ctx);
    rc = run_dispatch(ctx, 3, d);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* list emits a JSON array (the ref DB carries seed rows) */
    TEST_CONTAINS(ctx, stest_stdout(ctx), "[");

    char *v[] = { "acta_cli", "db", "version" };
    stest_capture_begin(ctx);
    rc = run_dispatch(ctx, 3, v);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"version\":");

    /* early exit: --version needs no entity/action */
    char *ver[] = { "acta_cli", "--version" };
    stest_capture_begin(ctx);
    rc = run_dispatch(ctx, 2, ver);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_OK);
}

/* ── suite entry point ───────────────────────────────────────────── */

int run_gparse_test(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_rejections(&ctx);
    test_input_sources(&ctx);
    test_dispatch(&ctx);

    stest_teardown(&ctx);
    return ctx.failures;
}

int main(void)
{
    int f = run_gparse_test();

    if (f == 0) {
        printf("PASS: all global-parse (S2) tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
