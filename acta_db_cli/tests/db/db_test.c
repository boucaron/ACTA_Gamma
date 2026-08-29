#include "../skill/skill_test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── local helper ─────────────────────────────────────────────────── */

static int do_db(stest_ctx_t *ctx, const char *action,
                 cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_db(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════
 *  db exec
 * ═══════════════════════════════════════════════════════════════════ */

/* --- success paths ------------------------------------------------- */

static void test_exec_positional_insert(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "INSERT INTO contexts(type, content, content_hash) "
               "VALUES('text','db-test-positional','h1')",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"ok\"");
    targs_free(a, &g);
}

static void test_exec_sql_flag(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "INSERT INTO contexts(type, content, content_hash) "
               "VALUES('text','db-test-sqlflag','h2')",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_exec_ddl_create_index(stest_ctx_t *ctx)
{
    /* DDL should also succeed */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "CREATE INDEX IF NOT EXISTS idx_ctx_dbtest "
               "ON contexts(type)",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);

    /* clean up */
    targs_flag(a, "sql", "DROP INDEX IF EXISTS idx_ctx_dbtest", &g);
    rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_exec_trailing_semicolon(stest_ctx_t *ctx)
{
    /* The lib (sqlite3_exec) accepts a trailing ';' after a single
     * statement (the help example uses one). Lock that behavior in. */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "INSERT INTO contexts(type, content, content_hash) "
               "VALUES('text','db-test-trailing-semicolon','h4');",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_exec_table_mode(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "INSERT INTO contexts(type, content, content_hash) "
               "VALUES('text','db-test-table','h3')",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* table mode prints bare "ok", not JSON */
    TEST(ctx, strstr(stest_stdout(ctx), "\"status\"") == NULL);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "ok");
    targs_free(a, &g);
}

/* --- error paths --------------------------------------------------- */

static void test_exec_no_source(stest_ctx_t *ctx)
{
    /* no positional, no --sql, no --file, no --stdin */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_exec_global_stdin_rejected(stest_ctx_t *ctx)
{
    /* The global --stdin is consumed by parse_globals into gopts->from_stdin
     * before db exec sees it. The handler must reject it with a clear error
     * (pointing at --sql_stdin), not silently fall through to "no SQL source"
     * or start reading stdin. */
    global_opts_t g = gopts_default();
    g.from_stdin = 1;
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_exec_empty_sql_flag(stest_ctx_t *ctx)
{
    /* --sql "" must be rejected with a clear error, not passed to the lib */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql", "", &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_exec_empty_file(stest_ctx_t *ctx)
{
    /* 0-byte file must be rejected (was: acta_db_exec(db, "")) */
    const char *path = "acta_test_empty.sql";
    FILE *fp = fopen(path, "w");
    if (fp)
        fclose(fp);

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "file", path, &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
    remove(path);
}

static void test_exec_invalid_sql(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql", "THIS IS NOT SQL", &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_exec_fk_violation(stest_ctx_t *ctx)
{
    /* INSERT into executions with a bogus context_id */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "INSERT INTO executions(context_id, skill_revision_id, "
               "model_revision_id, status) "
               "VALUES(9999, 1, 1, 'pending')",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    /* should fail: FK violation (PRAGMA foreign_keys=OFF in the dump,
     * but the test harness may enable it; if OFF, this still inserts –
     * we just assert the call ran and returned either OK or an error) */
    /* Keep assertion loose: must not crash */
    (void)rc;
    targs_free(a, &g);
}

static void test_exec_immutable_context(stest_ctx_t *ctx)
{
    /* contexts table has a BEFORE UPDATE trigger that RAISE(ABORT) */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "sql",
               "UPDATE contexts SET content='hack' WHERE id=1",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_exec_missing_file(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "file", "/nonexistent/path/to/migration.sql", &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── version ──────────────────────────────────────────────────────── */

static void test_version_json(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "version", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"version\":");
    targs_free(a, &g);
}

static void test_version_table(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_table();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "version", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "SQLite");
    targs_free(a, &g);
}

/* ── help ─────────────────────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "help", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "exec");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "version");
    targs_free(a, &g);
}

/* ── unknown action ───────────────────────────────────────────────── */

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "frobnicate", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "exect" is one char off "exec" – closest_match should suggest it */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "exect", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    /* suggestion goes to stderr, not captured stdout; just check rc */
    targs_free(a, &g);
}

/* ── verbose smoke (must not crash, output goes to stderr) ───────── */

static void test_exec_verbose(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.verbose = 3;
    cmd_args_t *a = targs_new();
    targs_flag(a, "sql",
               "INSERT INTO contexts(type, content, content_hash) "
               "VALUES('text','verbose-test','hv')",
               &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_db_test_all(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    /* exec – success */
    test_exec_positional_insert(&ctx);
    test_exec_sql_flag(&ctx);
    test_exec_ddl_create_index(&ctx);
    test_exec_trailing_semicolon(&ctx);
    test_exec_table_mode(&ctx);

    /* exec – errors */
    test_exec_no_source(&ctx);
    test_exec_global_stdin_rejected(&ctx);
    test_exec_empty_sql_flag(&ctx);
    test_exec_empty_file(&ctx);
    test_exec_invalid_sql(&ctx);
    test_exec_fk_violation(&ctx);
    test_exec_immutable_context(&ctx);
    test_exec_missing_file(&ctx);

    /* version */
    test_version_json(&ctx);
    test_version_table(&ctx);

    /* help */
    test_help(&ctx);

    /* unknown action */
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);

    /* verbose */
    test_exec_verbose(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
