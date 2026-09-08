/* cli_util_test_error_contract.c — T2 contract: the code/exit invariant.
 *
 * One namespace, and `code` = −exit everywhere:
 *   - library failures keep their ACTA_DB_ERR_* name but emit
 *     `code` = −map_rc_to_exit(rc)  (e.g. ACTA_DB_ERR_DUPLICATE →
 *     code -4, exit 4 — the raw −6/−7/−5 no longer leak onto the wire);
 *   - the DB-open-failure path is its own class: emit_db_open_error()
 *     → code -11, exit EXIT_DB_OPEN (11), making the spec's exit 11
 *     reachable.  main() itself is not unit-testable in-process, so the
 *     shared emitter is pinned here.
 *
 *   stderr line 1:  {"error":"ACTA_DB_ERR_<NAME>","code":-<exit>,
 *                    "message":"..."}
 *   exit:           the `code` field negated
 */
#include "test_helpers.h"

#include <unistd.h>
#include <fcntl.h>

/* ── stderr capture (local; same dup2 technique as
 * model_test_error_contract.c) ──────────────────────────────────── */
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

/* ── tests ───────────────────────────────────────────────────────── */

/* DUPLICATE / FK / INVALID_DB keep their names but carry `code` =
 * −exit (−4), so |code| == exit. */
static void test_db_error_codes_match_exit(stest_ctx_t *ctx)
{
    err_begin();
    int rc = finish_db_error(ACTA_DB_ERR_DUPLICATE, "dup");
    err_end();
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_DUPLICATE\",\"code\":-4,"
        "\"message\":\"dup\"}\n");

    err_begin();
    rc = finish_db_error(ACTA_DB_ERR_FK, "fk");
    err_end();
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_FK\",\"code\":-4,"
        "\"message\":\"fk\"}\n");

    err_begin();
    rc = finish_db_error(ACTA_DB_ERR_INVALID_DB, "bad db");
    err_end();
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_INVALID_DB\",\"code\":-4,"
        "\"message\":\"bad db\"}\n");

    /* the −1/−2/−3/−4 pins that already held stay byte-identical */
    err_begin();
    rc = finish_db_error(ACTA_DB_ERR_NOT_FOUND, "nf");
    err_end();
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_NOT_FOUND\",\"code\":-1,"
        "\"message\":\"nf\"}\n");

    err_begin();
    rc = finish_db_error(ACTA_DB_ERR_ALLOC, "oom");
    err_end();
    TEST_EQ(ctx, rc, EXIT_ALLOC);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_ALLOC\",\"code\":-3,"
        "\"message\":\"oom\"}\n");
}

/* CLI-usage namespace: emit_cli_error → code -10, exit EXIT_CLI (10). */
static void test_emit_cli_error_contract(stest_ctx_t *ctx)
{
    err_begin();
    int rc = emit_cli_error("unknown option '--deletd' (see --help)");
    err_end();
    TEST_EQ(ctx, rc, EXIT_CLI);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
        "\"message\":\"unknown option '--deletd' (see --help)\"}\n");
}

/* DB-open failure: the spec's exit 11 becomes real.  The name keeps the
 * raw rc's granularity; the code is −11. */
static void test_db_open_failure_contract(stest_ctx_t *ctx)
{
    err_begin();
    int rc = emit_db_open_error(ACTA_DB_ERR_INVALID_DB,
                                "cannot open database 'x' (not a sqlite db)");
    err_end();
    TEST_EQ(ctx, rc, EXIT_DB_OPEN);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_INVALID_DB\",\"code\":-11,"
        "\"message\":\"cannot open database 'x' (not a sqlite db)\"}\n");

    err_begin();
    rc = emit_db_open_error(ACTA_DB_ERR_SQL,
                            "cannot open database 'x' (bad path)");
    err_end();
    TEST_EQ(ctx, rc, EXIT_DB_OPEN);
    TEST_STREQ(ctx, s_err,
        "{\"error\":\"ACTA_DB_ERR_SQL\",\"code\":-11,"
        "\"message\":\"cannot open database 'x' (bad path)\"}\n");
}

/* ── runner ──────────────────────────────────────────────────────── */

int run_cli_util_test_error_contract(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, "acta_test_ref.db");

    test_db_error_codes_match_exit(&ctx);
    test_emit_cli_error_contract(&ctx);
    test_db_open_failure_contract(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
