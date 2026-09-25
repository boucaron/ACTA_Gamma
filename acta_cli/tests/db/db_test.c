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
 *  db init
 * ═══════════════════════════════════════════════════════════════════ */

#define INIT_FRESH_DB "acta_test_init_fresh.db"
#define INIT_PARTIAL_DB "acta_test_init_partial.db"
#define INIT_FOREIGN_DB "acta_test_init_foreign.db"

static void scratch_cleanup(const char *path)
{
    char p[256];
    snprintf(p, sizeof p, "%s", path);
    remove(p);
    snprintf(p, sizeof p, "%s-wal", path);
    remove(p);
    snprintf(p, sizeof p, "%s-shm", path);
    remove(p);
}

/* open a scratch connection on `path` (fresh file, schema not applied) */
static db_t *open_scratch(stest_ctx_t *ctx, const char *path, int *err)
{
    scratch_cleanup(path);
    db_t *db = acta_db_open(path, err, ACTA_DB_OPEN_CREATE);
    TEST_NOT_NULL(ctx, db);
    return db;
}

static void test_init_fresh(stest_ctx_t *ctx)
{
    int err = 0;
    db_t *db = open_scratch(ctx, INIT_FRESH_DB, &err);
    if (!db) return;

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_db("init", a, &g, db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"ok\"");
    targs_free(a, &g);

    /* the schema must actually have been applied */
    int n = 0, e2 = 0;
    char **tabs = acta_db_user_tables(db, &n, &e2);
    TEST_EQ(ctx, e2, ACTA_DB_OK);
    TEST_EQ(ctx, n, 9);
    acta_db_user_tables_free(tabs, n);

    acta_db_close(db);
    scratch_cleanup(INIT_FRESH_DB);
}

static void test_init_fresh_table(stest_ctx_t *ctx)
{
    int err = 0;
    db_t *db = open_scratch(ctx, INIT_FRESH_DB, &err);
    if (!db) return;

    global_opts_t g = gopts_table();
    cmd_args_t   *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_db("init", a, &g, db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST(ctx, strstr(stest_stdout(ctx), "\"status\"") == NULL);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "ok");
    targs_free(a, &g);

    acta_db_close(db);
    scratch_cleanup(INIT_FRESH_DB);
}

static void test_init_noop(stest_ctx_t *ctx)
{
    /* REF_DB is already schema'd: re-running must be a no-op, exit 0 */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "init", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"ok\"");
    targs_free(a, &g);
}

static void test_init_partial(stest_ctx_t *ctx)
{
    /* one canonical table, the rest missing: fail closed, exit 4 */
    int err = 0;
    db_t *db = open_scratch(ctx, INIT_PARTIAL_DB, &err);
    if (!db) return;

    int e2 = acta_db_exec(db,
        "CREATE TABLE models (id INTEGER PRIMARY KEY AUTOINCREMENT)");
    TEST_EQ(ctx, e2, ACTA_DB_OK);

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_db("init", a, &g, db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST(ctx, strstr(stest_stdout(ctx), "\"status\"") == NULL);
    targs_free(a, &g);

    acta_db_close(db);
    scratch_cleanup(INIT_PARTIAL_DB);
}
static void test_init_foreign(stest_ctx_t *ctx)
{
    /* unrelated table only: fail closed, exit 4 */
    int err = 0;
    db_t *db = open_scratch(ctx, INIT_FOREIGN_DB, &err);
    if (!db) return;

    int e2 = acta_db_exec(db,
        "CREATE TABLE foreign_tbl (id INTEGER PRIMARY KEY)");
    TEST_EQ(ctx, e2, ACTA_DB_OK);

    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_db("init", a, &g, db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);

    acta_db_close(db);
    scratch_cleanup(INIT_FOREIGN_DB);
}

static void test_init_rejects_input(stest_ctx_t *ctx)
{
    /* db init takes no SQL input: positional / --sql / --file /
     * --sql_stdin / global --stdin are all rejected with exit 4 */
    global_opts_t g = gopts_default();

    cmd_args_t *a = targs_new();
    targs_pos(a, "SELECT 1;", &g);
    TEST_EQ(ctx, do_db(ctx, "init", a, g), EXIT_INVALID);
    targs_free(a, &g);

    a = targs_new();
    targs_flag(a, "sql", "SELECT 1;", &g);
    TEST_EQ(ctx, do_db(ctx, "init", a, g), EXIT_INVALID);
    targs_free(a, &g);

    a = targs_new();
    targs_flag(a, "file", "x.sql", &g);
    TEST_EQ(ctx, do_db(ctx, "init", a, g), EXIT_INVALID);
    targs_free(a, &g);

    a = targs_new();
    targs_flag_bool(a, "sql_stdin", &g);
    TEST_EQ(ctx, do_db(ctx, "init", a, g), EXIT_INVALID);
    targs_free(a, &g);

    g.from_stdin = 1;
    a = targs_new();
    TEST_EQ(ctx, do_db(ctx, "init", a, g), EXIT_INVALID);
    targs_free(a, &g);
}

static void test_exec_removed(stest_ctx_t *ctx)
{
    /* db exec no longer exists: every form is an unknown action,
     * exit 10 (same as any other rejected action). */
    global_opts_t g = gopts_default();

    cmd_args_t *a = targs_new();
    targs_pos(a, "SELECT 1;", &g);
    TEST_EQ(ctx, do_db(ctx, "exec", a, g), EXIT_CLI);
    targs_free(a, &g);

    a = targs_new();
    targs_flag(a, "sql", "SELECT 1;", &g);
    TEST_EQ(ctx, do_db(ctx, "exec", a, g), EXIT_CLI);
    targs_free(a, &g);

    a = targs_new();
    targs_flag(a, "file", "x.sql", &g);
    TEST_EQ(ctx, do_db(ctx, "exec", a, g), EXIT_CLI);
    targs_free(a, &g);

    a = targs_new();
    targs_flag_bool(a, "sql_stdin", &g);
    TEST_EQ(ctx, do_db(ctx, "exec", a, g), EXIT_CLI);
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
    TEST_CONTAINS(ctx, stest_stdout(ctx), "init");
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
    /* "initt" is one char off "init" – closest_match should suggest it */
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();

    int rc = do_db(ctx, "initt", a, g);
    TEST_EQ(ctx, rc, EXIT_CLI);
    /* suggestion goes to stderr, not captured stdout; just check rc */
    targs_free(a, &g);
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

    /* init */
    test_init_fresh(&ctx);
    test_init_fresh_table(&ctx);
    test_init_noop(&ctx);
    test_init_partial(&ctx);
    test_init_foreign(&ctx);
    test_init_rejects_input(&ctx);
    test_exec_removed(&ctx);

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


    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
