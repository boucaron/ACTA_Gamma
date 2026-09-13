/* test_context_deleted.c – unit tests for the context soft-delete
 * lifecycle (delete / restore / get_live / _with_deleted listers /
 * the contexts_soft_delete_only trigger).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "context.h"
#include "db.h"
#include "test_common.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Fixture helper
 * ═══════════════════════════════════════════════════════════════════ */

#define FIXTURE_ROWS 3

/*
 *   id | type  | hash | content
 *   1  | note  | h1   | alpha
 *   2  | note  | h2   | beta
 *   3  | email | h3   | gamma
 */

static void ctx_fixture(db_t *db)
{
    static const char *rows[][3] = {
        /* type,   hash,  content */
        {"note",  "h1",  "alpha"},
        {"note",  "h2",  "beta"},
        {"email", "h3",  "gamma"},
    };

    for (int i = 0; i < FIXTURE_ROWS; i++) {
        context_t c;
        memset(&c, 0, sizeof(c));
        c.type         = (char *)rows[i][0];
        c.content_hash = (char *)rows[i][1];
        c.content      = (char *)rows[i][2];
        c.metadata     = NULL;

        int id = 0;
        TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c, &id), ACTA_DB_OK);
        TEST_ASSERT_EQ_INT(id, i + 1);
    }
}

/* Fresh db + fixture, returns an open handle; caller must teardown. */
static db_t *fixture_open(const char *path)
{
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    if (db) ctx_fixture(db);
    return db;
}

/* ═══════════════════════════════════════════════════════════════════
 *  delete
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_delete_round_trip(void)
{
    const char *path = "test/acta_test_ctx_delete_rt.db";
    db_t *db = fixture_open(path);

    int rc = acta_db_context_delete(db, 1);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    /* get now sees the deleted row with deleted_at populated */
    int err = 0;
    context_t *c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL(c->deleted_at);
    acta_db_context_free(c);

    /* restore brings it back live */
    rc = acta_db_context_restore(db, 1);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NULL(c->deleted_at);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

static void test_ctx_delete_already_deleted(void)
{
    const char *path = "test/acta_test_ctx_delete_twice.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 1), ACTA_DB_OK);
    /* second delete: row exists but is already deleted → NOT_FOUND */
    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 1), ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

static void test_ctx_delete_missing(void)
{
    const char *path = "test/acta_test_ctx_delete_missing.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 9999), ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

static void test_ctx_delete_null_db(void)
{
    TEST_ASSERT_EQ_INT(acta_db_context_delete(NULL, 1), ACTA_DB_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════
 *  restore  (strict)
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_restore_live_is_error(void)
{
    const char *path = "test/acta_test_ctx_restore_live.db";
    db_t *db = fixture_open(path);

    /* row 2 is live → strict restore must fail (unlike skill_restore) */
    TEST_ASSERT_EQ_INT(acta_db_context_restore(db, 2), ACTA_DB_ERR_NOT_FOUND);

    /* and it is still live afterwards */
    int err = 0;
    context_t *c = acta_db_context_get(db, 2, &err);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NULL(c->deleted_at);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

static void test_ctx_restore_missing(void)
{
    const char *path = "test/acta_test_ctx_restore_missing.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_restore(db, 9999), ACTA_DB_ERR_NOT_FOUND);

    test_db_teardown(db, path);
}

static void test_ctx_restore_null_db(void)
{
    TEST_ASSERT_EQ_INT(acta_db_context_restore(NULL, 1), ACTA_DB_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════
 *  get / get_live on deleted rows
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_get_deleted_row(void)
{
    const char *path = "test/acta_test_ctx_get_deleted.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 2), ACTA_DB_OK);

    /* get: returns the row, deleted_at set */
    int err = 0;
    context_t *c = acta_db_context_get(db, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ_INT(c->id, 2);
    TEST_ASSERT_NOT_NULL(c->deleted_at);
    TEST_ASSERT(strcmp(c->content, "beta") == 0);
    acta_db_context_free(c);

    /* get_live: NULL for the deleted row */
    c = acta_db_context_get_live(db, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(c);

    /* get_live: live row still returns normally */
    c = acta_db_context_get_live(db, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NULL(c->deleted_at);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

static void test_ctx_get_live_missing(void)
{
    const char *path = "test/acta_test_ctx_get_live_missing.db";
    db_t *db = fixture_open(path);

    int err = 0;
    context_t *c = acta_db_context_get_live(db, 9999, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(c);

    test_db_teardown(db, path);
}

static void test_ctx_get_live_null_db(void)
{
    int err = 0;
    context_t *c = acta_db_context_get_live(NULL, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(c);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query / count: live-only default vs _with_deleted
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_default_live_only(void)
{
    const char *path = "test/acta_test_ctx_query_live.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 3), ACTA_DB_OK);

    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 2);
    TEST_ASSERT_NULL(rows[0]->deleted_at);
    acta_db_context_list_free(rows, n);

    TEST_ASSERT_EQ_INT(acta_db_context_count(db, NULL, &err), 1);

    test_db_teardown(db, path);
}

static void test_ctx_query_with_deleted(void)
{
    const char *path = "test/acta_test_ctx_query_wd.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 3), ACTA_DB_OK);

    /* unfiltered: live + deleted, ordered by id */
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query_with_deleted(db, NULL, 0, 0,
                                                           &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, FIXTURE_ROWS);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    TEST_ASSERT_NOT_NULL(rows[0]->deleted_at);
    TEST_ASSERT_NULL(rows[1]->deleted_at);
    TEST_ASSERT_NOT_NULL(rows[2]->deleted_at);
    acta_db_context_list_free(rows, n);

    /* filter by type: deleted note (1) + live note (2) */
    context_query_t q = { .type = "note", .hash = NULL };
    n = 0; err = 0;
    rows = acta_db_context_query_with_deleted(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 2);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    TEST_ASSERT_EQ_INT(rows[1]->id, 2);
    acta_db_context_list_free(rows, n);

    /* filter by hash: only the deleted row h3 */
    q.type = NULL; q.hash = "h3";
    n = 0; err = 0;
    rows = acta_db_context_query_with_deleted(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 3);
    TEST_ASSERT_NOT_NULL(rows[0]->deleted_at);
    acta_db_context_list_free(rows, n);

    TEST_ASSERT_EQ_INT(acta_db_context_count_with_deleted(db, NULL, &err),
                       FIXTURE_ROWS);
    context_query_t q2 = { .type = "note", .hash = NULL };
    TEST_ASSERT_EQ_INT(acta_db_context_count_with_deleted(db, &q2, &err), 2);

    test_db_teardown(db, path);
}

static void test_ctx_query_with_deleted_pagination(void)
{
    const char *path = "test/acta_test_ctx_query_wd_paging.db";
    db_t *db = fixture_open(path);

    /* no deletes: _with_deleted must equal the live view */
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query_with_deleted(db, NULL, 0, 2,
                                                           &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    acta_db_context_list_free(rows, n);

    n = 0; err = 0;
    rows = acta_db_context_query_with_deleted(db, NULL, 2, 1, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 3);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

static void test_ctx_query_with_deleted_invalid_db(void)
{
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query_with_deleted(NULL, NULL, 0, 0,
                                                           &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(rows);
    TEST_ASSERT_EQ_INT(n, 0);
}

static void test_ctx_count_with_deleted_invalid_db(void)
{
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_count_with_deleted(NULL, NULL, &err),
                       -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════
 *  contexts_soft_delete_only trigger
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_trigger_content_update_fails(void)
{
    const char *path = "test/acta_test_ctx_trigger_content.db";
    db_t *db = fixture_open(path);

    /* changing a content column must be aborted by the trigger */
    TEST_ASSERT_EQ_INT(acta_db_exec(db,
        "UPDATE contexts SET content = 'x' WHERE id = 1;"),
        ACTA_DB_ERR_SQL);

    /* row 1 is untouched */
    int err = 0;
    context_t *c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT(strcmp(c->content, "alpha") == 0);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

static void test_ctx_trigger_deleted_at_update_ok(void)
{
    const char *path = "test/acta_test_ctx_trigger_flag.db";
    db_t *db = fixture_open(path);

    /* the flag flip is the one permitted UPDATE */
    TEST_ASSERT_EQ_INT(acta_db_exec(db,
        "UPDATE contexts SET deleted_at = datetime('now') WHERE id = 1;"),
        ACTA_DB_OK);

    int err = 0;
    context_t *c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL(c->deleted_at);
    acta_db_context_free(c);

    /* and restore via raw SQL is permitted too */
    TEST_ASSERT_EQ_INT(acta_db_exec(db,
        "UPDATE contexts SET deleted_at = NULL WHERE id = 1;"),
        ACTA_DB_OK);

    c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NULL(c->deleted_at);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  free: deleted_at string is released
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_free_deleted_row(void)
{
    const char *path = "test/acta_test_ctx_free_deleted.db";
    db_t *db = fixture_open(path);

    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, 1), ACTA_DB_OK);

    int err = 0;
    context_t *c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_NOT_NULL(c);
    acta_db_context_free(c);

    int n = 0;
    context_t **rows = acta_db_context_query_with_deleted(db, NULL, 0, 0,
                                                           &n, &err);
    TEST_ASSERT_NOT_NULL(rows);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Entry point
 * ═══════════════════════════════════════════════════════════════════ */

int run_context_deleted_tests(void)
{
    test_ctx_delete_round_trip();
    test_ctx_delete_already_deleted();
    test_ctx_delete_missing();
    test_ctx_delete_null_db();

    test_ctx_restore_live_is_error();
    test_ctx_restore_missing();
    test_ctx_restore_null_db();

    test_ctx_get_deleted_row();
    test_ctx_get_live_missing();
    test_ctx_get_live_null_db();

    test_ctx_query_default_live_only();
    test_ctx_query_with_deleted();
    test_ctx_query_with_deleted_pagination();
    test_ctx_query_with_deleted_invalid_db();
    test_ctx_count_with_deleted_invalid_db();

    test_ctx_trigger_content_update_fails();
    test_ctx_trigger_deleted_at_update_ok();

    test_ctx_free_deleted_row();

    return test_failures;
}
