#include "test_common.h"
#include "db.h"

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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
    test_db_teardown(db, path);
}

/* ---------- 2.5: context_create — NULL metadata (optional) ---------- */
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_INVALID);
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

    int err = ACTA_DB_ERR_SQL;
    context_t *got = acta_db_context_get(db, id, &err);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
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

    int err = ACTA_DB_ERR_SQL;
    context_t *got = acta_db_context_get(db, 999999, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    test_db_teardown(db, path);
}

/* ---------- 2.9: context_get — NULL db ---------- */
static void test_context_get_null_db(void) {
    int err = ACTA_DB_OK;
    context_t *got = acta_db_context_get(NULL, 1, &err);
    TEST_ASSERT_NULL(got);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
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
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_hash(db, "same_hash", 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
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
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_hash(db, "nonexistent_hash", 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
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
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_by_hash(db, NULL, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
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
    int err = 0;
    context_t *got = acta_db_context_get(db, id, &err);
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
    int err = 0;
    context_t **items = acta_db_context_list_by_hash(db, "bulk", 0, 0, &count, &err);
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
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_ERR_SQL);
    test_db_teardown(db, path);
}

/* ---------- 2.18: context_list_all — happy path ---------- */
static void test_context_list_all_happy(void) {
    const char *path = "test/acta_test_ctx_listall.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c1 = { .type = "a", .content = "x", .content_hash = "h1" };
    context_t c2 = { .type = "b", .content = "y", .content_hash = "h2" };
    context_t c3 = { .type = "c", .content = "z", .content_hash = "h3" };
    int id1, id2, id3;
    acta_db_context_create(db, &c1, &id1);
    acta_db_context_create(db, &c2, &id2);
    acta_db_context_create(db, &c3, &id3);

    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    TEST_ASSERT_EQ_INT(items[1]->id, id2);
    TEST_ASSERT_EQ_INT(items[2]->id, id3);
    TEST_ASSERT_EQ_STR(items[0]->type, "a");
    TEST_ASSERT_EQ_STR(items[1]->type, "b");
    TEST_ASSERT_EQ_STR(items[2]->type, "c");
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.19: context_list_all — empty table ---------- */
static void test_context_list_all_empty(void) {
    const char *path = "test/acta_test_ctx_listallempty.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = -1;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_all(db, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.20: context_list_all — NULL db ---------- */
static void test_context_list_all_null_db(void) {
    int count = 0;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_all(NULL, 0, 0, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- 2.21: context_list_by_type — match ---------- */
static void test_context_list_by_type_match(void) {
    const char *path = "test/acta_test_ctx_listtype.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c1 = { .type = "doc", .content = "a", .content_hash = "h1" };
    context_t c2 = { .type = "doc", .content = "b", .content_hash = "h2" };
    context_t c3 = { .type = "image", .content = "c", .content_hash = "h3" };
    int id1, id2, id3;
    acta_db_context_create(db, &c1, &id1);
    acta_db_context_create(db, &c2, &id2);
    acta_db_context_create(db, &c3, &id3);

    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_type(db, "doc", 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, id1);
    TEST_ASSERT_EQ_INT(items[1]->id, id2);
    TEST_ASSERT_EQ_STR(items[0]->type, "doc");
    TEST_ASSERT_EQ_STR(items[1]->type, "doc");
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.22: context_list_by_type — no match ---------- */
static void test_context_list_by_type_nomatch(void) {
    const char *path = "test/acta_test_ctx_listtypenomatch.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = { .type = "doc", .content = "a", .content_hash = "h1" };
    int id;
    acta_db_context_create(db, &c, &id);

    int count = -1;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_type(db, "audio", 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.23: context_list_by_type — NULL type ---------- */
static void test_context_list_by_type_null(void) {
    const char *path = "test/acta_test_ctx_listtypenull.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = -1;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_by_type(db, NULL, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.24: context_list_by_type — NULL db ---------- */
static void test_context_list_by_type_null_db(void) {
    int count = 0;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_by_type(NULL, "doc", 0, 0, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ---------- 2.25: context_list_all — limit truncates ---------- */
static void test_context_list_all_limit(void) {
    const char *path = "test/acta_test_ctx_listall_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ids[5];
    for (int i = 0; i < 5; i++) {
        context_t c = { .type = "t", .content = "c", .content_hash = "h" };
        acta_db_context_create(db, &c, &ids[i]);
    }

    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_all(db, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[0]);
    TEST_ASSERT_EQ_INT(items[1]->id, ids[1]);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.26: context_list_all — offset skips rows ---------- */
static void test_context_list_all_offset(void) {
    const char *path = "test/acta_test_ctx_listall_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ids[4];
    for (int i = 0; i < 4; i++) {
        context_t c = { .type = "t", .content = "c", .content_hash = "h" };
        acta_db_context_create(db, &c, &ids[i]);
    }

    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_all(db, 2, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[2]);
    TEST_ASSERT_EQ_INT(items[1]->id, ids[3]);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.27: context_list_all — offset + limit (window) ---------- */
static void test_context_list_all_offset_limit(void) {
    const char *path = "test/acta_test_ctx_listall_win.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ids[5];
    for (int i = 0; i < 5; i++) {
        context_t c = { .type = "t", .content = "c", .content_hash = "h" };
        acta_db_context_create(db, &c, &ids[i]);
    }

    /* Skip first 2, take next 2 → rows 3 and 4 (0-indexed 2,3) */
    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_all(db, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[2]);
    TEST_ASSERT_EQ_INT(items[1]->id, ids[3]);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.28: context_list_all — offset beyond result set ---------- */
static void test_context_list_all_offset_beyond(void) {
    const char *path = "test/acta_test_ctx_listall_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    context_t c = { .type = "t", .content = "c", .content_hash = "h" };
    acta_db_context_create(db, &c, &(int){0});

    int count = -1;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_all(db, 10, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.29: context_list_all — negative offset ---------- */
static void test_context_list_all_neg_offset(void) {
    const char *path = "test/acta_test_ctx_listall_negoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_all(db, -1, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.30: context_list_by_type — offset + limit ---------- */
static void test_context_list_by_type_offset_limit(void) {
    const char *path = "test/acta_test_ctx_listtype_win.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int doc_ids[3];
    for (int i = 0; i < 3; i++) {
        context_t c = { .type = "doc", .content = "c", .content_hash = "h" };
        acta_db_context_create(db, &c, &doc_ids[i]);
    }
    /* one non-doc row to ensure type filter still applies */
    context_t other = { .type = "image", .content = "c", .content_hash = "h" };
    acta_db_context_create(db, &other, &(int){0});

    /* offset=1, limit=1 → second doc only */
    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_type(db, "doc", 1, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, doc_ids[1]);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.31: context_list_by_hash — limit ---------- */
static void test_context_list_by_hash_limit(void) {
    const char *path = "test/acta_test_ctx_hashlimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ids[3];
    for (int i = 0; i < 3; i++) {
        context_t c = { .type = "t", .content = "c", .content_hash = "h" };
        acta_db_context_create(db, &c, &ids[i]);
    }

    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_hash(db, "h", 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[0]);
    TEST_ASSERT_EQ_INT(items[1]->id, ids[1]);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.32: context_list_by_hash — offset ---------- */
static void test_context_list_by_hash_offset(void) {
    const char *path = "test/acta_test_ctx_hashoffset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int ids[3];
    for (int i = 0; i < 3; i++) {
        context_t c = { .type = "t", .content = "c", .content_hash = "h" };
        acta_db_context_create(db, &c, &ids[i]);
    }

    int count = 0;
    int err = ACTA_DB_ERR_SQL;
    context_t **items = acta_db_context_list_by_hash(db, "h", 2, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(items[0]->id, ids[2]);
    acta_db_context_list_free(items, count);
    test_db_teardown(db, path);
}

/* ---------- 2.33: context_list_by_type — negative offset ---------- */
static void test_context_list_by_type_neg_offset(void) {
    const char *path = "test/acta_test_ctx_listtype_negoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_by_type(db, "doc", -1, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.34: context_list_by_hash — negative offset ---------- */
static void test_context_list_by_hash_neg_offset(void) {
    const char *path = "test/acta_test_ctx_hashnegoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int count = 0;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_by_hash(db, "h", -1, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);
    TEST_ASSERT_NULL(items);
    test_db_teardown(db, path);
}

/* ---------- 2.35: context_list_all — NULL db with pagination params ---------- */
static void test_context_list_all_null_db_paged(void) {
    int count = 0;
    int err = ACTA_DB_OK;
    context_t **items = acta_db_context_list_all(NULL, 5, 10, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
    TEST_ASSERT_EQ_INT(count, 0);
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
    test_context_list_all_happy();
    test_context_list_all_empty();
    test_context_list_all_null_db();
    test_context_list_by_type_match();
    test_context_list_by_type_nomatch();
    test_context_list_by_type_null();
    test_context_list_by_type_null_db();
    /* new pagination tests */
    test_context_list_all_limit();
    test_context_list_all_offset();
    test_context_list_all_offset_limit();
    test_context_list_all_offset_beyond();
    test_context_list_all_neg_offset();
    test_context_list_by_type_offset_limit();
    test_context_list_by_hash_limit();
    test_context_list_by_hash_offset();
    test_context_list_by_type_neg_offset();
    test_context_list_by_hash_neg_offset();
    test_context_list_all_null_db_paged();
}
