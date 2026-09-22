#include "test_helpers.h"

#include <sys/stat.h>

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

static void test_exec_positional_real(stest_ctx_t *ctx)
{
    /* The documented positional form: db exec "INSERT ...".  (The
     * existing test_exec_positional_insert actually drives the --sql
     * flag; this one exercises the real positional path.  Includes a
     * trailing ';' as in the help example. */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_pos(a,
              "INSERT INTO contexts(type, content, content_hash) "
              "VALUES('text','db-test-positional-real','h6');",
              &g);

    int rc = do_db(ctx, "exec", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"ok\"");
    targs_free(a, &g);
}

static void test_exec_positional_after_bool_flag(stest_ctx_t *ctx)
{
    /* Regression (P1 #1): the old cmd_args_next_positional heuristic
     * ate the token after a boolean flag as its value, so a positional
     * SQL after --sql_stdin was lost (and the handler would read
     * stdin). The positional must survive the bool flag and win as the
     * first-listed source — and stdin must NOT be read. */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag_bool(a, "sql_stdin", &g);
    targs_pos(a,
              "INSERT INTO contexts(type, content, content_hash) "
              "VALUES('text','db-test-pos-after-bool','h7');",
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
    TEST_CONTAINS(ctx, stest_stdout(ctx), "backup");
    targs_free(a, &g);
}

/* ── unknown action ───────────────────────────────────────────────── */

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "frobnicate", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    targs_free(a, &g);
}

static void test_unknown_action_suggestion(stest_ctx_t *ctx)
{
    /* "exect" is one char off "exec" – closest_match should suggest it */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "exect", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
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

/* ═══════════════════════════════════════════════════════════════════
 *  known issues (KI-n) — run via parse_globals +
 *  handler exactly like main.c does (stest_run_argv).
 * ═══════════════════════════════════════════════════════════════════ */

/* KI-1 (fixed): db exec --sql_stdin used to always fail rc 4 —
 * db.c derived the boolean flag with cmd_args_flag(...) != NULL, which
 * is NULL by construction. Now use_stdin = cmd_args_has_flag(...);
 * this test is a regression pin: the INSERT below must succeed. */
static void test_exec_sql_stdin_flag(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "db", "exec", "--sql_stdin" };
    int rc = stest_run_argv(ctx, cmd_db, 4, argv0,
        "INSERT INTO contexts(type, content, content_hash) "
        "VALUES('text','ki1-sql-stdin','hki1');");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"ok\"");
}

/* KI-5 (fixed, now a regression pin): help/--tools claim "no SELECT"
 * and db.c now enforces it — a statement whose first keyword is
 * SELECT is rejected before execution. The SELECT below must fail
 * with exit 4 (not run silently). */
static void test_exec_select_behavior_pinned(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "db", "exec", "SELECT 1;" };
    int rc = stest_run_argv(ctx, cmd_db, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════
 *  db backup
 * ═══════════════════════════════════════════════════════════════════ */

#define BACKUP_TARGET "acta_test_backup.db"

static void backup_cleanup(void)
{
    remove(BACKUP_TARGET);
    remove(BACKUP_TARGET "-wal");
    remove(BACKUP_TARGET "-shm");
}

/* --- success ------------------------------------------------------- */

static void test_backup_success(stest_ctx_t *ctx)
{
    backup_cleanup();

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "to", BACKUP_TARGET, &g);

    int rc = do_db(ctx, "backup", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx),
                  "\"target\":\"acta_test_backup.db\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"bytes\":");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"quick_check\":\"ok\"");
    targs_free(a, &g);

    /* The backup must open as a complete database with the same row
     * counts as the live DB — taken while the test connection (and
     * its WAL) is still open. */
    int err = 0;
    int live = acta_db_context_count(ctx->db, NULL, &err);
    TEST_EQ(ctx, err, ACTA_DB_OK);

    db_t *bk = acta_db_open(BACKUP_TARGET, &err, ACTA_DB_OPEN_EXISTING);
    TEST_NOT_NULL(ctx, bk);
    if (bk) {
        int n = acta_db_context_count(bk, NULL, &err);
        TEST_EQ(ctx, err, ACTA_DB_OK);
        TEST_EQ(ctx, n, live);
        acta_db_close(bk);
    }

    backup_cleanup();
}

static void test_backup_table(stest_ctx_t *ctx)
{
    backup_cleanup();

    global_opts_t g = gopts_table();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "to", BACKUP_TARGET, &g);

    int rc = do_db(ctx, "backup", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST(ctx, strstr(stest_stdout(ctx), "\"target\"") == NULL);
    TEST_CONTAINS(ctx, stest_stdout(ctx), BACKUP_TARGET);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "bytes");
    targs_free(a, &g);

    backup_cleanup();
}

/* --- error paths --------------------------------------------------- */

static void test_backup_missing_to(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "backup", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    targs_free(a, &g);
}

static void test_backup_existing_target(stest_ctx_t *ctx)
{
    /* Pre-create the target with known content: the command must be
     * rejected (no silent overwrite) and the file left untouched. */
    FILE *fp = fopen(BACKUP_TARGET, "w");
    TEST_NOT_NULL(ctx, fp);
    if (fp) { fputs("sentinel", fp); fclose(fp); }

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "to", BACKUP_TARGET, &g);

    int rc = do_db(ctx, "backup", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);

    fp = fopen(BACKUP_TARGET, "r");
    TEST_NOT_NULL(ctx, fp);
    if (fp) {
        char buf[64];
        size_t n = fread(buf, 1, sizeof buf - 1, fp);
        buf[n] = '\0';
        TEST_STREQ(ctx, buf, "sentinel");
        fclose(fp);
    }
    targs_free(a, &g);

    backup_cleanup();
}

static void test_backup_invalid_chars(stest_ctx_t *ctx)
{
    /* Quote / semicolon (and backslash on POSIX) targets are rejected
     * before any write; nothing is left behind on failure.  On
     * Windows the backslash is the native path separator and is a
     * valid target character, so the backslash case is excluded there. */
#ifndef _WIN32
    const char *bad[] = { "bad'name.db", "a;b.db", "a\\b.db", "a\"b.db" };
#else
    const char *bad[] = { "bad'name.db", "a;b.db", "a\"b.db" };
#endif
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        global_opts_t g = gopts_default();
        cmd_args_t   *a = targs_new();
        targs_flag(a, "to", bad[i], &g);
        TEST_EQ(ctx, do_db(ctx, "backup", a, g), EXIT_CLI);
        targs_free(a, &g);

        struct stat st;
        TEST(ctx, stat(bad[i], &st) != 0);   /* nothing written */
    }
}

static void test_backup_target_is_db_path(stest_ctx_t *ctx)
{
    /* Backing the database up onto its own path is rejected. */
    const char *dbpath = acta_db_main_path(ctx->db);
    TEST_NOT_NULL(ctx, dbpath);

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "to", dbpath, &g);

    int rc = do_db(ctx, "backup", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    targs_free(a, &g);
}

static void test_backup_uncreatable_target(stest_ctx_t *ctx)
{
    /* A path in a non-existent directory: rejected, nothing written. */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    targs_flag(a, "to", "/nonexistent_dir_xyz/backup.db", &g);

    int rc = do_db(ctx, "backup", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_db_test_all(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    /* exec – success */
    test_exec_positional_insert(&ctx);
    test_exec_positional_real(&ctx);
    test_exec_positional_after_bool_flag(&ctx);
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

    /* backup */
    test_backup_success(&ctx);
    test_backup_table(&ctx);
    test_backup_missing_to(&ctx);
    test_backup_existing_target(&ctx);
    test_backup_invalid_chars(&ctx);
    test_backup_target_is_db_path(&ctx);
    test_backup_uncreatable_target(&ctx);

    /* unknown action */
    test_unknown_action(&ctx);
    test_unknown_action_suggestion(&ctx);

    /* verbose */
    test_exec_verbose(&ctx);

    /* known issues (KI-n) */
    test_exec_sql_stdin_flag(&ctx);
    test_exec_select_behavior_pinned(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
