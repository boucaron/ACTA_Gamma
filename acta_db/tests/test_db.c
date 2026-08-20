#include "test_common.h"
#include "internal.h"


/* ---------- 1.1: acta_db_open — new file ---------- */
static void test_db_open_new(void) {
    const char *path = "test/acta_test_new.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    test_db_teardown(db, path);
}

/* ---------- 1.2: acta_db_open — existing file ---------- */
static void test_db_open_existing(void) {
    const char *path = "test/acta_test_existing.db";
    remove(path);
    db_t *db1 = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db1);
    int rc = acta_db_close(db1);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    db_t *db2 = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db2);
    test_db_teardown(db2, path);
}

/* ---------- 1.3: acta_db_open — invalid path ---------- */
static void test_db_open_invalid_path(void) {
    int err = 0;
    db_t *db = acta_db_open("/nonexistent/dir/sub/file.db", &err);
    TEST_ASSERT_NULL(db);
    TEST_ASSERT(err < 0);
}

/* ---------- 1.4: acta_db_open — NULL path ---------- */
static void test_db_open_null_path(void) {
    int err = 0;
    db_t *db = acta_db_open(NULL, &err);
    TEST_ASSERT_NULL(db);
    TEST_ASSERT(err < 0);
}

/* ---------- 1.5: acta_db_close — valid handle ---------- */
static void test_db_close_valid(void) {
    const char *path = "test/acta_test_close.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int rc = acta_db_close(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    remove(path);
}

/* ---------- 1.6: acta_db_close — NULL handle ---------- */
static void test_db_close_null(void) {
    int rc = acta_db_close(NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ---------- 1.6b: acta_db_close — outstanding statements ---------- */
static void test_db_close_outstanding_stmts(void) {
    const char *path = "test/acta_test_close_busy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    acta_db_exec(db, "CREATE TABLE t (id INTEGER);");

    /* Prepare a statement and leave it unfinalized. */
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;
    TEST_ASSERT_EQ_INT(
        sqlite3_prepare_v2(db->handle, "SELECT 1;", -1, &stmt, &tail),
        SQLITE_OK);

    /* Close with an outstanding statement — should fail. */
    int rc = acta_db_close(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);

    /* Finalize the statement so SQLite can release the handle. */
    sqlite3_finalize(stmt);
    remove(path);
}

/* ---------- 1.6c: acta_db_close — in-transaction (implicit rollback) ---------- */
static void test_db_close_implicit_rollback(void) {
    const char *path = "test/acta_test_close_txn.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    acta_db_exec(db, "CREATE TABLE t (id INTEGER);");

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    acta_db_exec(db, "INSERT INTO t(id) VALUES(1);");

    /* Close without explicit commit/rollback — implicit rollback applies. */
    int rc = acta_db_close(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    remove(path);
}

/* ---------- 1.7: acta_db_exec — valid DDL ---------- */
static void test_db_exec_valid_ddl(void) {
    const char *path = "test/acta_test_exec.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int rc = acta_db_exec(db, "CREATE TABLE test_table (id INTEGER PRIMARY KEY);");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    test_db_teardown(db, path);
}

/* ---------- 1.8: acta_db_exec — invalid SQL ---------- */
static void test_db_exec_invalid_sql(void) {
    const char *path = "test/acta_test_exec_inv.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int rc = acta_db_exec(db, "SELEKT 1;");
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 1.9: acta_db_exec — NULL SQL ---------- */
static void test_db_exec_null_sql(void) {
    const char *path = "test/acta_test_exec_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int rc = acta_db_exec(db, NULL);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 1.10: acta_db_last_error — after success ---------- */
static void test_db_last_error_after_success(void) {
    const char *path = "test/acta_test_err_ok.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    acta_db_exec(db, "CREATE TABLE t (id INTEGER);");
    const char *err = acta_db_last_error(db);
    /* After success, should be NULL or empty */
    TEST_ASSERT(err == NULL || strlen(err) == 0);
    test_db_teardown(db, path);
}

/* ---------- 1.11: acta_db_last_error — after failure ---------- */
static void test_db_last_error_after_failure(void) {
    const char *path = "test/acta_test_err_fail.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    acta_db_exec(db, "INVALID SQL STATEMENT;");
    const char *err = acta_db_last_error(db);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT(strlen(err) > 0);
    test_db_teardown(db, path);
}

/* ---------- 1.12: acta_db_transaction — commit ---------- */
static int txn_callback_insert(db_t *db, void *user_data) {
    (void)user_data;
    return acta_db_exec(db,
        "INSERT INTO contexts(type, content, content_hash) "
        "VALUES('t','c','h');");
}

static void test_db_transaction_commit(void) {
    const char *path = "test/acta_test_txn_commit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    /* Create the table the callback expects */
    acta_db_exec(db,
        "CREATE TABLE contexts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type TEXT NOT NULL,"
        "  content TEXT NOT NULL,"
        "  content_hash TEXT NOT NULL,"
        "  metadata TEXT,"
        "  created_at TEXT"
        ");");

    int rc = acta_db_transaction(db, txn_callback_insert, NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    test_db_teardown(db, path);
}

/* ---------- 1.13: acta_db_transaction — rollback on callback failure ---------- */
static int txn_callback_fail(db_t *db, void *user_data) {
    (void)db;
    (void)user_data;
    return -1;
}

static void test_db_transaction_rollback_callback(void) {
    const char *path = "test/acta_test_txn_rb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int rc = acta_db_transaction(db, txn_callback_fail, NULL);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 1.14: acta_db_transaction — rollback on SQL error ---------- */
static int txn_callback_bad_sql(db_t *db, void *user_data) {
    (void)user_data;
    return acta_db_exec(db, "THIS IS NOT SQL AT ALL;");
}

static void test_db_transaction_rollback_sql_error(void) {
    const char *path = "test/acta_test_txn_sqlerr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int rc = acta_db_transaction(db, txn_callback_bad_sql, NULL);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 1.15: acta_db_transaction — user_data passthrough ---------- */
static int txn_callback_user_data(db_t *db, void *user_data) {
    (void)db;
    int *val = (int *)user_data;
    *val = 42;
    return 0;
}

static void test_db_transaction_user_data(void) {
    const char *path = "test/acta_test_txn_ud.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    int val = 0;
    int rc = acta_db_transaction(db, txn_callback_user_data, &val);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(val, 42);
    test_db_teardown(db, path);
}

/* ================================================================== */
/*  2.x: Application-controlled transactions (begin/commit/rollback)  */
/* ================================================================== */

/* Helper: create a simple table for txn tests. */
static void create_simple_table(db_t *db) {
    acta_db_exec(db,
        "CREATE TABLE items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL"
        ");");
}

/* Helper: count rows in items table. */
static int count_items(db_t *db) {
    const char *tail = NULL;
    sqlite3_stmt *stmt;
    int count = 0;
    if (sqlite3_prepare_v2(db->handle, "SELECT COUNT(*) FROM items;", -1, &stmt, &tail) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    /* tail is not heap-allocated — do NOT free it */
    return count;
}


/* ---------- 2.1: acta_db_begin — successful ---------- */
static void test_db_begin_success(void) {
    const char *path = "test/acta_test_begin_ok.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    int rc = acta_db_begin(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    /* Clean up with a rollback so the txn flag is cleared before close. */
    acta_db_rollback(db);
    test_db_teardown(db, path);
}

/* ---------- 2.2: acta_db_begin — NULL handle ---------- */
static void test_db_begin_null(void) {
    int rc = acta_db_begin(NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ---------- 2.3: acta_db_begin — nested (already in txn) ---------- */
static void test_db_begin_nested(void) {
    const char *path = "test/acta_test_begin_nested.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    int rc = acta_db_begin(db);  /* second BEGIN while first is active */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    acta_db_rollback(db);
    test_db_teardown(db, path);
}

/* ---------- 2.4: acta_db_commit — successful (data persists) ---------- */
static void test_db_commit_persists(void) {
    const char *path = "test/acta_test_commit_p.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    acta_db_exec(db, "INSERT INTO items(name) VALUES('alpha');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('beta');");

    int rc = acta_db_commit(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count_items(db), 2);

    test_db_teardown(db, path);
}

/* ---------- 2.5: acta_db_commit — NULL handle ---------- */
static void test_db_commit_null(void) {
    int rc = acta_db_commit(NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ---------- 2.6: acta_db_commit — no transaction in progress ---------- */
static void test_db_commit_no_txn(void) {
    const char *path = "test/acta_test_commit_notxn.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    int rc = acta_db_commit(db);  /* never called begin */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 2.7: acta_db_rollback — successful (data discarded) ---------- */
static void test_db_rollback_discards(void) {
    const char *path = "test/acta_test_rollback_d.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    /* Pre-existing row (committed before txn). */
    acta_db_exec(db, "INSERT INTO items(name) VALUES('existing');");
    TEST_ASSERT_EQ_INT(count_items(db), 1);

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    acta_db_exec(db, "INSERT INTO items(name) VALUES('transient');");
    /* Inside txn we see 2 rows. */
    TEST_ASSERT_EQ_INT(count_items(db), 2);

    int rc = acta_db_rollback(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    /* After rollback only the pre-existing row remains. */
    TEST_ASSERT_EQ_INT(count_items(db), 1);

    test_db_teardown(db, path);
}

/* ---------- 2.8: acta_db_rollback — NULL handle ---------- */
static void test_db_rollback_null(void) {
    int rc = acta_db_rollback(NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
}

/* ---------- 2.9: acta_db_rollback — no transaction in progress ---------- */
static void test_db_rollback_no_txn(void) {
    const char *path = "test/acta_test_rollback_notxn.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    int rc = acta_db_rollback(db);  /* never called begin */
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 2.10: implicit rollback on close ---------- */
static void test_db_implicit_rollback_on_close(void) {
    const char *path = "test/acta_test_implicit_rb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    acta_db_exec(db, "INSERT INTO items(name) VALUES('committed');");
    TEST_ASSERT_EQ_INT(count_items(db), 1);

    /* Begin txn, insert, then close WITHOUT commit/rollback. */
    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    acta_db_exec(db, "INSERT INTO items(name) VALUES('uncommitted');");
    int rc = acta_db_close(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    /* Re-open and verify only the committed row survived. */
    db_t *db2 = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db2);
    TEST_ASSERT_EQ_INT(count_items(db2), 1);
    test_db_teardown(db2, path);
}

/* ---------- 2.11: multi-operation commit (simulates multi-file import) ---------- */
static void test_db_multi_op_commit(void) {
    const char *path = "test/acta_test_multi_commit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);

    /* Simulate 3 "files" each inserting rows. */
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file1_a');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file1_b');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file2_a');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file3_a');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file3_b');");

    int rc = acta_db_commit(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count_items(db), 5);

    test_db_teardown(db, path);
}

/* ---------- 2.12: multi-operation rollback (aborted import) ---------- */
static void test_db_multi_op_rollback(void) {
    const char *path = "test/acta_test_multi_rb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    acta_db_exec(db, "INSERT INTO items(name) VALUES('pre');");

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file1_a');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file2_a');");
    acta_db_exec(db, "INSERT INTO items(name) VALUES('file3_a');");

    /* Simulate an error detected after file 2 — abort. */
    int rc = acta_db_rollback(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count_items(db), 1);  /* only 'pre' survives */

    test_db_teardown(db, path);
}

/* ---------- 2.13: begin → exec error → rollback recovers ---------- */
static void test_db_exec_error_then_rollback(void) {
    const char *path = "test/acta_test_err_rb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    acta_db_exec(db, "INSERT INTO items(name) VALUES('before');");

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);
    acta_db_exec(db, "INSERT INTO items(name) VALUES('good');");
    int rc = acta_db_exec(db, "GARBAGE SQL;");
    TEST_ASSERT(rc < 0);  /* SQL error inside txn */

    rc = acta_db_rollback(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count_items(db), 1);  /* 'before' only */

    /* Connection is still usable after rollback. */
    acta_db_exec(db, "INSERT INTO items(name) VALUES('after');");
    TEST_ASSERT_EQ_INT(count_items(db), 2);

    test_db_teardown(db, path);
}

/* ---------- 2.14: acta_db_transaction — nesting rejected ---------- */
static int txn_callback_harmless(db_t *db, void *user_data) {
    (void)db; (void)user_data;
    return 0;
}

static void test_db_transaction_nesting_rejected(void) {
    const char *path = "test/acta_test_txn_nest.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    TEST_ASSERT_EQ_INT(acta_db_begin(db), ACTA_DB_OK);

    /* Trying to run the callback-style txn inside an active txn must fail. */
    int rc = acta_db_transaction(db, txn_callback_harmless, NULL);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    acta_db_rollback(db);
    test_db_teardown(db, path);
}

/* ---------- 2.15: acta_db_close — returns ACTA_DB_OK when no txn ---------- */
static void test_db_close_returns_ok(void) {
    const char *path = "test/acta_test_close_rc.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    create_simple_table(db);

    int rc = acta_db_close(db);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    remove(path);
}

/* ================================================================== */
/*  Runner                                                            */
/* ================================================================== */

void run_db_tests(void) {
    fprintf(stderr, "\n=== db.h tests ===\n");
    test_db_open_new();
    test_db_open_existing();
    test_db_open_invalid_path();
    test_db_open_null_path();
    test_db_close_valid();
    test_db_close_null();
    test_db_close_outstanding_stmts();
    test_db_close_implicit_rollback();
    test_db_exec_valid_ddl();
    test_db_exec_invalid_sql();
    test_db_exec_null_sql();
    test_db_last_error_after_success();
    test_db_last_error_after_failure();
    test_db_transaction_commit();
    test_db_transaction_rollback_callback();
    test_db_transaction_rollback_sql_error();
    test_db_transaction_user_data();
    /* --- 2.x: begin / commit / rollback --- */
    test_db_begin_success();
    test_db_begin_null();
    test_db_begin_nested();
    test_db_commit_persists();
    test_db_commit_null();
    test_db_commit_no_txn();
    test_db_rollback_discards();
    test_db_rollback_null();
    test_db_rollback_no_txn();
    test_db_implicit_rollback_on_close();
    test_db_multi_op_commit();
    test_db_multi_op_rollback();
    test_db_exec_error_then_rollback();
    test_db_transaction_nesting_rejected();
    test_db_close_returns_ok();
}
