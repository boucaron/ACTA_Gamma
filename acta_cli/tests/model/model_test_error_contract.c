/* model_test_error_contract.c — P4 regression: library failures must emit
 * the single-line JSON error on stderr (finish_op_error / finish_db_error)
 * and keep stdout clean, instead of exiting non-zero silently.
 *
 * Before P4, every `if (rc != ACTA_DB_OK)` path in the entity files printed
 * nothing to stderr — only the exit code carried the failure.  The contract
 * under test (db.c is the reference implementation):
 *
 *   stderr line 1:  {"error":"ACTA_DB_ERR_<NAME>","code":<rc>,
 *                   "message":"<entity> <action> failed: <detail>"}
 *   stdout:         empty (JSON errors never go to stdout)
 *   exit:           map_rc_to_exit(rc)  (ACTA_DB_ERR_NOT_FOUND → 1)
 */
#include "test_helpers.h"

#include <unistd.h>
#include <fcntl.h>

#define REF_DB "acta_test_ref.db"

/* ── stderr capture (local; the shared helper captures stdout only) ── */
/* Same dup2 technique as stest_capture_begin/end, applied to fd 2.
 * The buffer is a static 64 KiB: the contract line is short; anything
 * larger is already a contract violation (extra stderr lines) and the
 * prefix/shape asserts below still hold on the truncated head. */
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

/* Pinned stderr prefixes: name, code, and op label per the contract. */
static const char *const DELETE_ERR_PREFIX =
    "{\"error\":\"ACTA_DB_ERR_NOT_FOUND\",\"code\":-1,"
    "\"message\":\"model delete failed:";
static const char *const RESTORE_ERR_PREFIX =
    "{\"error\":\"ACTA_DB_ERR_NOT_FOUND\",\"code\":-1,"
    "\"message\":\"model restore failed:";

/* Single-line JSON check: prefix match + closing "...}\n". */
static int err_line_matches(const char *prefix)
{
    size_t plen = strlen(prefix);
    return strncmp(s_err, prefix, plen) == 0 &&
           s_err_len > plen &&
           s_err[s_err_len - 1] == '\n' &&
           s_err[s_err_len - 2] == '}';
}

/* ── action wrappers (stdout + stderr captured) ───────────────────── */

static int do_delete(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    err_begin();
    int rc = cmd_model("delete", args, &gopts, ctx->db);
    err_end();
    stest_capture_end(ctx);
    return rc;
}

static int do_restore(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    err_begin();
    int rc = cmd_model("restore", args, &gopts, ctx->db);
    err_end();
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

/* `model delete <missing>` → NOT_FOUND: JSON error line on stderr,
 * empty stdout, exit 1. */
static void test_delete_missing_emits_json_error(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_delete(ctx, a, g);

    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    TEST_STREQ(ctx, stest_stdout(ctx), "");

    /* stderr line 1 is the JSON error (prefix pins name, code, message),
     * closed as single-line JSON: "…}\n". */
    TEST(ctx, err_line_matches(DELETE_ERR_PREFIX));
    targs_free(a, &g);
}

/* `model restore <missing>` → same contract, different op label. */
static void test_restore_missing_emits_json_error(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_restore(ctx, a, g);

    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    TEST_STREQ(ctx, stest_stdout(ctx), "");

    /* Same contract, different op label. */
    TEST(ctx, err_line_matches(RESTORE_ERR_PREFIX));
    targs_free(a, &g);
}

/* Positive control: the success path stays quiet on stderr (no spurious
 * error line) and prints the success JSON on stdout.  Runs last — it
 * mutates the row (id=1 is pristine in this suite's fresh ref copy). */
static void test_success_path_keeps_stderr_clean(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_delete(ctx, a, g);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"deleted\":true");
    TEST_STREQ(ctx, captured_stderr(), "");
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_model_test_error_contract(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_delete_missing_emits_json_error(&ctx);
    test_restore_missing_emits_json_error(&ctx);
    test_success_path_keeps_stderr_clean(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
