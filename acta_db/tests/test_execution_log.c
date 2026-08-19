/* test_execution_log.c — Tests for execution_log.h (tests 10.1 – 10.16) */

#include "test_common.h"
#include "execution_log.h"
#include <sqlite3.h>

/* ---------- helpers ---------- */

/* Insert an execution row, temporarily disabling FK checks.
 * Returns 0 on success, non-zero on failure. */
static int el_insert_execution(db_t *db, int execution_id) {
    acta_db_exec(db, "PRAGMA foreign_keys=OFF;");
    char sql[192];
    snprintf(sql, sizeof(sql),
             "INSERT INTO executions (id, context_id, skill_revision_id, model_revision_id) "
             "VALUES (%d, 1, 1, 1);", execution_id);
    int rc = acta_db_exec(db, sql);
    acta_db_exec(db, "PRAGMA foreign_keys=ON;");
    return rc;
}

/* Create a log entry; returns id on success, -1 on failure */
static int el_create_log(db_t *db, int execution_id, const char *level,
                         const char *event, const char *message,
                         const char *metadata) {
    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = execution_id;
    log.level = (char *)level;
    log.event = (char *)event;
    log.message = (char *)message;
    log.metadata = (char *)metadata;

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    return (rc == 0) ? id : -1;
}

/* Count logs for a given execution_id */
static int el_count_logs(db_t *db, int execution_id) {
    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, execution_id, &count);
    if (logs) {
        acta_db_execution_log_list_free(logs, count);
    }
    return count;
}

static int el_raw_delete_execution(db_t *db, int execution_id) {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM executions WHERE id = %d", execution_id);
    return acta_db_exec(db, sql);
}


/* ---------- 10.1: create — happy ---------- */
static void test_el_create_happy(void) {
    const char *path = "test/acta_test_el_create_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    execution_log_t log = {
        .execution_id = 1,
        .level        = "info",
        .event        = "step_completed",
        .message      = "All good",
        .metadata     = NULL,
    };
    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* ---------- 10.2: create — invalid execution_id ---------- */
static void test_el_create_invalid_execution(void) {
    const char *path = "test/acta_test_el_create_badexec.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = 999999;
    log.level = (char *)"info";
    log.event = (char *)"event";
    log.message = (char *)"msg";
    log.metadata = NULL;

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 10.3: create — invalid level ---------- */
static void test_el_create_invalid_level(void) {
    const char *path = "test/acta_test_el_create_badlevel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = 1;
    log.level = (char *)"verbose";
    log.event = (char *)"event";
    log.message = (char *)"msg";
    log.metadata = NULL;

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 10.4: create — all valid levels ---------- */
static void test_el_create_all_valid_levels(void) {
    const char *path = "test/acta_test_el_create_levels.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id_debug = el_create_log(db, 1, "debug", "dbg_event", "debug msg", NULL);
    TEST_ASSERT(id_debug > 0);

    int id_info = el_create_log(db, 1, "info", "info_event", "info msg", NULL);
    TEST_ASSERT(id_info > 0);

    int id_warn = el_create_log(db, 1, "warn", "warn_event", "warn msg", NULL);
    TEST_ASSERT(id_warn > 0);

    int id_error = el_create_log(db, 1, "error", "error_event", "error msg", NULL);
    TEST_ASSERT(id_error > 0);

    test_db_teardown(db, path);
}


/* ---------- 10.5: create — NULL event ---------- */
static void test_el_create_null_event(void) {
    const char *path = "test/acta_test_el_create_nullevent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = 1;
    log.level = (char *)"info";
    log.event = NULL;
    log.message = (char *)"msg";
    log.metadata = NULL;

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT(rc < 0);

    test_db_teardown(db, path);
}

/* ---------- 10.6: create — NULL message ---------- */
static void test_el_create_null_message(void) {
    const char *path = "test/acta_test_el_create_nullmsg.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, "info", "event", NULL, NULL);
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* ---------- 10.7: create — NULL metadata ---------- */
static void test_el_create_null_metadata(void) {
    const char *path = "test/acta_test_el_create_nullmeta.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, "info", "event", "msg", NULL);
    TEST_ASSERT(id > 0);

    test_db_teardown(db, path);
}

/* ---------- 10.8: list_by_execution — multiple logs ---------- */
static void test_el_list_multiple(void) {
    const char *path = "test/acta_test_el_list_multi.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 5; i++) {
        int id = el_create_log(db, 1, "info", "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    int count = el_count_logs(db, 1);
    TEST_ASSERT_EQ_INT(count, 5);

    test_db_teardown(db, path);
}

/* ---------- 10.9: list_by_execution — ordering ---------- */
static void test_el_list_ordering(void) {
    const char *path = "test/acta_test_el_list_order.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id1 = el_create_log(db, 1, "info", "first", "msg1", NULL);
    int id2 = el_create_log(db, 1, "info", "second", "msg2", NULL);
    int id3 = el_create_log(db, 1, "info", "third", "msg3", NULL);
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);
    TEST_ASSERT(id3 > 0);

    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, 1, &count);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);

    /* Verify ordering: id should be monotonically increasing */
    TEST_ASSERT(logs[0].id <= logs[1].id);
    TEST_ASSERT(logs[1].id <= logs[2].id);

    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.10: list_by_execution — no logs ---------- */
static void test_el_list_no_logs(void) {
    const char *path = "test/acta_test_el_list_nologs.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, 42, &count);
    TEST_ASSERT_EQ_INT(count, 0);

    /* logs may be NULL or a valid (empty) array */
    if (logs) {
        acta_db_execution_log_list_free(logs, count);
    }

    test_db_teardown(db, path);
}

/* ---------- 10.11: list_by_execution — non-existent execution ---------- */
static void test_el_list_nonexistent_execution(void) {
    const char *path = "test/acta_test_el_list_nonexist.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, 999999, &count);
    TEST_ASSERT_EQ_INT(count, 0);

    if (logs) {
        acta_db_execution_log_list_free(logs, count);
    }

    test_db_teardown(db, path);
}

/* ---------- 10.12: list_by_execution — excludes other executions ---------- */
static void test_el_list_excludes_others(void) {
    const char *path = "test/acta_test_el_list_exclude.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    el_insert_execution(db, 2);

    /* Create logs for execution 1 */
    int id_a1 = el_create_log(db, 1, "info", "event_a1", "msg", NULL);
    int id_a2 = el_create_log(db, 1, "info", "event_a2", "msg", NULL);
    TEST_ASSERT(id_a1 > 0);
    TEST_ASSERT(id_a2 > 0);

    /* Create logs for execution 2 */
    int id_b1 = el_create_log(db, 2, "info", "event_b1", "msg", NULL);
    TEST_ASSERT(id_b1 > 0);

    /* List execution 1 — should only contain id_a1 and id_a2 */
    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, 1, &count);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 2);

    /* Verify execution 2's log is NOT present */
    for (int i = 0; i < count; i++) {
        TEST_ASSERT(logs[i].id != id_b1);
        TEST_ASSERT_EQ_INT(logs[i].execution_id, 1);
    }

    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.13: free — valid ---------- */
static void test_el_free_valid(void) {
    const char *path = "test/acta_test_el_free_valid.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, "info", "event", "msg", NULL);
    TEST_ASSERT(id > 0);

    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, 1, &count);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 1);

    acta_db_execution_log_free(logs);
    TEST_ASSERT(1); /* no crash */

    test_db_teardown(db, path);
}

/* ---------- 10.14: free — NULL ---------- */
static void test_el_free_null(void) {
    const char *path = "test/acta_test_el_free_null.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    acta_db_execution_log_free(NULL);
    TEST_ASSERT(1); /* no crash */

    test_db_teardown(db, path);
}

/* ---------- 10.15: list_free — valid ---------- */
static void test_el_list_free_valid(void) {
    const char *path = "test/acta_test_el_list_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 4; i++) {
        int id = el_create_log(db, 1, "info", "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    int count = 0;
    execution_log_t *logs = acta_db_execution_log_list_by_execution(db, 1, &count);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);

    acta_db_execution_log_list_free(logs, count);
    TEST_ASSERT(1); /* no crash */

    test_db_teardown(db, path);
}

/* ---------- 10.16: CASCADE delete ---------- */
static void test_el_cascade_delete(void) {
    const char *path = "test/acta_test_el_cascade.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    /* Create logs for execution 1 */
    int id1 = el_create_log(db, 1, "info", "event1", "msg", NULL);
    int id2 = el_create_log(db, 1, "warn", "event2", "msg", NULL);
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);

    int count_before = el_count_logs(db, 1);
    TEST_ASSERT_EQ_INT(count_before, 2);

    /* Delete the execution row via raw SQL (CASCADE should remove logs) */
    int rc = el_raw_delete_execution(db, 1);
    TEST_ASSERT_EQ_INT(rc, 0);

    /* Logs should be gone due to CASCADE */
    int count_after = el_count_logs(db, 1);
    TEST_ASSERT_EQ_INT(count_after, 0);

    test_db_teardown(db, path);
}

/* ---------- runner ---------- */
void run_execution_log_tests(void) {
    fprintf(stderr, "\n=== execution_log tests ===\n");
    test_el_create_happy();
    test_el_create_invalid_execution();
    test_el_create_invalid_level();
    test_el_create_all_valid_levels();
    test_el_create_null_event();
    test_el_create_null_message();
    test_el_create_null_metadata();
    test_el_list_multiple();
    test_el_list_ordering();
    test_el_list_no_logs();
    test_el_list_nonexistent_execution();
    test_el_list_excludes_others();
    test_el_free_valid();
    test_el_free_null();
    test_el_list_free_valid();
    test_el_cascade_delete();
}
