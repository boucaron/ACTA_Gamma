/* test_light_queries.c — light-projection listers
 *
 * Verifies that acta_db_context_query_light / _with_deleted_light and
 * acta_db_execution_query_light omit the blob columns (blob fields are
 * NULL in every returned row) while keeping ids, status/type,
 * timestamps, parent and identical pagination to the full listers.
 */

#include "test_common.h"
#include "test_execution_common.h"

/* ── tiny local helpers ──────────────────────────────────────────── */

/* Create a context row; returns the new id (> 0) or -1. */
static int ctx_create(db_t *db, const char *type, const char *content,
                      const char *hash)
{
    context_t c = {0};
    c.type         = (char *)type;
    c.content      = (char *)content;
    c.content_hash = (char *)hash;

    int id = 0;
    if (acta_db_context_create(db, &c, &id) != ACTA_DB_OK)
        return -1;
    return id;
}

/* ── context light ───────────────────────────────────────────────── */

/* context light – blob field NULL, non-blob fields correct,
 * pagination identical to the full lister */
static void test_context_light_fields_and_pagination(void) {
    const char *path = "test/acta_test_ctx_light.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = ctx_create(db, "doc", "full blob one", "h1");
    int id2 = ctx_create(db, "note", "full blob two", "h2");
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(id2 > 0);

    int count = 0, err = 0;
    context_t **items = acta_db_context_query_light(db, NULL, 0, 0,
                                                    &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_NULL(items[i]->content);       /* blob omitted */
        TEST_ASSERT_NOT_NULL(items[i]->type);
        TEST_ASSERT_NOT_NULL(items[i]->content_hash);
        TEST_ASSERT_NOT_NULL(items[i]->created_at);
        TEST_ASSERT_NULL(items[i]->deleted_at);    /* live rows */
    }
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    TEST_ASSERT_EQ_STR(items[0]->type, "doc");
    TEST_ASSERT_EQ_STR(items[0]->content_hash, "h1");
    TEST_ASSERT_EQ_INT(items[1]->id, id2);

    acta_db_context_list_free(items, count);

    /* pagination identical to the full lister */
    count = 0; err = 0;
    items = acta_db_context_query(db, NULL, 1, 1, &count, &err);
    int full_count = count;
    int first_id = items && items[0] ? items[0]->id : -1;
    acta_db_context_list_free(items, count);

    count = 0; err = 0;
    items = acta_db_context_query_light(db, NULL, 1, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, full_count);
    TEST_ASSERT_EQ_INT(items[0]->id, first_id);
    TEST_ASSERT_NULL(items[0]->content);
    acta_db_context_list_free(items, count);

    test_db_teardown(db, path);
}

/* context light – limit 0 clamps to ACTA_DB_MAX_PAGE like the full lister */
static void test_context_light_limit_zero(void) {
    const char *path = "test/acta_test_ctx_light_nolimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 5; i++) {
        char type[16], content[16], hash[16];
        snprintf(type, sizeof(type), "t%d", i);
        snprintf(content, sizeof(content), "blob %d", i);
        snprintf(hash, sizeof(hash), "h%d", i);
        TEST_ASSERT(ctx_create(db, type, content, hash) > 0);
    }

    int count = 0, err = 0;
    context_t **items = acta_db_context_query_light(db, NULL, 0, 0,
                                                    &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 5);
    acta_db_context_list_free(items, count);

    test_db_teardown(db, path);
}

/* context with_deleted light – deleted rows included with
 * deleted_at set, blob still NULL */
static void test_context_with_deleted_light(void) {
    const char *path = "test/acta_test_ctx_light_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id1 = ctx_create(db, "doc", "blob one", "h1");
    int id2 = ctx_create(db, "doc", "blob two", "h2");
    TEST_ASSERT(id1 > 0 && id2 > 0);
    TEST_ASSERT_EQ_INT(acta_db_context_delete(db, id1), ACTA_DB_OK);

    int count = 0, err = 0;
    context_t **items = acta_db_context_query_with_deleted_light(db, NULL,
                                                                 0, 0,
                                                                 &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_NULL(items[i]->content);
        TEST_ASSERT_EQ_INT(items[i]->id, (i == 0) ? id1 : id2);
    }
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);  /* id1 was deleted */
    TEST_ASSERT_NULL(items[1]->deleted_at);      /* id2 is live */
    acta_db_context_list_free(items, count);

    /* live-only light variant excludes the deleted row */
    count = 0; err = 0;
    items = acta_db_context_query_light(db, NULL, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id2);
    TEST_ASSERT_NULL(items[0]->content);
    acta_db_context_list_free(items, count);

    test_db_teardown(db, path);
}

/* ── execution light ─────────────────────────────────────────────── */

/* execution light – blob fields NULL, non-blob fields correct,
 * pagination identical to the full lister */
static void test_execution_light_fields_and_pagination(void) {
    const char *path = "test/acta_test_exec_light.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), 0);

    int id1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    int id2 = exec_create(db, ctx_id, sr_id, mr_id, id1);
    TEST_ASSERT(id1 > 0 && id2 > 0);

    /* populate blobs on row 1 so we can prove they are NOT returned */
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, id1), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_complete(db, id1, "result one"),
                       ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_set_raw_response(db, id1, "raw one"),
                       ACTA_DB_OK);

    int count = 0, err = 0;
    execution_t **items = acta_db_execution_query_light(db, NULL, 0, 0,
                                                        &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_NULL(items[i]->raw_response);
        TEST_ASSERT_NULL(items[i]->result);
        TEST_ASSERT_NULL(items[i]->error);
        TEST_ASSERT_NOT_NULL(items[i]->status);
        TEST_ASSERT_NOT_NULL(items[i]->created_at);
        TEST_ASSERT_EQ_INT(items[i]->context_id, ctx_id);
    }

    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    TEST_ASSERT_EQ_STR(items[0]->status, "completed");
    TEST_ASSERT_EQ_INT(items[0]->parent_execution_id, 0);
    TEST_ASSERT_EQ_INT(items[1]->id, id2);
    TEST_ASSERT_EQ_STR(items[1]->status, "pending");
    TEST_ASSERT_EQ_INT(items[1]->parent_execution_id, id1);

    acta_db_execution_list_free(items, count);

    /* full lister returns the same rows (ids) plus the blobs */
    count = 0; err = 0;
    items = acta_db_execution_query(db, NULL, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    TEST_ASSERT_EQ_STR(items[0]->result, "result one");
    TEST_ASSERT_EQ_STR(items[0]->raw_response, "raw one");
    acta_db_execution_list_free(items, count);

    test_db_teardown(db, path);
}

/* execution light – include_deleted, status filter, and count parity
 * with the full lister */
static void test_execution_light_filters_and_count(void) {
    const char *path = "test/acta_test_exec_light_filters.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ctx_id, sr_id, mr_id;
    TEST_ASSERT_EQ_INT(exec_setup(db, &ctx_id, &sr_id, &mr_id), 0);

    int id1 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    int id2 = exec_create(db, ctx_id, sr_id, mr_id, 0);
    TEST_ASSERT(id1 > 0 && id2 > 0);
    TEST_ASSERT_EQ_INT(acta_db_execution_start(db, id2), ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(acta_db_execution_delete(db, id1), ACTA_DB_OK);

    /* live-only light: only the running row */
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    int count = 0, err = 0;
    execution_t **items = acta_db_execution_query_light(db, &q, 0, 0,
                                                        &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id2);
    TEST_ASSERT_EQ_STR(items[0]->status, "running");
    acta_db_execution_list_free(items, count);

    /* include_deleted light: both rows, blob fields NULL */
    q.include_deleted = 1;
    count = 0; err = 0;
    items = acta_db_execution_query_light(db, &q, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    TEST_ASSERT_NOT_NULL(items[0]->deleted_at);
    TEST_ASSERT_EQ_INT(items[1]->id, id2);
    TEST_ASSERT_NULL(items[1]->deleted_at);
    acta_db_execution_list_free(items, count);

    /* status filter on light */
    q.include_deleted = 0;
    q.status = "running";
    count = 0; err = 0;
    items = acta_db_execution_query_light(db, &q, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->id, id2);
    acta_db_execution_list_free(items, count);

    /* count parity: light lister sees exactly what acta_db_execution_count
     * reports (the count query is projection-agnostic) */
    int total = acta_db_execution_count(db, &q, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 1);

    test_db_teardown(db, path);
}

/* ── runner ──────────────────────────────────────────────────────── */

int run_light_queries_tests(void) {
    fprintf(stderr, "\n=== light_queries tests ===\n");

    test_context_light_fields_and_pagination();
    test_context_light_limit_zero();
    test_context_with_deleted_light();

    test_execution_light_fields_and_pagination();
    test_execution_light_filters_and_count();

    return test_failures;
}
