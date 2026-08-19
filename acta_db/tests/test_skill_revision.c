/* test_skill_revision.c — Tests for skill_revision.h (tests 8.1 – 8.10) */

#include "test_common.h"
#include "skill_revision.h"
#include "skill.h"

/* Short helper to keep test bodies readable (skill create has many params) */
static int sr_create_skill(db_t *db, const char *name) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name = (char *)name;
    s.description = (char *)"desc";
    s.prompt_template = (char *)"You are a helpful assistant.";
    s.output_schema = (char *)"json";
    s.folder_id = 0;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    return (rc == ACTA_DB_OK) ? id : -1;
}

static int sr_update_skill(db_t *db, int skill_id, const char *name) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id = skill_id;
    s.name = (char *)name;
    s.description = (char *)"desc";
    s.prompt_template = (char *)"You are a helpful assistant.";
    s.output_schema = (char *)"json";

    return acta_db_skill_update(db, &s);
}


/* ---------- 8.1: get — existing ---------- */
static void test_sr_get_existing(void) {
    const char *path = "test/acta_test_sr_get.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill");
    TEST_ASSERT(skill_id > 0);

    /* Grab rev 1 by (skill_id, 1), then fetch by its row id */
    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    TEST_ASSERT_NOT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    int rev_id = rev->id;
    acta_db_skill_revision_free(rev);

    err = 0;
    skill_revision_t *by_id = acta_db_skill_revision_get(db, rev_id, &err);
    TEST_ASSERT_NOT_NULL(by_id);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(by_id->id, rev_id);
    TEST_ASSERT_EQ_INT(by_id->skill_id, skill_id);
    TEST_ASSERT_EQ_INT(by_id->revision, 1);
    TEST_ASSERT_EQ_STR(by_id->name, "RevSkill");
    TEST_ASSERT(by_id->deleted_at == NULL);
    acta_db_skill_revision_free(by_id);

    test_db_teardown(db, path);
}

/* ---------- 8.2: get — non-existent ---------- */
static void test_sr_get_nonexistent(void) {
    const char *path = "test/acta_test_sr_get404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get(db, 999999, &err);
    TEST_ASSERT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);   /* not-found → OK, caller checks NULL */

    test_db_teardown(db, path);
}

/* ---------- 8.3: get_by_skill_and_rev — existing ---------- */
static void test_sr_get_by_skill_rev_existing(void) {
    const char *path = "test/acta_test_sr_gbsrev.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill83");
    TEST_ASSERT(skill_id > 0);

    int rc = sr_update_skill(db, skill_id, "Updated");
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    skill_revision_t *rev2 = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 2, &err);
    TEST_ASSERT_NOT_NULL(rev2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev2->skill_id, skill_id);
    TEST_ASSERT_EQ_INT(rev2->revision, 2);
    TEST_ASSERT_EQ_STR(rev2->name, "Updated");
    acta_db_skill_revision_free(rev2);

    test_db_teardown(db, path);
}

/* ---------- 8.4: get_by_skill_and_rev — missing ---------- */
static void test_sr_get_by_skill_rev_missing(void) {
    const char *path = "test/acta_test_sr_gbsrev_miss.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill84");
    TEST_ASSERT(skill_id > 0);

    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 99, &err);
    TEST_ASSERT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);   /* not-found → OK, caller checks NULL */

    test_db_teardown(db, path);
}

/* ---------- 8.5: list_by_skill — multiple ---------- */
static void test_sr_list_multiple(void) {
    const char *path = "test/acta_test_sr_list.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill85");
    TEST_ASSERT(skill_id > 0);
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int count = 0;
    int err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(db, skill_id, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQ_INT(items[i]->skill_id, skill_id);
    }
    acta_db_skill_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 8.6: list_by_skill — includes deleted ---------- */
static void test_sr_list_includes_deleted(void) {
    const char *path = "test/acta_test_sr_list_del.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill86");
    TEST_ASSERT(skill_id > 0);
    sr_update_skill(db, skill_id, "v2");

    int rc = acta_db_skill_soft_delete(db, skill_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int count = 0;
    int err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(db, skill_id, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    /* 3 rows: rev1 (create), rev2 (update), rev3 (delete) */
    TEST_ASSERT_EQ_INT(count, 3);

    /* Last row is the deleted revision */
    TEST_ASSERT_NOT_NULL(items[2]->deleted_at);
    acta_db_skill_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 8.7: free — valid & NULL, list_free — valid ---------- */
static void test_sr_free_valid(void) {
    const char *path = "test/acta_test_sr_free.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill87");
    TEST_ASSERT(skill_id > 0);

    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    TEST_ASSERT_NOT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    acta_db_skill_revision_free(rev);

    test_db_teardown(db, path);
}

static void test_sr_free_null(void) {
    acta_db_skill_revision_free(NULL);
    TEST_ASSERT(1);
}

static void test_sr_list_free_valid(void) {
    const char *path = "test/acta_test_sr_lfree.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill87lf");
    TEST_ASSERT(skill_id > 0);
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int count = 0;
    int err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(db, skill_id, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_skill_revision_list_free(items, count);

    test_db_teardown(db, path);
}

/* ---------- 8.8: get_latest — multiple revisions, returns highest ---------- */
static void test_sr_get_latest_multiple(void) {
    const char *path = "test/acta_test_sr_latest.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill88");
    TEST_ASSERT(skill_id > 0);
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int err = 0;
    skill_revision_t *latest = acta_db_skill_revision_get_latest(db, skill_id, &err);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(latest->skill_id, skill_id);
    TEST_ASSERT_EQ_INT(latest->revision, 3);
    TEST_ASSERT_EQ_STR(latest->name, "v3");
    acta_db_skill_revision_free(latest);

    test_db_teardown(db, path);
}

/* ---------- 8.9: get_latest — single revision ---------- */
static void test_sr_get_latest_single(void) {
    const char *path = "test/acta_test_sr_latest_single.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int skill_id = sr_create_skill(db, "RevSkill89");
    TEST_ASSERT(skill_id > 0);

    int err = 0;
    skill_revision_t *latest = acta_db_skill_revision_get_latest(db, skill_id, &err);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(latest->skill_id, skill_id);
    TEST_ASSERT_EQ_INT(latest->revision, 1);
    TEST_ASSERT_EQ_STR(latest->name, "RevSkill89");
    acta_db_skill_revision_free(latest);

    test_db_teardown(db, path);
}

/* ---------- 8.10: get_latest — non-existent skill ---------- */
static void test_sr_get_latest_nonexistent(void) {
    const char *path = "test/acta_test_sr_latest_404.db";
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);

    int err = 0;
    skill_revision_t *latest = acta_db_skill_revision_get_latest(db, 999999, &err);
    TEST_ASSERT_NULL(latest);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);   /* not-found → OK, caller checks NULL */

    test_db_teardown(db, path);
}

/* ---------- runner ---------- */
void run_skill_revision_tests(void) {
    fprintf(stderr, "\n=== skill_revision tests ===\n");
    test_sr_get_existing();
    test_sr_get_nonexistent();
    test_sr_get_by_skill_rev_existing();
    test_sr_get_by_skill_rev_missing();
    test_sr_list_multiple();
    test_sr_list_includes_deleted();
    test_sr_free_valid();
    test_sr_free_null();
    test_sr_list_free_valid();
    test_sr_get_latest_multiple();
    test_sr_get_latest_single();
    test_sr_get_latest_nonexistent();
}
