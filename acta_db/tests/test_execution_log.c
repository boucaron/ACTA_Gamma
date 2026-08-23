/* test_execution_log.c — Tests for acta_db_execution_log.h */

#include "test_common.h"
#include "execution_log.h"
#include <sqlite3.h>

/* ---------- helpers ---------- */

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

/* Uses the dedicated count API (not a lister round-trip). */
static int el_count(db_t *db, int execution_id, const char *level) {
    return acta_db_execution_log_count(db, execution_id, level, NULL);
}

static int el_raw_delete_execution(db_t *db, int execution_id) {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM executions WHERE id = %d", execution_id);
    return acta_db_exec(db, sql);
}

/* ================================================================ */
/*  10.1 – 10.7: create                                             */
/* ================================================================ */

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
    log.level = (char *)"verbose";
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

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_DEBUG, "e", "m", NULL);
    TEST_ASSERT(id > 0);
    id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO,  "e", "m", NULL);
    TEST_ASSERT(id > 0);
    id = el_create_log(db, 1, ACTA_LOG_LEVEL_WARN,  "e", "m", NULL);
    TEST_ASSERT(id > 0);
    id = el_create_log(db, 1, ACTA_LOG_LEVEL_ERROR, "e", "m", NULL);
    TEST_ASSERT(id > 0);

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

/* ================================================================ */
/*  10.8 – 10.12: list_by_execution                                  */
/* ================================================================ */

/* ---------- 10.8: list — multiple logs ---------- */
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

    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 5);

    test_db_teardown(db, path);
}

/* ---------- 10.9: list — ordering ---------- */
static void test_el_list_ordering(void) {
    const char *path = "test/acta_test_el_list_order.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id1 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "first",  "msg1", NULL);
    int id2 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "second", "msg2", NULL);
    int id3 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "third",  "msg3", NULL);
    TEST_ASSERT(id1 > 0 && id2 > 0 && id3 > 0);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);

    TEST_ASSERT(logs[0]->id <= logs[1]->id);
    TEST_ASSERT(logs[1]->id <= logs[2]->id);

    acta_db_execution_log_list_free(logs, count);
    test_db_teardown(db, path);
}

/* ---------- 10.10: list — no logs ---------- */
static void test_el_list_no_logs(void) {
    const char *path = "test/acta_test_el_list_nologs.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 42, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.11: list — non-existent execution ---------- */
static void test_el_list_nonexistent_execution(void) {
    const char *path = "test/acta_test_el_list_nonexist.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 999999, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.12: list — excludes other executions ---------- */
static void test_el_list_excludes_others(void) {
    const char *path = "test/acta_test_el_list_exclude.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    el_insert_execution(db, 2);

    int id_a1 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event_a1", "msg", NULL);
    int id_a2 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event_a2", "msg", NULL);
    int id_b1 = el_create_log(db, 2, ACTA_LOG_LEVEL_INFO, "event_b1", "msg", NULL);
    TEST_ASSERT(id_a1 > 0 && id_a2 > 0 && id_b1 > 0);

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -1, &count, &err);
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

/* ================================================================ */
/*  10.13 – 10.15: free                                              */
/* ================================================================ */

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
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 1);

    acta_db_execution_log_list_free(logs, count);
    TEST_ASSERT(1);

    test_db_teardown(db, path);
}

/* ---------- 10.14: free — NULL ---------- */
static void test_el_free_null(void) {
    acta_db_execution_log_free(NULL);
    acta_db_execution_log_list_free(NULL, 0);
    TEST_ASSERT(1);
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
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);

    acta_db_execution_log_list_free(logs, count);
    TEST_ASSERT(1);

    test_db_teardown(db, path);
}

/* ================================================================ */
/*  10.16: CASCADE delete                                            */
/* ================================================================ */

/* ---------- 10.16: CASCADE delete ---------- */
static void test_el_cascade_delete(void) {
    const char *path = "test/acta_test_el_cascade.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int id1 = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event1", "msg", NULL);
    int id2 = el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "event2", "msg", NULL);
    TEST_ASSERT(id1 > 0 && id2 > 0);

    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 2);

    int rc = el_raw_delete_execution(db, 1);
    TEST_ASSERT_EQ_INT(rc, 0);

    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 0);

    test_db_teardown(db, path);
}

/* ================================================================ */
/*  10.17 – 10.22: pagination                                        */
/* ================================================================ */

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

    /* limit=2 → exactly 2 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_log_list_free(logs, count);

    /* limit=100 (exceeds total) → all 6 rows */
    count = 0;
    err   = 0;
    logs  = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, 100, &count, &err);
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

    int first_id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "first", "msg", NULL);
    TEST_ASSERT(first_id > 0);
    for (int i = 0; i < 4; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* offset=2 → rows 3,4,5 */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 2, -1, &count, &err);
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

    /* offset=3, limit=4 → rows 4,5,6,7 → 4 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 3, 4, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);
    acta_db_execution_log_list_free(logs, count);

    /* offset=8, limit=5 → only 2 rows remain */
    count = 0;
    err   = 0;
    logs  = acta_db_execution_log_list_by_execution(db, 1, NULL, 8, 5, &count, &err);
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

    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 10, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.21: limit <= 0 → no limit (return all) ---------- */
static void test_el_list_limit_zero_no_limit(void) {
    const char *path = "test/acta_test_el_list_limit0.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 3; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* limit=0 → "no limit" per contract → all 3 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_execution_log_list_free(logs, count);

    /* limit=-1 → also "no limit" → all 3 rows */
    count = 0;
    err   = 0;
    logs  = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_execution_log_list_free(logs, count);

    /* limit=-5 → also "no limit" → all 3 rows */
    count = 0;
    err   = 0;
    logs  = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -5, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.22: negative offset clamps to 0 ---------- */
static void test_el_list_negative_offset(void) {
    const char *path = "test/acta_test_el_list_negoffset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 4; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* offset=-100 → clamped to 0 → all 4 rows */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, -100, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);

    acta_db_execution_log_list_free(logs, count);
    test_db_teardown(db, path);
}

/* ================================================================ */
/*  10.23 – 10.27: get                                               */
/* ================================================================ */

/* ---------- 10.23: get — happy path, all fields populated ---------- */
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

/* ---------- 10.24: get — not found (NULL + ACTA_DB_OK) ---------- */
static void test_el_get_not_found(void) {
    const char *path = "test/acta_test_el_get_notfound.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);

    int err;
    execution_log_t *log = acta_db_execution_log_get(db, 999999, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(log);

    test_db_teardown(db, path);
}

/* ---------- 10.25: get — NULL db ---------- */
static void test_el_get_null_db(void) {
    int err = 0;
    execution_log_t *log = acta_db_execution_log_get(NULL, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(log);
}

/* ---------- 10.26: get — NULL err (existence check) ---------- */
static void test_el_get_null_err(void) {
    const char *path = "test/acta_test_el_get_nullerr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
    TEST_ASSERT(id > 0);

    /* err is NULL — caller only wants the pointer */
    execution_log_t *log = acta_db_execution_log_get(db, id, NULL);
    TEST_ASSERT_NOT_NULL(log);
    acta_db_execution_log_free(log);

    /* Non-existent id, NULL err: returns NULL without crashing */
    execution_log_t *missing = acta_db_execution_log_get(db, 424242, NULL);
    TEST_ASSERT_NULL(missing);

    test_db_teardown(db, path);
}

/* ---------- 10.27: get — NULL optional fields round-trip ---------- */
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

/* ================================================================ */
/*  10.28 – 10.34: count                                             */
/* ================================================================ */

/* ---------- 10.28: count — happy (matches lister total) ---------- */
static void test_el_count_happy(void) {
    const char *path = "test/acta_test_el_count_happy.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    el_insert_execution(db, 2);

    for (int i = 0; i < 7; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }
    for (int i = 0; i < 3; i++) {
        int id = el_create_log(db, 2, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, NULL, &err), 7);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 2, NULL, &err), 3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 10.29: count — no rows → 0 ---------- */
static void test_el_count_empty(void) {
    const char *path = "test/acta_test_el_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, NULL, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 10.30: count — non-existent execution → 0 ---------- */
static void test_el_count_nonexistent(void) {
    const char *path = "test/acta_test_el_count_nonexist.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 999999, NULL, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* ---------- 10.31: count — NULL db ---------- */
static void test_el_count_null_db(void) {
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(NULL, 1, NULL, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- 10.32: count — NULL err (caller ignores code) ---------- */
static void test_el_count_null_err(void) {
    const char *path = "test/acta_test_el_count_nullerr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
    TEST_ASSERT(id > 0);

    /* err is NULL — just want the number */
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, NULL, NULL), 1);

    test_db_teardown(db, path);
}

/* ---------- 10.33: count — consistency with paginated lister ---------- */
static void test_el_count_matches_pages(void) {
    const char *path = "test/acta_test_el_count_pages.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int total = 12;
    for (int i = 0; i < total; i++) {
        int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "event", "msg", NULL);
        TEST_ASSERT(id > 0);
    }

    /* count says 12 */
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, NULL, NULL), total);

    /* Walk through pages of 5: 5 + 5 + 2 = 12 */
    int page_sum = 0;
    for (int offset = 0; offset < total; offset += 5) {
        int count = 0, err = 0;
        execution_log_t **logs =
            acta_db_execution_log_list_by_execution(db, 1, NULL, offset, 5, &count, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        page_sum += count;
        acta_db_execution_log_list_free(logs, count);
    }
    TEST_ASSERT_EQ_INT(page_sum, total);

    test_db_teardown(db, path);
}

/* ---------- 10.34: count — reflects inserts and cascade deletes ---------- */
static void test_el_count_dynamic(void) {
    const char *path = "test/acta_test_el_count_dynamic.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 0);

    int id = el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "a", "m", NULL);
    TEST_ASSERT(id > 0);
    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 1);

    id = el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "b", "m", NULL);
    TEST_ASSERT(id > 0);
    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 2);

    /* Cascade-delete the execution → logs vanish */
    el_raw_delete_execution(db, 1);
    TEST_ASSERT_EQ_INT(el_count(db, 1, NULL), 0);

    test_db_teardown(db, path);
}

/* ================================================================ */
/*  10.35 – 10.42: level filter                                      */
/* ================================================================ */

/* ---------- 10.35: list — filter by level returns only matching rows ---------- */
static void test_el_list_filter_by_level(void) {
    const char *path = "test/acta_test_el_list_filter_level.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    /* Insert mixed levels: 3 info, 2 error, 1 warn */
    for (int i = 0; i < 3; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_INFO,  "info_evt",  "m", NULL) > 0);
    for (int i = 0; i < 2; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_ERROR, "error_evt", "m", NULL) > 0);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "warn_evt", "m", NULL) > 0);

    /* Filter: only errors → 2 rows */
    int count = 0, err = 0;
    execution_log_t **logs =
        acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_ERROR, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 2);
    for (int i = 0; i < count; i++)
        TEST_ASSERT(strcmp(logs[i]->level, ACTA_LOG_LEVEL_ERROR) == 0);
    acta_db_execution_log_list_free(logs, count);

    /* Filter: only infos → 3 rows */
    count = 0;
    logs = acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_INFO, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++)
        TEST_ASSERT(strcmp(logs[i]->level, ACTA_LOG_LEVEL_INFO) == 0);
    acta_db_execution_log_list_free(logs, count);

    /* Filter: only warns → 1 row */
    count = 0;
    logs = acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_WARN, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT(strcmp(logs[0]->level, ACTA_LOG_LEVEL_WARN) == 0);
    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.36: list — NULL level returns all (backward compat) ---------- */
static void test_el_list_null_level_all(void) {
    const char *path = "test/acta_test_el_list_nulllevel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_DEBUG, "e", "m", NULL) > 0);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_INFO,  "e", "m", NULL) > 0);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_WARN,  "e", "m", NULL) > 0);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_ERROR, "e", "m", NULL) > 0);

    /* NULL → all 4 */
    int count = 0, err = 0;
    execution_log_t **logs = acta_db_execution_log_list_by_execution(db, 1, NULL, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);
    acta_db_execution_log_list_free(logs, count);

    /* "" (empty) → also all 4 */
    count = 0;
    logs = acta_db_execution_log_list_by_execution(db, 1, "", 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 4);
    acta_db_execution_log_list_free(logs, count);

    test_db_teardown(db, path);
}

/* ---------- 10.37: list — invalid level string → ACTA_DB_ERR_INVALID ---------- */
static void test_el_list_invalid_level(void) {
    const char *path = "test/acta_test_el_list_badlevel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "e", "m", NULL) > 0);

    int count = 0, err = 0;
    execution_log_t **logs =
        acta_db_execution_log_list_by_execution(db, 1, "verbose", 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(logs);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

/* ---------- 10.38: list — level filter + pagination ---------- */
static void test_el_list_filter_pagination(void) {
    const char *path = "test/acta_test_el_list_filter_paging.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    /* 4 errors, 3 infos */
    for (int i = 0; i < 4; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_ERROR, "err", "m", NULL) > 0);
    for (int i = 0; i < 3; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "info", "m", NULL) > 0);

    /* Filter errors, page size 2: first page → 2 */
    int count = 0, err = 0;
    execution_log_t **logs =
        acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_ERROR, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_log_list_free(logs, count);

    /* Second page → 2 */
    count = 0;
    logs = acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_ERROR, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    acta_db_execution_log_list_free(logs, count);

    /* offset=3 → 1 remaining */
    count = 0;
    logs = acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_ERROR, 3, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    acta_db_execution_log_list_free(logs, count);

    /* offset=4 → 0 (past the end of filtered set) */
    count = 0;
    logs = acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_ERROR, 4, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.39: list — level filter + no match → empty ---------- */
static void test_el_list_filter_no_match(void) {
    const char *path = "test/acta_test_el_list_filter_nomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    /* Only debug rows exist */
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_DEBUG, "e", "m", NULL) > 0);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_DEBUG, "e", "m", NULL) > 0);

    /* Ask for errors → none */
    int count = 0, err = 0;
    execution_log_t **logs =
        acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_ERROR, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(logs);

    test_db_teardown(db, path);
}

/* ---------- 10.40: count — with level filter ---------- */
static void test_el_count_filter_by_level(void) {
    const char *path = "test/acta_test_el_count_filter.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    for (int i = 0; i < 3; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_INFO,  "e", "m", NULL) > 0);
    for (int i = 0; i < 2; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_ERROR, "e", "m", NULL) > 0);
    TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "e", "m", NULL) > 0);

    int err = 0;

    /* NULL → all 6 */
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, NULL, &err), 6);

    /* info → 3 */
    err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, ACTA_LOG_LEVEL_INFO, &err), 3);

    /* error → 2 */
    err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, ACTA_LOG_LEVEL_ERROR, &err), 2);

    /* warn → 1 */
    err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, ACTA_LOG_LEVEL_WARN, &err), 1);

    /* debug → 0 (none exist) */
    err = 0;
    TEST_ASSERT_EQ_INT(acta_db_execution_log_count(db, 1, ACTA_LOG_LEVEL_DEBUG, &err), 0);

    test_db_teardown(db, path);
}

/* ---------- 10.41: count — invalid level → ACTA_DB_ERR_INVALID ---------- */
static void test_el_count_invalid_level(void) {
    const char *path = "test/acta_test_el_count_badlevel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    int err = 0;
    int rc = acta_db_execution_log_count(db, 1, "critical", &err);
    TEST_ASSERT_EQ_INT(rc, -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ---------- 10.42: count — consistency with filtered lister ---------- */
static void test_el_count_matches_filtered_pages(void) {
    const char *path = "test/acta_test_el_count_filtered_pages.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    el_insert_execution(db, 1);

    /* 5 warnings, 4 infos */
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_WARN, "e", "m", NULL) > 0);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT(el_create_log(db, 1, ACTA_LOG_LEVEL_INFO, "e", "m", NULL) > 0);

    /* count(warn) should equal sum of paginated warn pages */
    int total = acta_db_execution_log_count(db, 1, ACTA_LOG_LEVEL_WARN, NULL);
    TEST_ASSERT_EQ_INT(total, 5);

    int page_sum = 0;
    for (int offset = 0; offset < total; offset += 2) {
        int count = 0, err = 0;
        execution_log_t **logs =
            acta_db_execution_log_list_by_execution(db, 1, ACTA_LOG_LEVEL_WARN, offset, 2, &count, &err);
        TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
        page_sum += count;
        acta_db_execution_log_list_free(logs, count);
    }
    TEST_ASSERT_EQ_INT(page_sum, total);

    test_db_teardown(db, path);
}

/* ================================================================ */
/*  runner                                                            */
/* ================================================================ */

void run_execution_log_tests(void) {
    fprintf(stderr, "\n=== execution_log tests ===\n");

    /* create */
    test_el_create_happy();
    test_el_create_invalid_execution();
    test_el_create_invalid_level();
    test_el_create_all_valid_levels();
    test_el_create_null_event();
    test_el_create_null_message();
    test_el_create_null_metadata();

    /* list */
    test_el_list_multiple();
    test_el_list_ordering();
    test_el_list_no_logs();
    test_el_list_nonexistent_execution();
    test_el_list_excludes_others();

    /* free */
    test_el_free_valid();
    test_el_free_null();
    test_el_list_free_valid();

    /* cascade */
    test_el_cascade_delete();

    /* pagination */
    test_el_list_limit();
    test_el_list_offset();
    test_el_list_offset_limit();
    test_el_list_offset_beyond();
    test_el_list_limit_zero_no_limit();
    test_el_list_negative_offset();

    /* get */
    test_el_get_happy();
    test_el_get_not_found();
    test_el_get_null_db();
    test_el_get_null_err();
    test_el_get_null_optional_fields();

    /* count */
    test_el_count_happy();
    test_el_count_empty();
    test_el_count_nonexistent();
    test_el_count_null_db();
    test_el_count_null_err();
    test_el_count_matches_pages();
    test_el_count_dynamic();

    /* level filter */
    test_el_list_filter_by_level();
    test_el_list_null_level_all();
    test_el_list_invalid_level();
    test_el_list_filter_pagination();
    test_el_list_filter_no_match();
    test_el_count_filter_by_level();
    test_el_count_invalid_level();
    test_el_count_matches_filtered_pages();
}
