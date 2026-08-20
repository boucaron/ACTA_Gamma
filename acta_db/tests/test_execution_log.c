/* test_execution_log.c — Tests for acta_db_execution_log.h (tests 10.1 – 10.21) */

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
    return (rc == ACTA_DB_OK) ? id : -1;
}

/* Count logs for a given execution_id (fetches all rows: offset=0, limit=-1) */
static int el_count_logs(db_t *db, int execution_id) {
    int count = 0, err = 0;
    execution_log_t **items = acta_db_execution_log_list_by_execution(
        db, execution_id, 0, -1, &count, &err);
    if (items) {
        acta_db_execution_log_list_free(items, count);
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

    execution_log_t log = {0};
    log.execution_id = 1;
    ACTA_LOG_LEVEL_SET(log.level, INFO);
    log.event        = "step_completed";
    log.message      = "All good";

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
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
    ACTA_LOG_LEVEL_SET(log.level, INFO);
    log.event        = "event";
    log.message      = "msg";

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT(rc != ACTA_DB_OK);

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
    log.level = (char *)"verbose";   /* intentionally not a valid constant */
    log.event        = "event";
    log.message      = "msg";

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 10.4: create — all valid levels ---------- */
static void test_el_create_all_valid_levels(void) {
    const char *path = "test/acta_test_el_create_levels.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id_debug = el_create_log(db, 1, ACTA_LOG_LEVEL_DEBUG, "dbg_event", "debug msg", NULL);
    TEST_ASSERT(id_debug > 0);

    int id_info = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "info_event", "info msg", NULL);
    TEST_ASSERT(id_info > 0);

    int id_warn = el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "warn_event", "warn msg", NULL);
    TEST_ASSERT(id_warn > 0);

    int id_error = el_create_log(db, 1, ACTA_LOG_LEVEL_ERROR, "error_event", "error msg", NULL);
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
    ACTA_LOG_LEVEL_SET(log.level, INFO);
    log.event   = NULL;
    log.message = "msg";

    int id = 0;
    int rc = acta_db_execution_log_create(db, &log, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 10.6: create — NULL message ---------- */
static void test_el_create_null_message(void) {
    const char *path = "test/acta_test_el_create_nullmsg.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", NULL, NULL);
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

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
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
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
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

    int id1 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "first",  "msg1", NULL);
    int id2 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "second", "msg2", NULL);
    int id3 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "third",  "msg3", NULL);
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);
    TEST_ASSERT(id3 > 0);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);

    /* Verify ordering: id should be monotonically increasing */
    TEST_ASSERT(logs[0]->id <= logs[1]->id);
    TEST_ASSERT(logs[1]->id <= logs[2]->id);

    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.10: list_by_execution — no logs ---------- */
static void test_el_list_no_logs(void) {
    const char *path = "test/acta_test_el_list_nologs.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 42, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);          /* not-found → NULL, no allocation to free */

    test_db_teardown(db, path);
}

/* ---------- 10.11: list_by_execution — non-existent execution ---------- */
static void test_el_list_nonexistent_execution(void) {
    const char *path = "test/acta_test_el_list_nonexist.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 999999, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

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

    int id_a1 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event_a1", "msg", NULL);
    int id_a2 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event_a2", "msg", NULL);
    TEST_ASSERT(id_a1 > 0);
    TEST_ASSERT(id_a2 > 0);

    int id_b1 = el_create_log(db, 2, ACTA_LOG_LEVEL_INFO, "event_b1", "msg", NULL);
    TEST_ASSERT(id_b1 > 0);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT(logs[i]->id != id_b1);
        TEST_ASSERT_EQ_INT(logs[i]->execution_id, 1);
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

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
    TEST_ASSERT(id > 0);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 1);

    acta_db_execution_log_list_free(logs, count);
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
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
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

    int id1 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO,  "event1", "msg", NULL);
    int id2 = el_create_log(db, 1, ACTA_LOG_LEVEL_WARN,  "event2", "msg", NULL);
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);

    int count_before = el_count_logs(db, 1);
    TEST_ASSERT_EQ_INT(count_before, 2);

    int rc = el_raw_delete_execution(db, 1);
    TEST_ASSERT_EQ_INT(rc, 0);

    int count_after = el_count_logs(db, 1);
    TEST_ASSERT_EQ_INT(count_after, 0);

    test_db_teardown(db, path);
}

/* ---------- 10.17: limit — returns exactly `limit` rows ---------- */
static void test_el_list_limit(void) {
    const char *path = "test/acta_test_el_list_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 6; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* limit=2, offset=0 → exactly 2 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_log_list_free(logs, count);

    /* limit=100 (exceeds total) → all 6 rows */
    count = 0;
    err   = 0;
    logs  = acta_db_execution_log_list_by_execution(db, 1, 0, 100, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 6);
    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.18: offset — skips `offset` rows ---------- */
static void test_el_list_offset(void) {
    const char *path = "test/acta_test_el_list_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    /* Create 5 logs; capture the first id for later verification */
    int first_id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "first", "msg", NULL);
    TEST_ASSERT(first_id > 0);
    for (int i = 0; i < 4; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* offset=2, limit=-1 → rows 3,4,5 (3 rows), first one is NOT the original first */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 2, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT(logs[0]->id != first_id);

    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.19: offset + limit combined ---------- */
static void test_el_list_offset_limit(void) {
    const char *path = "test/acta_test_el_list_offlimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 10; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* offset=3, limit=4 → rows at indices 3,4,5,6 → 4 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 3, 4, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);

    acta_db_execution_log_list_free(logs, count);

    /* offset=8, limit=5 → only 2 rows remain (indices 8,9) */
    count = 0;
    err   = 0;
    logs  = acta_db_execution_log_list_by_execution(db, 1, 8, 5, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 2);

    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.20: offset beyond total → empty result ---------- */
static void test_el_list_offset_beyond(void) {
    const char *path = "test/acta_test_el_list_offbeyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 3; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* offset=10 (well beyond the 3 rows) → 0 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 10, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.21: limit=0 → no rows returned ---------- */
static void test_el_list_limit_zero(void) {
    const char *path = "test/acta_test_el_list_limit0.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 3; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* limit=0 → SQLite returns zero rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.22: get — happy path, all fields populated ---------- */
static void test_el_get_happy(void) {
    const char *path = "test/acta_test_el_get_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "deploy_started",
                           "Kicking off deploy", "{\"env\":\"prod\"}");
    TEST_ASSERT(id > 0);

    int err;
    execution_log_t *log = acta_db_execution_log_get(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(log);
    TEST_ASSERT_EQ_INT(log->id, id);
    TEST_ASSERT_EQ_INT(log->execution_id, 1);
    TEST_ASSERT(strcmp(log->level, ACTA_LOG_LEVEL_WARN) == 0);
    TEST_ASSERT(strcmp(log->event, "deploy_started") == 0);
    TEST_ASSERT(strcmp(log->message, "Kicking off deploy") == 0);
    TEST_ASSERT(strcmp(log->metadata, "{\"env\":\"prod\"}") == 0);
    TEST_ASSERT_NOT_NULL(log->created_at);

    acta_db_execution_log_free(log);
    test_db_teardown(db, path);
}


/* ---------- 10.23: get — not found ---------- */
static void test_el_get_not_found(void) {
    const char *path = "test/acta_test_el_get_notfound.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);

    int err;
    execution_log_t *log = acta_db_execution_log_get(db, 999999, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_NOT_FOUND);
    TEST_ASSERT_NULL(log);

    test_db_teardown(db, path);
}



/* ---------- 10.24: get — NULL db ---------- */
static void test_el_get_null_db(void) {
    int err;
    execution_log_t *log = acta_db_execution_log_get(NULL, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(log);
}


/* ---------- 10.25: get — NULL out_log (existence check) ---------- */
static void test_el_get_null_out_log(void) {
    const char *path = "test/acta_test_el_get_nullout.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
    TEST_ASSERT(id > 0);

    /* err is NULL — caller only wants the pointer (existence check) */
    execution_log_t *log = acta_db_execution_log_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(log);
    acta_db_execution_log_free(log);

    /* Non-existent id with NULL err: returns NULL (not-found). */
    execution_log_t *missing = acta_db_execution_log_get(db, 424242, NULL);
    TEST_ASSERT_NULL(missing);

    test_db_teardown(db, path);
}



/* ---------- 10.26: get — NULL optional fields round-trip ---------- */
static void test_el_get_null_optional_fields(void) {
    const char *path = "test/acta_test_el_get_nullopt.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_DEBUG, "bare_event", NULL, NULL);
    TEST_ASSERT(id > 0);

    int err;
    execution_log_t *log = acta_db_execution_log_get(db, id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(log);
    TEST_ASSERT_EQ_INT(log->id, id);
    TEST_ASSERT(strcmp(log->level, ACTA_LOG_LEVEL_DEBUG) == 0);
    TEST_ASSERT(strcmp(log->event, "bare_event") == 0);
    TEST_ASSERT_NULL(log->message);
    TEST_ASSERT_NULL(log->metadata);

    acta_db_execution_log_free(log);
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
    test_el_list_limit();
    test_el_list_offset();
    test_el_list_offset_limit();
    test_el_list_offset_beyond();
    test_el_list_limit_zero();
    test_el_get_happy();
    test_el_get_not_found();
    test_el_get_null_db();
    test_el_get_null_out_log();
    test_el_get_null_optional_fields();

}
