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
 *   G8  inline "--name=value" extraction (flag_inline_value): the exact
 *       value each global flag yields, plus the --verbose error paths
 *
 * The per-entity *_test_create.c suites already cover the input-source
 * happy paths; this suite owns the *seam* itself, plus the
 * parse_globals / dispatch paths those suites cannot reach.
 */
#include "test_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>   /* _O_BINARY (MinGW) */
#ifdef _WIN32
#include <io.h>   /* _pipe */
#endif

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

/*
 * Run parse_globals with stderr redirected to a pipe; rc gets
 * parse_globals' return value and the captured stderr is returned in
 * a heap buffer the caller frees (NULL on plumbing failure). Used by
 * G8 to pin the error-line text of the --verbose inline branches.
 */
#ifdef _WIN32
#define CAPTURE_FD_DUP   _dup
#define CAPTURE_FD_DUP2  _dup2
#define CAPTURE_FD_CLOSE _close
#define CAPTURE_PIPE(n)  (_pipe(n, 65536, _O_BINARY) != 0)
#else
#define CAPTURE_FD_DUP   dup
#define CAPTURE_FD_DUP2  dup2
#define CAPTURE_FD_CLOSE close
#define CAPTURE_PIPE(n)  (pipe(n) != 0)
#endif

static char *run_parse_capture_stderr(int argc, char **argv,
                                      global_opts_t *g, int *rc)
{
    int saved = CAPTURE_FD_DUP(STDERR_FILENO);
    int p[2];
    if (saved < 0 || CAPTURE_PIPE(p)) {
        if (saved >= 0) CAPTURE_FD_CLOSE(saved);
        return NULL;
    }
    CAPTURE_FD_DUP2(p[1], STDERR_FILENO);
    CAPTURE_FD_CLOSE(p[1]);

    *rc = parse_globals(argc, argv, g);

    fflush(stderr);
    CAPTURE_FD_DUP2(saved, STDERR_FILENO);
    CAPTURE_FD_CLOSE(saved);

    FILE *in = fdopen(p[0], "rb");
    if (!in) return NULL;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    for (;;) {
        if (len >= cap) {
            cap = cap ? cap * 2 : 4096;
            char *nb = realloc(buf, cap);
            if (!nb) { fclose(in); free(buf); return NULL; }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, in);
        if (n == 0) break;
        len += n;
    }
    fclose(in);
    buf[len] = '\0';
    return buf;
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

/* ── G8: inline "--name=value" extraction (flag_inline_value) ──────
 *
 * Direct parse_globals calls (no dispatch): these pin the exact value
 * each flag's inline form yields, which the exit-code-only seam tests
 * never check (a desynced value offset would still pass them, since
 * run_dispatch never consumes g->db / g->fields).
 */

static void test_inline_values(stest_ctx_t *ctx)
{
    /* --db=<path> */
    {
        char *a[] = { "acta_cli", "model", "list", "--db=foo.db" };
        global_opts_t g;
        int rc = parse_globals(4, a, &g);
        TEST_EQ(ctx, rc, EXIT_OK);
        if (rc == EXIT_OK) {
            TEST_STREQ(ctx, g.db, "foo.db");
            free(g.argv);
        }
    }

    /* --fields=<csv> */
    {
        char *a[] = { "acta_cli", "model", "list",
                      "--fields=name,description" };
        global_opts_t g;
        int rc = parse_globals(4, a, &g);
        TEST_EQ(ctx, rc, EXIT_OK);
        if (rc == EXIT_OK) {
            TEST_STREQ(ctx, g.fields, "name,description");
            free(g.argv);
        }
    }

    /* --json=<blob> */
    {
        char *a[] = { "acta_cli", "model", "create",
                      "--json={\"name\":\"x\",\"backend\":\"openai\","
                      "\"model_identifier\":\"m\"}" };
        global_opts_t g;
        int rc = parse_globals(4, a, &g);
        TEST_EQ(ctx, rc, EXIT_OK);
        if (rc == EXIT_OK) {
            TEST_STREQ(ctx, g.json_input,
                       "{\"name\":\"x\",\"backend\":\"openai\","
                       "\"model_identifier\":\"m\"}");
            free(g.argv);
        }
    }

    /* --from_file=<path> */
    {
        char *a[] = { "acta_cli", "model", "create", "--from_file=p.json" };
        global_opts_t g;
        int rc = parse_globals(4, a, &g);
        TEST_EQ(ctx, rc, EXIT_OK);
        if (rc == EXIT_OK) {
            TEST_STREQ(ctx, g.from_file, "p.json");
            free(g.argv);
        }
    }

    /* --verbose boundaries: =0 (low end) and =2 (mid) */
    {
        char *a0[] = { "acta_cli", "model", "list", "--verbose=0" };
        global_opts_t g;
        int rc = parse_globals(4, a0, &g);
        TEST_EQ(ctx, rc, EXIT_OK);
        if (rc == EXIT_OK) {
            TEST_EQ(ctx, g.verbose, 0);
            free(g.argv);
        }
    }
    {
        char *a2[] = { "acta_cli", "model", "list", "--verbose=2" };
        global_opts_t g;
        int rc = parse_globals(4, a2, &g);
        TEST_EQ(ctx, rc, EXIT_OK);
        if (rc == EXIT_OK) {
            TEST_EQ(ctx, g.verbose, 2);
            free(g.argv);
        }
    }

    /* --verbose= (empty inline value) → CLI error naming the level */
    {
        char *a[] = { "acta_cli", "model", "list", "--verbose=" };
        global_opts_t g;
        int rc = 0;
        char *err = run_parse_capture_stderr(4, a, &g, &rc);
        TEST_EQ(ctx, rc, EXIT_CLI);
        if (err) {
            TEST(ctx, strstr(err, "invalid --verbose level") != NULL);
            free(err);
        }
    }

    /* --verbose=abc → same error, message carries the extracted value */
    {
        char *a[] = { "acta_cli", "model", "list", "--verbose=abc" };
        global_opts_t g;
        int rc = 0;
        char *err = run_parse_capture_stderr(4, a, &g, &rc);
        TEST_EQ(ctx, rc, EXIT_CLI);
        if (err) {
            TEST(ctx, strstr(err, "invalid --verbose level") != NULL);
            TEST(ctx, strstr(err, "'abc'") != NULL);
            free(err);
        }
    }
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
    test_inline_values(&ctx);
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
