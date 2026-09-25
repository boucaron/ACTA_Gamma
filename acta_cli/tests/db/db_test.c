#include "test_helpers.h"
#include "migrations_sql.h"

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
/* ═══════════════════════════════════════════════════════════════════
 *  db migrate
 * ═══════════════════════════════════════════════════════════════════ */

#define MIGRATE_FRESH_DB "acta_test_migrate_fresh.db"
#define MIGRATE_FOREIGN_DB "acta_test_migrate_foreign.db"

static void test_migrate_fresh(stest_ctx_t *ctx)
{
    /* fresh empty file (user_version 0, no user tables): the 0.1
     * migration applies, user_version becomes 1, exit 0 */
    int err = 0;
    db_t *db = open_scratch(ctx, MIGRATE_FRESH_DB, &err);
    if (!db) return;

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_db("migrate", a, &g, db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"status\":\"ok\"");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"schema_version\":\"0.1\"");
    targs_free(a, &g);

    int uv = -1;
    TEST_EQ(ctx, acta_db_schema_version(db, &uv), ACTA_DB_OK);
    TEST_EQ(ctx, uv, 1);
    int n = 0, e2 = 0;
    char **tabs = acta_db_user_tables(db, &n, &e2);
    TEST_EQ(ctx, e2, ACTA_DB_OK);
    TEST_EQ(ctx, n, 9);   /* the 0.1 baseline schema tables */
    acta_db_user_tables_free(tabs, n);

    acta_db_close(db);
    scratch_cleanup(MIGRATE_FRESH_DB);
}

static void test_migrate_noop(stest_ctx_t *ctx)
{
    /* db init (schema + user_version 1) then db migrate: idempotent
     * no-op, exit 0, current version printed */
    int err = 0;
    db_t *db = open_scratch(ctx, MIGRATE_FRESH_DB, &err);
    if (!db) return;

    global_opts_t g = gopts_default();

    cmd_args_t *a = targs_new();
    stest_capture_begin(ctx);
    int rc = cmd_db("init", a, &g, db);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);

    a = targs_new();
    stest_capture_begin(ctx);
    rc = cmd_db("migrate", a, &g, db);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"schema_version\":\"0.1\"");
    targs_free(a, &g);

    acta_db_close(db);
    scratch_cleanup(MIGRATE_FRESH_DB);
}

static void test_migrate_foreign(stest_ctx_t *ctx)
{
    /* user_version 0 with a non-ACTA table: fail closed, exit 4,
     * nothing applied */
    int err = 0;
    db_t *db = open_scratch(ctx, MIGRATE_FOREIGN_DB, &err);
    if (!db) return;

    int e2 = acta_db_exec(db,
        "CREATE TABLE foreign_tbl (id INTEGER PRIMARY KEY)");
    TEST_EQ(ctx, e2, ACTA_DB_OK);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_db("migrate", a, &g, db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST(ctx, strstr(stest_stdout(ctx), "\"status\"") == NULL);
    int uv = -1;
    TEST_EQ(ctx, acta_db_schema_version(db, &uv), ACTA_DB_OK);
    TEST_EQ(ctx, uv, 0);   /* version unchanged */
    targs_free(a, &g);

    acta_db_close(db);
    scratch_cleanup(MIGRATE_FOREIGN_DB);
}

static void test_migrate_failing_migration(stest_ctx_t *ctx)
{
    /* fixture: 0.1 succeeds, 0.2 fails midway — 0.2 rolls back,
     * user_version keeps the prior value (1), the failure names the
     * migration */
    int err = 0;
    db_t *db = open_scratch(ctx, MIGRATE_FRESH_DB, &err);
    if (!db) return;

    static const acta_migration_t migs[] = {
        { 1, "CREATE TABLE m01_t (id INTEGER PRIMARY KEY);\n" },
        { 2, "CREATE TABLE m02_t (id INTEGER PRIMARY KEY);\n"
              "SELECT * FROM table_that_does_not_exist;\n" },
        { 0, NULL }   /* sentinel, not part of the count */
    };

    int uv = -1;
    char fail[256];
    int rc = db_migrate_apply(db, migs, 2, &uv, fail, sizeof fail);
    TEST_EQ(ctx, rc, ACTA_DB_ERR_INVALID);
    TEST(ctx, strstr(fail, "0.2") != NULL);
    TEST_EQ(ctx, uv, 1);   /* 0.1 applied; 0.2 rolled back */

    int n = 0, e2 = 0;
    char **tabs = acta_db_user_tables(db, &n, &e2);
    TEST_EQ(ctx, e2, ACTA_DB_OK);
    int have_01 = 0, have_02 = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(tabs[i], "m01_t") == 0) have_01 = 1;
        if (strcmp(tabs[i], "m02_t") == 0) have_02 = 1;
    }
    TEST_EQ(ctx, have_01, 1);
    TEST_EQ(ctx, have_02, 0);
    acta_db_user_tables_free(tabs, n);

    acta_db_close(db);
    scratch_cleanup(MIGRATE_FRESH_DB);
}

static void test_migrate_ordering(stest_ctx_t *ctx)
{
    /* fixture: two pending migrations apply in ascending order */
    int err = 0;
    db_t *db = open_scratch(ctx, MIGRATE_FRESH_DB, &err);
    if (!db) return;

    static const acta_migration_t migs[] = {
        { 1, "CREATE TABLE m_ord_1 (id INTEGER PRIMARY KEY);\n" },
        { 2, "CREATE TABLE m_ord_2 (id INTEGER PRIMARY KEY);\n" },
        { 0, NULL }
    };

    int uv = -1;
    char fail[256];
    int rc = db_migrate_apply(db, migs, 2, &uv, fail, sizeof fail);
    TEST_EQ(ctx, rc, ACTA_DB_OK);
    TEST_EQ(ctx, uv, 2);

    int n = 0, e2 = 0;
    char **tabs = acta_db_user_tables(db, &n, &e2);
    TEST_EQ(ctx, e2, ACTA_DB_OK);
    int have_1 = 0, have_2 = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(tabs[i], "m_ord_1") == 0) have_1 = 1;
        if (strcmp(tabs[i], "m_ord_2") == 0) have_2 = 1;
    }
    TEST_EQ(ctx, have_1, 1);
    TEST_EQ(ctx, have_2, 1);
    acta_db_user_tables_free(tabs, n);

    acta_db_close(db);
    scratch_cleanup(MIGRATE_FRESH_DB);
}

static void test_migrate_rejects_input(stest_ctx_t *ctx)
{
    /* db migrate takes no SQL input: positional / --sql / --file /
     * --sql_stdin / global --stdin are all rejected with exit 4 */
    global_opts_t g = gopts_default();

    cmd_args_t *a = targs_new();
    targs_pos(a, "ALTER TABLE models ADD COLUMN x TEXT;", &g);
    TEST_EQ(ctx, do_db(ctx, "migrate", a, g), EXIT_INVALID);
    targs_free(a, &g);

    a = targs_new();
    targs_flag(a, "sql", "SELECT 1;", &g);
    TEST_EQ(ctx, do_db(ctx, "migrate", a, g), EXIT_INVALID);
    targs_free(a, &g);

    a = targs_new();
    targs_flag(a, "file", "x.sql", &g);
    TEST_EQ(ctx, do_db(ctx, "migrate", a, g), EXIT_INVALID);
    targs_free(a, &g);

    a = targs_new();
    targs_flag_bool(a, "sql_stdin", &g);
    TEST_EQ(ctx, do_db(ctx, "migrate", a, g), EXIT_INVALID);
    targs_free(a, &g);

    g.from_stdin = 1;
    a = targs_new();
    TEST_EQ(ctx, do_db(ctx, "migrate", a, g), EXIT_INVALID);
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
    TEST_CONTAINS(ctx, stest_stdout(ctx), "migrate");
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

    /* migrate */
    test_migrate_fresh(&ctx);
    test_migrate_noop(&ctx);
    test_migrate_foreign(&ctx);
    test_migrate_failing_migration(&ctx);
    test_migrate_ordering(&ctx);
    test_migrate_rejects_input(&ctx);

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
