/* cli_util_test_dispatch.c — contract test for the unknown-*entity* error
 * in commands_dispatch (entity_not_found, 73591d4):
 *
 *   stderr line 1:  {"error":"ACTA_CLI_ERR","code":-10,
 *                    "message":"unknown entity: <name>"}     (byte-stable,
 *                    scripts grep it — the suggestion never alters it)
 *   stderr line 2:  "  Did you mean '<entity>'?"             (only when the
 *                    fuzzy matcher finds a candidate; same matcher as the
 *                    unknown-action UX)
 *   stdout:         empty
 *   exit:           EXIT_CLI  (unchanged; NOT EXIT_INVALID)
 */
#include "test_helpers.h"

#include <unistd.h>
#include <fcntl.h>

#define REF_DB "acta_test_ref.db"

/* ── stderr capture (local; the shared helper captures stdout only) ──
 * Same dup2 technique as stest_capture_begin/end, applied to fd 2 —
 * mirrored from model_test_error_contract.c. Static 64 KiB: both lines
 * are short; anything larger is already a contract violation and the
 * exact-match asserts below still hold on the truncated head. */
#define ERR_CAP 65536

static int    s_saved_err = -1;
static int    s_err_fd    = -1;
static char   s_err[ERR_CAP];
static size_t s_err_len  = 0;

static void err_begin(void)
{
    int p[2];
#ifdef _WIN32
    if (_pipe(p, 65536, _O_BINARY) != 0) return;
#else
    if (pipe(p) != 0) return;
#endif
    s_saved_err = dup(STDERR_FILENO);
    dup2(p[1], STDERR_FILENO);
    close(p[1]);
    s_err_fd  = p[0];
    s_err_len = 0;
    s_err[0]  = '\0';
}

static void err_end(void)
{
    if (s_saved_err < 0) return;
    fflush(stderr);
    dup2(s_saved_err, STDERR_FILENO);
    close(s_saved_err);
    s_saved_err = -1;
    for (;;) {
        size_t space = sizeof(s_err) - 1 - s_err_len;
        ssize_t n = read(s_err_fd, s_err + s_err_len, space);
        if (n <= 0) break;
        s_err_len += (size_t)n;
    }
    close(s_err_fd);
    s_err_fd = -1;
    s_err[s_err_len] = '\0';
}

const char *captured_stderr(void) { return s_err; }

/* ── dispatch wrapper (stdout + stderr captured) ──────────────────── */

static int do_dispatch(stest_ctx_t *ctx, const char *entity, const char *action)
{
    global_opts_t g = gopts_default();
    cmd_args_t ga;
    cmd_args_init(&ga, 0, NULL);

    stest_capture_begin(ctx);
    err_begin();
    int rc = commands_dispatch(entity, action, &ga, &g, ctx->db);
    err_end();
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

/* No candidate within threshold → exactly the JSON line, byte-stable. */
static void test_unknown_entity_no_suggestion(stest_ctx_t *ctx)
{
    int rc = do_dispatch(ctx, "frobnicate", "get");
    TEST_EQ(ctx, rc, EXIT_CLI);
    TEST_STREQ(ctx, stest_stdout(ctx), "");
    TEST_STREQ(ctx, captured_stderr(),
        "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
        "\"message\":\"unknown entity: frobnicate\"}\n");
}

/* "modle" is 2 edits (transposition) from "model" → JSON line +
 * suggestion line. */
static void test_unknown_entity_suggests_closest(stest_ctx_t *ctx)
{
    int rc = do_dispatch(ctx, "modle", "get");
    TEST_EQ(ctx, rc, EXIT_CLI);
    TEST_STREQ(ctx, stest_stdout(ctx), "");
    TEST_STREQ(ctx, captured_stderr(),
        "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
        "\"message\":\"unknown entity: modle\"}\n"
        "  Did you mean 'model'?\n");
}

/* Short target: "dbx" is 1 edit from "db" (limit 1 for names ≤ 3). */
static void test_unknown_entity_suggests_short_name(stest_ctx_t *ctx)
{
    int rc = do_dispatch(ctx, "dbx", "get");
    TEST_EQ(ctx, rc, EXIT_CLI);
    TEST_STREQ(ctx, stest_stdout(ctx), "");
    TEST_STREQ(ctx, captured_stderr(),
        "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
        "\"message\":\"unknown entity: dbx\"}\n"
        "  Did you mean 'db'?\n");
}

/* Positive control: a valid entity still dispatches (suggestion path
 * must not shadow lookup), stderr stays clean. */
static void test_known_entity_still_dispatches(stest_ctx_t *ctx)
{
    int rc = do_dispatch(ctx, "db", "version");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"version\"");
    TEST_STREQ(ctx, captured_stderr(), "");
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_cli_util_test_dispatch(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_unknown_entity_no_suggestion(&ctx);
    test_unknown_entity_suggests_closest(&ctx);
    test_unknown_entity_suggests_short_name(&ctx);
    test_known_entity_still_dispatches(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
