/* context_test.c – unit tests for the context API. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "context.h"
#include "db.h"
#include "test_common.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Fixture helper
 * ═══════════════════════════════════════════════════════════════════ */

#define FIXTURE_ROWS  8

/*
 *   id | type  | hash | content
 *   1  | note  | h1   | alpha
 *   2  | note  | h2   | beta
 *   3  | note  | h3   | gamma
 *   4  | email | h1   | delta
 *   5  | email | h4   | epsilon
 *   6  | email | h5   | zeta
 *   7  | doc   | h1   | eta
 *   8  | doc   | h6   | theta
 */

static void ctx_fixture(db_t *db)
{
    static const char *rows[][3] = {
        /* type,   hash,  content */
        {"note",  "h1",  "alpha"},
        {"note",  "h2",  "beta"},
        {"note",  "h3",  "gamma"},
        {"email", "h1",  "delta"},
        {"email", "h4",  "epsilon"},
        {"email", "h5",  "zeta"},
        {"doc",   "h1",  "eta"},
        {"doc",   "h6",  "theta"},
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


/* ═══════════════════════════════════════════════════════════════════
 *  create
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_create_valid(void)
{
    const char *path = "test/acta_test_ctx_create.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c;
    memset(&c, 0, sizeof(c));
    c.type         = (char *)"note";
    c.content      = (char *)"hello world";
    c.content_hash = (char *)"abc123";

    int id = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c, &id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(id, 1);

    /* second insert, with metadata */
    c.metadata = (char *)"some meta";
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c, &id), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(id, 2);

    test_db_teardown(db, path);
}

static void test_ctx_create_invalid(void)
{
    const char *path = "test/acta_test_ctx_create_inv.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c;
    memset(&c, 0, sizeof(c));
    c.content      = (char *)"x";
    c.content_hash = (char *)"H";

    /* NULL type */
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c, &(int){0}),
                       ACTA_DB_ERR_INVALID);

    /* NULL content */
    c.type      = (char *)"x";
    c.content   = NULL;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c, &(int){0}),
                       ACTA_DB_ERR_INVALID);

    /* NULL content_hash */
    c.content      = (char *)"x";
    c.content_hash = NULL;
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, &c, &(int){0}),
                       ACTA_DB_ERR_INVALID);

    /* NULL struct */
    TEST_ASSERT_EQ_INT(acta_db_context_create(db, NULL, &(int){0}),
                       ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  get
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_get_valid(void)
{
    const char *path = "test/acta_test_ctx_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_t *c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ_INT(c->id, 1);
    TEST_ASSERT(strcmp(c->type, "note") == 0);
    TEST_ASSERT(strcmp(c->content, "alpha") == 0);
    TEST_ASSERT(strcmp(c->content_hash, "h1") == 0);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

static void test_ctx_get_not_found(void)
{
    const char *path = "test/acta_test_ctx_get_nf.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_t *c = acta_db_context_get(db, 9999, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(c);

    test_db_teardown(db, path);
}

static void test_ctx_get_null_db(void)
{
    int err = 0;
    context_t *c = acta_db_context_get(NULL, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(c);
}

static void test_ctx_get_null_err(void)
{
    const char *path = "test/acta_test_ctx_get_nerr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    context_t *c = acta_db_context_get(db, 1, NULL);
    TEST_ASSERT_NOT_NULL(c);
    acta_db_context_free(c);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – no filter, no paging
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_all(void)
{
    const char *path = "test/acta_test_ctx_query_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(rows);
    TEST_ASSERT_EQ_INT(n, FIXTURE_ROWS);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    TEST_ASSERT_EQ_INT(rows[7]->id, 8);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – by type
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_by_type(void)
{
    const char *path = "test/acta_test_ctx_query_type.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    context_query_t q = { .type = "note", .hash = NULL };
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 3);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    TEST_ASSERT_EQ_INT(rows[1]->id, 2);
    TEST_ASSERT_EQ_INT(rows[2]->id, 3);
    acta_db_context_list_free(rows, n);

    /* email */
    q.type = "email";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 3);
    acta_db_context_list_free(rows, n);

    /* doc */
    q.type = "doc";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_context_list_free(rows, n);

    /* unknown → 0 */
    q.type = "nonexistent";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);
    if (rows) acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – by hash
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_by_hash(void)
{
    const char *path = "test/acta_test_ctx_query_hash.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    context_query_t q = { .type = NULL, .hash = "h1" };
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 3);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    TEST_ASSERT_EQ_INT(rows[1]->id, 4);
    TEST_ASSERT_EQ_INT(rows[2]->id, 7);
    acta_db_context_list_free(rows, n);

    /* single match */
    q.hash = "h6";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 8);
    acta_db_context_list_free(rows, n);

    /* no match */
    q.hash = "h_nope";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 0);
    if (rows) acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – combined type + hash
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_combined(void)
{
    const char *path = "test/acta_test_ctx_query_comb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    context_query_t q = { .type = "note", .hash = "h1" };
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    acta_db_context_list_free(rows, n);

    q.type = "email";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 4);
    acta_db_context_list_free(rows, n);

    q.type = "doc";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 7);
    acta_db_context_list_free(rows, n);

    /* no intersection */
    q.type = "note"; q.hash = "h4";
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(n, 0);
    if (rows) acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – offset / limit
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_offset_limit(void)
{
    const char *path = "test/acta_test_ctx_query_off.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    context_query_t q = { .type = "note", .hash = NULL };  /* ids 1,2,3 */
    int n = 0, err = 0;
    context_t **rows;

    /* page 1: offset=0 limit=2 → ids 1,2 */
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 0, 2, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 2);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    TEST_ASSERT_EQ_INT(rows[1]->id, 2);
    acta_db_context_list_free(rows, n);

    /* page 2: offset=2 limit=1 → id 3 */
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 2, 1, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 3);
    acta_db_context_list_free(rows, n);

    /* past the end */
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 10, 5, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);
    if (rows) acta_db_context_list_free(rows, n);

    /* limit > remaining */
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q, 1, 100, &n, &err);
    TEST_ASSERT_EQ_INT(n, 2);
    acta_db_context_list_free(rows, n);

    /* unfiltered, limit 1 */
    context_query_t q0 = { .type = NULL, .hash = NULL };
    n = 0; err = 0;
    rows = acta_db_context_query(db, &q0, 0, 1, &n, &err);
    TEST_ASSERT_EQ_INT(n, 1);
    TEST_ASSERT_EQ_INT(rows[0]->id, 1);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – negative limit means "no limit"
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_neg_limit_ok(void)
{
    const char *path = "test/acta_test_ctx_query_nlim.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    /* limit <= 0 → no cap, returns all */
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, NULL, 0, -1, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, FIXTURE_ROWS);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – validation
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_invalid_db(void)
{
    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(NULL, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(rows);
    TEST_ASSERT_EQ_INT(n, 0);
}

static void test_ctx_query_neg_offset(void)
{
    const char *path = "test/acta_test_ctx_query_noff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, NULL, -1, 10, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_NULL(rows);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query – NULL out params
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_null_out_params(void)
{
    const char *path = "test/acta_test_ctx_query_nout.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    /* both out_count and err NULL */
    context_t **rows = acta_db_context_query(db, NULL, 0, 0, NULL, NULL);
    TEST_ASSERT_NOT_NULL(rows);
    /* count via a second call to know how to free */
    int n = 0, err = 0;
    acta_db_context_query(db, NULL, 0, 0, &n, &err);
    acta_db_context_list_free(rows, n);

    /* only err NULL */
    n = 0;
    rows = acta_db_context_query(db, NULL, 0, 0, &n, NULL);
    TEST_ASSERT_NOT_NULL(rows);
    TEST_ASSERT_EQ_INT(n, FIXTURE_ROWS);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  query on empty table
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_query_empty(void)
{
    const char *path = "test/acta_test_ctx_query_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int n = 0, err = 0;
    context_t **rows = acta_db_context_query(db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(n, 0);
    if (rows) acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  count
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_count_all(void)
{
    const char *path = "test/acta_test_ctx_count_all.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, NULL, &err), FIXTURE_ROWS);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_ctx_count_by_type(void)
{
    const char *path = "test/acta_test_ctx_count_type.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_query_t q;
    memset(&q, 0, sizeof(q));

    q.type = "note";
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 3);
    q.type = "email";
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 3);
    q.type = "doc";
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 2);

    test_db_teardown(db, path);
}

static void test_ctx_count_by_hash(void)
{
    const char *path = "test/acta_test_ctx_count_hash.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_query_t q = { .type = NULL, .hash = "h1" };
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 3);

    q.hash = "h6";
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 1);

    test_db_teardown(db, path);
}

static void test_ctx_count_combined(void)
{
    const char *path = "test/acta_test_ctx_count_comb.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_query_t q = { .type = "note", .hash = "h1" };
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 1);

    q.type = "email";
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 1);

    q.type = "doc";
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 1);

    test_db_teardown(db, path);
}

static void test_ctx_count_no_match(void)
{
    const char *path = "test/acta_test_ctx_count_nm.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_query_t q = { .type = "nonexistent", .hash = NULL };
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 0);

    test_db_teardown(db, path);
}

static void test_ctx_count_null_db(void)
{
    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_count(NULL, NULL, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_ctx_count_null_err(void)
{
    const char *path = "test/acta_test_ctx_count_nerr.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    TEST_ASSERT_EQ_INT(acta_db_context_count(db, NULL, NULL), FIXTURE_ROWS);

    test_db_teardown(db, path);
}

static void test_ctx_count_empty(void)
{
    const char *path = "test/acta_test_ctx_count_empty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, NULL, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    context_query_t q = { .type = "note", .hash = NULL };
    TEST_ASSERT_EQ_INT(acta_db_context_count(db, &q, &err), 0);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  free / list_free
 * ═══════════════════════════════════════════════════════════════════ */

static void test_ctx_free_null(void)
{
    acta_db_context_free(NULL);
    acta_db_context_list_free(NULL, 0);
    acta_db_context_list_free(NULL, 5);
}

static void test_ctx_free_valid(void)
{
    const char *path = "test/acta_test_ctx_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    ctx_fixture(db);

    int err = 0;
    context_t *c = acta_db_context_get(db, 1, &err);
    TEST_ASSERT_NOT_NULL(c);
    acta_db_context_free(c);

    int n = 0;
    context_t **rows = acta_db_context_query(db, NULL, 0, 0, &n, &err);
    TEST_ASSERT_NOT_NULL(rows);
    acta_db_context_list_free(rows, n);

    test_db_teardown(db, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Entry point
 * ═══════════════════════════════════════════════════════════════════ */

void run_context_tests(void)
{
    /* create */
    test_ctx_create_valid();
    test_ctx_create_invalid();

    /* get */
    test_ctx_get_valid();
    test_ctx_get_not_found();
    test_ctx_get_null_db();
    test_ctx_get_null_err();

    /* query */
    test_ctx_query_all();
    test_ctx_query_by_type();
    test_ctx_query_by_hash();
    test_ctx_query_combined();
    test_ctx_query_offset_limit();
    test_ctx_query_neg_limit_ok();
    test_ctx_query_invalid_db();
    test_ctx_query_neg_offset();
    test_ctx_query_null_out_params();
    test_ctx_query_empty();

    /* count */
    test_ctx_count_all();
    test_ctx_count_by_type();
    test_ctx_count_by_hash();
    test_ctx_count_combined();
    test_ctx_count_no_match();
    test_ctx_count_null_db();
    test_ctx_count_null_err();
    test_ctx_count_empty();

    /* free */
    test_ctx_free_null();
    test_ctx_free_valid();
}
