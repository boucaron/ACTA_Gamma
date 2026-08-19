#include "test_common.h"

/* ---------- 2.1: context_create — happy path ---------- */
static void test_context_create_happy(void) {
    const char *path = "test/acta_test_ctx_create.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = {
        .type = "document",
        .content = "Hello world",
        .content_hash = "abc123",
        .metadata = NULL,
    };
    int id = 0;
    int rc = acta_db_context_create(db, &c, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(id > 0);
    test_db_teardown(db, path);
}

/* ---------- 2.2: context_create — NULL type ---------- */
static void test_context_create_null_type(void) {
    const char *path = "test/acta_test_ctx_nulltype.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = {
        .type = NULL,
        .content = "test",
        .content_hash = "hash1",
    };
    int id = 0;
    int rc = acta_db_context_create(db, &c, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 2.3: context_create — NULL content ---------- */
static void test_context_create_null_content(void) {
    const char *path = "test/acta_test_ctx_nullcontent.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = {
        .type = "doc",
        .content = NULL,
        .content_hash = "hash1",
    };
    int id = 0;
    int rc = acta_db_context_create(db, &c, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 2.4: context_create — NULL content_hash ---------- */
static void test_context_create_null_hash(void) {
    const char *path = "test/acta_test_ctx_nullhash.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = {
        .type = "doc",
        .content = "test",
        .content_hash = NULL,
    };
    int id = 0;
    int rc = acta_db_context_create(db, &c, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 2.5: context_create — NULL metadata ---------- */
static void test_context_create_null_metadata(void) {
    const char *path = "test/acta_test_ctx_nullmeta.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = {
        .type = "doc",
        .content = "test",
        .content_hash = "hash1",
        .metadata = NULL,
    };
    int id = 0;
    int rc = acta_db_context_create(db, &c, &id);
    TEST_ASSERT_EQ_INT(rc, 0);
    TEST_ASSERT(id > 0);
    test_db_teardown(db, path);
}

/* ---------- 2.6: context_create — NULL struct ---------- */
static void test_context_create_null_struct(void) {
    const char *path = "test/acta_test_ctx_nullstruct.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int id = 0;
    int rc = acta_db_context_create(db, NULL, &id);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

/* ---------- 2.7: context_get — existing id ---------- */
static void test_context_get_existing(void) {
    const char *path = "test/acta_test_ctx_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = { .type = "doc", .content = "data", .content_hash = "h1" };
    int id = 0;
    acta_db_context_create(db, &c, &id);

    context_t *got = acta_db_context_get(db, id);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(got->id, id);
    TEST_ASSERT_EQ_STR(got->type, "doc");
    TEST_ASSERT_EQ_STR(got->content, "data");
    TEST_ASSERT_EQ_STR(got->content_hash, "h1");
    acta_db_context_free(got);
    test_db_teardown(db, path);
}

/* ---------- 2.8: context_get — non-existent id ---------- */
static void test_context_get_nonexistent(void) {
    const char *path = "test/acta_test_ctx_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t *got = acta_db_context_get(db, 999999);
    TEST_ASSERT_NULL(got);
    test_db_teardown(db, path);
}

/* ---------- 2.9: context_get — NULL db ---------- */
static void test_context_get_null_db(void) {
    context_t *got = acta_db_context_get(NULL, 1);
    TEST_ASSERT_NULL(got);
}

/* ---------- 2.10: context_list_by_hash — match ---------- */
static void test_context_list_by_hash_match(void) {
    const char *path = "test/acta_test_ctx_hashmatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c1 = { .type = "a", .content = "x", .content_hash = "same_hash" };
    context_t c2 = { .type = "b", .content = "y", .content_hash = "same_hash" };
    int id1, id2;
    acta_db_context_create(db, &c1, &id1);
    acta_db_context_create(db, &c2, &id2);

    int count = 0;
    context_t *items = acta_db_context_list_by_hash(db, "same_hash", &count);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.11: context_list_by_hash — no match ---------- */
static void test_context_list_by_hash_nomatch(void) {
    const char *path = "test/acta_test_ctx_hashnomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    context_t *items = acta_db_context_list_by_hash(db, "nonexistent_hash", &count);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.12: context_list_by_hash — NULL hash ---------- */
static void test_context_list_by_hash_null(void) {
    const char *path = "test/acta_test_ctx_hashnull.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = -1;
    context_t *items = acta_db_context_list_by_hash(db, NULL, &count);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.13: context_free — valid ---------- */
static void test_context_free_valid(void) {
    const char *path = "test/acta_test_ctx_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = { .type = "t", .content = "c", .content_hash = "h" };
    int id;
    acta_db_context_create(db, &c, &id);
    context_t *got = acta_db_context_get(db, id);
    acta_db_context_free(got);
    test_db_teardown(db, path);
}

/* ---------- 2.14: context_free — NULL ---------- */
static void test_context_free_null(void) {
    acta_db_context_free(NULL);
    TEST_ASSERT(1);
}

/* ---------- 2.15: context_list_free — valid ---------- */
static void test_context_list_free_valid(void) {
    const char *path = "test/acta_test_ctx_listfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    for (int i = 0; i < 5; i++) {
        context_t c = { .type = "t", .content = "c", .content_hash = "bulk" };
        acta_db_context_create(db, &c, &(int){0});
    }
    int count = 0;
    context_t *items = acta_db_context_list_by_hash(db, "bulk", &count);
    TEST_ASSERT_EQ_INT(count, 5);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.16: context_list_free — NULL/0 ---------- */
static void test_context_list_free_null(void) {
    acta_db_context_list_free(NULL, 0);
    TEST_ASSERT(1);
}

/* ---------- 2.17: Immutability — UPDATE rejected ---------- */
static void test_context_immutable(void) {
    const char *path = "test/acta_test_ctx_immutable.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = { .type = "t", .content = "c", .content_hash = "h" };
    int id;
    acta_db_context_create(db, &c, &id);

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE contexts SET type='changed' WHERE id=%d;", id);
    int rc = acta_db_exec(db, sql);
    TEST_ASSERT(rc < 0);
    test_db_teardown(db, path);
}

void run_context_tests(void) {
    fprintf(stderr, "\n=== context.h tests ===\n");
    test_context_create_happy();
    test_context_create_null_type();
    test_context_create_null_content();
    test_context_create_null_hash();
    test_context_create_null_metadata();
    test_context_create_null_struct();
    test_context_get_existing();
    test_context_get_nonexistent();
    test_context_get_null_db();
    test_context_list_by_hash_match();
    test_context_list_by_hash_nomatch();
    test_context_list_by_hash_null();
    test_context_free_valid();
    test_context_free_null();
    test_context_list_free_valid();
    test_context_list_free_null();
    test_context_immutable();
}
