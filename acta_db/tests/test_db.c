#include "test_common.h"

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
    acta_db_close(db1);
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
    acta_db_close(db);
    remove(path);
}

/* ---------- 1.6: acta_db_close — NULL handle ---------- */
static void test_db_close_null(void) {
    acta_db_close(NULL);
    TEST_ASSERT(1); /* didn't crash */
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

void run_db_tests(void) {
    fprintf(stderr, "\n=== db.h tests ===\n");
    test_db_open_new();
    test_db_open_existing();
    test_db_open_invalid_path();
    test_db_open_null_path();
    test_db_close_valid();
    test_db_close_null();
    test_db_exec_valid_ddl();
    test_db_exec_invalid_sql();
    test_db_exec_null_sql();
    test_db_last_error_after_success();
    test_db_last_error_after_failure();
    test_db_transaction_commit();
    test_db_transaction_rollback_callback();
    test_db_transaction_rollback_sql_error();
    test_db_transaction_user_data();
}
