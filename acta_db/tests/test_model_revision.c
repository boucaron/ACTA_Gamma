/* test_model_revision.c — Tests for model_revision.h */

#include "test_common.h"
#include "model_revision.h"

/* ── helpers ───────────────────────────────────────────────────────── */

/* Populate a model_t with fixed test values, overriding name/id as needed.
 * The (char *) casts are required because model_t uses char* fields but
 * the API only reads them (const model_t *). */
static void model_init(model_t *m, int id, const char *name, const char *ident) {
    memset(m, 0, sizeof(*m));
    m->id               = id;
    m->name             = (char *)name;
    m->model_identifier = (char *)ident;
    m->description      = (char *)"desc";
    m->backend          = (char *)"openai";
    m->base_url         = (char *)"http://localhost:8080";
    m->configuration    = (char *)"{ }";
    m->folder_id        = 0;
}

static int mr_create(db_t *db, const char *name) {
    model_t m;
    model_init(&m, 0, name, name);   /* ident = name at creation */
    int id = 0;
    return (acta_db_model_create(db, &m, &id) == ACTA_DB_OK) ? id : -1;
}

static int mr_update(db_t *db, int model_id, const char *new_name, const char *ident) {
    model_t m;
    model_init(&m, model_id, new_name, ident);
    return acta_db_model_update(db, &m);
}

static int mr_create_with_revs(db_t *db, const char *name, int n_updates) {
    int id = mr_create(db, name);
    if (id <= 0) return -1;
    for (int i = 1; i <= n_updates; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s_v%d", name, i + 1);
        if (mr_update(db, id, buf, name) != ACTA_DB_OK) return -1;
    }
    return id;
}




/* ── getters ───────────────────────────────────────────────────────── */

static void test_mr_get_existing(void) {
    const char *path = "test/acta_test_mr_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModel");
    TEST_ASSERT(model_id > 0);

    /* get_by_model_and_rev → capture id */
    int err = 0;
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(
        db, model_id, 1, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(rev);
    int rev_id = rev->id;
    acta_db_model_revision_free(rev);

    /* get by id → verify round-trip */
    model_revision_t *by_id = acta_db_model_revision_get(db, rev_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(by_id);
    TEST_ASSERT_EQ_INT(by_id->id, rev_id);
    TEST_ASSERT_EQ_INT(by_id->model_id, model_id);
    TEST_ASSERT_EQ_INT(by_id->revision, 1);
    TEST_ASSERT_EQ_STR(by_id->name, "RevModel");
    TEST_ASSERT(by_id->deleted_at == NULL);
    acta_db_model_revision_free(by_id);

    test_db_teardown(db, path);
}

static void test_mr_get_nonexistent(void) {
    const char *path = "test/acta_test_mr_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = ACTA_DB_OK;
    model_revision_t *rev = acta_db_model_revision_get(db, 999999, &err);
    TEST_ASSERT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);   /* not-found is OK */

    test_db_teardown(db, path);
}

static void test_mr_get_by_model_rev_existing(void) {
    const char *path = "test/acta_test_mr_gbmrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModel53");
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(mr_update(db, model_id, "Updated", "RevModel53"), ACTA_DB_OK);


    int err = 0;
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(
        db, model_id, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(rev);
    TEST_ASSERT_EQ_INT(rev->revision, 2);
    TEST_ASSERT_EQ_STR(rev->name, "Updated");
    acta_db_model_revision_free(rev);

    test_db_teardown(db, path);
}

static void test_mr_get_by_model_rev_missing(void) {
    const char *path = "test/acta_test_mr_gbmrev_miss.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModel54");
    TEST_ASSERT(model_id > 0);

    int err = ACTA_DB_OK;
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(
        db, model_id, 99, &err);
    TEST_ASSERT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* Soft-delete triggers a new revision (the UPDATE fires the trigger).
 * The latest revision should carry deleted_at. */
static void test_mr_get_by_model_rev_deleted(void) {
    const char *path = "test/acta_test_mr_gbmrev_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModel55");
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);

    /* After create(1) + soft_delete(2), rev 2 is the delete snapshot */
    int err = 0;
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(
        db, model_id, 2, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(rev);
    TEST_ASSERT_EQ_INT(rev->revision, 2);
    TEST_ASSERT_NOT_NULL(rev->deleted_at);
    acta_db_model_revision_free(rev);

    test_db_teardown(db, path);
}

/* ── get_latest ────────────────────────────────────────────────────── */

static void test_mr_get_latest_multi(void) {
    const char *path = "test/acta_test_mr_latest_multi.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel519", 2);  /* revs 1,2,3 */
    TEST_ASSERT(model_id > 0);

    int err = 0;
    model_revision_t *latest = acta_db_model_revision_get_latest(db, model_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(latest->revision, 3);
    TEST_ASSERT_EQ_STR(latest->name, "RevModel519_v3");

    acta_db_model_revision_free(latest);

    test_db_teardown(db, path);
}

static void test_mr_get_latest_single(void) {
    const char *path = "test/acta_test_mr_latest_single.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModel520");
    TEST_ASSERT(model_id > 0);

    int err = 0;
    model_revision_t *latest = acta_db_model_revision_get_latest(db, model_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(latest->revision, 1);
    TEST_ASSERT_EQ_STR(latest->name, "RevModel520");
    acta_db_model_revision_free(latest);

    test_db_teardown(db, path);
}

static void test_mr_get_latest_nonexistent(void) {
    const char *path = "test/acta_test_mr_latest_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = ACTA_DB_OK;
    model_revision_t *latest = acta_db_model_revision_get_latest(db, 999999, &err);
    TEST_ASSERT_NULL(latest);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* KI-3: get by id must not return soft-deleted rows — NULL with
 * ACTA_DB_OK (not-found), consistent with context/exec getters. */
static void test_mr_get_deleted(void) {
    const char *path = "test/acta_test_mr_get_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel522", 1);  /* revs 1,2 */
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);
    /* soft-delete is an UPDATE → rev 3 carries deleted_at */
    int derr = 0;
    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(
        db, model_id, 3, &derr);
    TEST_ASSERT_NOT_NULL(rev);
    int deleted_id = rev->id;
    acta_db_model_revision_free(rev);

    int err = 0;
    model_revision_t *r = acta_db_model_revision_get(db, deleted_id, &err);
    TEST_ASSERT_NULL(r);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);   /* not-found is OK */

    test_db_teardown(db, path);
}

/* KI-2: get_latest must skip soft-deleted revisions — latest live rev
 * of a model with revs 1,2,3 where rev 3 is a soft-delete snapshot
 * is rev 2. */
static void test_mr_get_latest_skips_deleted(void) {
    const char *path = "test/acta_test_mr_latest_deleted.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel521", 1);  /* revs 1,2 */
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);
    /* soft-delete is an UPDATE → rev 3 carries deleted_at */

    int err = 0;
    model_revision_t *latest = acta_db_model_revision_get_latest(db, model_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(latest->revision, 2);
    TEST_ASSERT(latest->deleted_at == NULL);

    acta_db_model_revision_free(latest);

    test_db_teardown(db, path);
}

/* ── lister ────────────────────────────────────────────────────────── */

static void test_mr_list_multiple(void) {
    const char *path = "test/acta_test_mr_list.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel56", 2);  /* 3 revs */
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_id, model_id);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_mr_list_ordering(void) {
    const char *path = "test/acta_test_mr_list_order.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel57", 2);
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++)
        TEST_ASSERT_EQ_INT(items[i]->revision, i + 1);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* KI-4: default lister is live-only; the _with_deleted variant
 * includes soft-deleted rows.  revs: 1=create, 2=update, 3=soft-delete. */
static void test_mr_list_with_deleted_includes_deleted(void) {
    const char *path = "test/acta_test_mr_list_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel58", 1);  /* revs 1,2 */
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model_with_deleted(
        db, model_id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_NOT_NULL(items[2]->deleted_at);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_mr_list_excludes_deleted(void) {
    const char *path = "test/acta_test_mr_list_livedel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel527", 1);  /* revs 1,2 */
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);
    /* rev 3 carries deleted_at → default lister must skip it */

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    for (int i = 0; i < count; i++)
        TEST_ASSERT(items[i]->deleted_at == NULL);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ── pagination ────────────────────────────────────────────────────── */

static void test_mr_list_pagination_limit(void) {
    const char *path = "test/acta_test_mr_list_limit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel59", 3);  /* 4 revs */
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->revision, 1);
    TEST_ASSERT_EQ_INT(items[1]->revision, 2);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_mr_list_pagination_offset(void) {
    const char *path = "test/acta_test_mr_list_offset.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel510", 3);  /* 4 revs */
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 2, 2, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->revision, 3);
    TEST_ASSERT_EQ_INT(items[1]->revision, 4);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_mr_list_offset_beyond_total(void) {
    const char *path = "test/acta_test_mr_list_off_beyond.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel511", 1);  /* 2 revs */
    TEST_ASSERT(model_id > 0);

    int count = 99, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 10, 5, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 0);

    test_db_teardown(db, path);
}

static void test_mr_list_limit_one(void) {
    const char *path = "test/acta_test_mr_list_limit1.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel512", 1);  /* 2 revs */
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, 1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 1);
    TEST_ASSERT_EQ_INT(items[0]->revision, 1);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* limit < 0 means "no limit" (same as 0) */
static void test_mr_list_negative_limit(void) {
    const char *path = "test/acta_test_mr_list_neglimit.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel513", 2);  /* 3 revs */
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, -1, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* Cross-model isolation: revisions of model A must not appear in model B's list. */
static void test_mr_list_cross_model_isolation(void) {
    const char *path = "test/acta_test_mr_list_cross.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_a = mr_create_with_revs(db, "ModelA", 1);  /* 2 revs */
    TEST_ASSERT(model_a > 0);

    int model_b = mr_create_with_revs(db, "ModelB", 2);  /* 3 revs */
    TEST_ASSERT(model_b > 0);

    /* model A sees exactly its own 2 revisions */
    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_a, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    for (int i = 0; i < count; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_id, model_a);
    acta_db_model_revision_list_free(items, count);

    /* model B sees exactly its own 3 revisions */
    count = 0; err = 0;
    items = acta_db_model_revision_list_by_model(
        db, model_b, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++)
        TEST_ASSERT_EQ_INT(items[i]->model_id, model_b);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}



/* ── count ─────────────────────────────────────────────────────────── */

static void test_mr_count_multi(void) {
    const char *path = "test/acta_test_mr_count_multi.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel523", 2);  /* 3 revs */
    TEST_ASSERT(model_id > 0);

    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_model_revision_count(db, model_id, &err), 3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

static void test_mr_count_single(void) {
    const char *path = "test/acta_test_mr_count_single.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModel524");
    TEST_ASSERT(model_id > 0);

    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_model_revision_count(db, model_id, &err), 1);

    test_db_teardown(db, path);
}

static void test_mr_count_nonexistent(void) {
    const char *path = "test/acta_test_mr_count_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_model_revision_count(db, 999999, &err), 0);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    test_db_teardown(db, path);
}

/* KI-4: default counter is live-only; count_with_deleted includes
 * soft-deleted rows.  revs: 1=create, 2=update, 3=soft-delete. */
static void test_mr_count_with_deleted_includes_deleted(void) {
    const char *path = "test/acta_test_mr_count_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel526", 1);  /* revs 1,2 */
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);

    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(
        acta_db_model_revision_count_with_deleted(db, model_id, &err), 3);

    test_db_teardown(db, path);
}

static void test_mr_count_excludes_deleted(void) {
    const char *path = "test/acta_test_mr_count_livedel.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModel528", 1);  /* revs 1,2 */
    TEST_ASSERT(model_id > 0);
    TEST_ASSERT_EQ_INT(acta_db_model_soft_delete(db, model_id), ACTA_DB_OK);
    /* rev 3 carries deleted_at → default counter must skip it */

    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_model_revision_count(db, model_id, &err), 2);

    test_db_teardown(db, path);
}

/* ── invalid-argument guards ───────────────────────────────────────── */

static void test_mr_get_null_db(void) {
    int err = ACTA_DB_OK;
    TEST_ASSERT_NULL(acta_db_model_revision_get(NULL, 1, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_mr_get_invalid_id(void) {
    const char *path = "test/acta_test_mr_get_inv_id.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = ACTA_DB_OK;
    TEST_ASSERT_NULL(acta_db_model_revision_get(db, 0, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    err = ACTA_DB_OK;
    TEST_ASSERT_NULL(acta_db_model_revision_get(db, -1, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_mr_get_by_model_rev_null_db(void) {
    int err = ACTA_DB_OK;
    TEST_ASSERT_NULL(acta_db_model_revision_get_by_model_and_rev(NULL, 1, 1, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_mr_get_latest_null_db(void) {
    int err = ACTA_DB_OK;
    TEST_ASSERT_NULL(acta_db_model_revision_get_latest(NULL, 1, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_mr_list_null_db(void) {
    int count = 0, err = ACTA_DB_OK;
    TEST_ASSERT_NULL(acta_db_model_revision_list_by_model(NULL, 1, 0, 0, &count, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_mr_list_negative_offset(void) {
    const char *path = "test/acta_test_mr_list_negoff.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = ACTA_DB_OK;
    int count = 0;
    TEST_ASSERT_NULL(acta_db_model_revision_list_by_model(db, 1, -1, 10, &count, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    test_db_teardown(db, path);
}

static void test_mr_list_with_deleted_null_db(void) {
    int count = 0, err = ACTA_DB_OK;
    TEST_ASSERT_NULL(
        acta_db_model_revision_list_by_model_with_deleted(NULL, 1, 0, 0, &count, &err));
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_mr_count_null_db(void) {
    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_model_revision_count(NULL, 1, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

static void test_mr_count_with_deleted_null_db(void) {
    int err = ACTA_DB_OK;
    TEST_ASSERT_EQ_INT(acta_db_model_revision_count_with_deleted(NULL, 1, &err), -1);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ── free / list_free ──────────────────────────────────────────────── */

static void test_mr_free_valid(void) {
    const char *path = "test/acta_test_mr_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create(db, "RevModelFree");
    TEST_ASSERT(model_id > 0);

    model_revision_t *rev = acta_db_model_revision_get_by_model_and_rev(
        db, model_id, 1, NULL);
    TEST_ASSERT_NOT_NULL(rev);
    acta_db_model_revision_free(rev);

    test_db_teardown(db, path);
}

static void test_mr_free_null(void) {
    acta_db_model_revision_free(NULL);  /* must not crash */
    TEST_ASSERT(1);
}

static void test_mr_list_free_valid(void) {
    const char *path = "test/acta_test_mr_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int model_id = mr_create_with_revs(db, "RevModelLFree", 2);
    TEST_ASSERT(model_id > 0);

    int count = 0, err = 0;
    model_revision_t **items = acta_db_model_revision_list_by_model(
        db, model_id, 0, 0, &count, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_model_revision_list_free(items, count);

    test_db_teardown(db, path);
}

static void test_mr_list_free_null(void) {
    acta_db_model_revision_list_free(NULL, 0);  /* must not crash */
    TEST_ASSERT(1);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_model_revision_tests(void) {
    fprintf(stderr, "\n=== model_revision tests ===\n");

    /* getters */
    test_mr_get_existing();
    test_mr_get_nonexistent();
    test_mr_get_by_model_rev_existing();
    test_mr_get_by_model_rev_missing();
    test_mr_get_by_model_rev_deleted();

    /* get_latest */
    test_mr_get_latest_multi();
    test_mr_get_latest_single();
    test_mr_get_latest_nonexistent();
    test_mr_get_latest_skips_deleted();
    test_mr_get_deleted();

    /* lister */
    test_mr_list_multiple();
    test_mr_list_ordering();
    test_mr_list_with_deleted_includes_deleted();
    test_mr_list_excludes_deleted();
    test_mr_list_cross_model_isolation();

    /* pagination */
    test_mr_list_pagination_limit();
    test_mr_list_pagination_offset();
    test_mr_list_offset_beyond_total();
    test_mr_list_limit_one();
    test_mr_list_negative_limit();

    /* count */
    test_mr_count_multi();
    test_mr_count_single();
    test_mr_count_nonexistent();
    test_mr_count_with_deleted_includes_deleted();
    test_mr_count_excludes_deleted();

    /* invalid-argument guards */
    test_mr_get_null_db();
    test_mr_get_invalid_id();
    test_mr_get_by_model_rev_null_db();
    test_mr_get_latest_null_db();
    test_mr_list_null_db();
    test_mr_list_with_deleted_null_db();
    test_mr_list_negative_offset();
    test_mr_count_null_db();
    test_mr_count_with_deleted_null_db();

    /* free / list_free */
    test_mr_free_valid();
    test_mr_free_null();
    test_mr_list_free_valid();
    test_mr_list_free_null();

    return test_failures;
}
