/* test_skill_revision.c — Tests for skill_revision.h */

#include "test_common.h"
#include "skill_revision.h"
#include "skill.h"

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Create a skill and return its id, or fail the test. */
static int sr_create_skill(db_t *db, const char *name) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.name            = (char *)name;
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"You are a helpful assistant.";
    s.output_schema   = (char *)"json";
    s.folder_id       = 0;

    int id = 0;
    int rc = acta_db_skill_create(db, &s, &id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    TEST_ASSERT(id > 0);
    return id;
}

/* Update a skill and return the revision number that should now exist.
 * Returns the expected revision (1 + prior count) on success. */
static int sr_update_skill(db_t *db, int skill_id, const char *name) {
    skill_t s;
    memset(&s, 0, sizeof(s));
    s.id              = skill_id;
    s.name            = (char *)name;
    s.description     = (char *)"desc";
    s.prompt_template = (char *)"You are a helpful assistant.";
    s.output_schema   = (char *)"json";

    int rc = acta_db_skill_update(db, &s);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);
    return rc;
}

/* Open a fresh per-test DB. Aborts the test on failure. */
static db_t *sr_db_open(const char *path) {
    remove(path);
    db_t *db = test_db_open(path);
    TEST_ASSERT_NOT_NULL(db);
    return db;
}

static void sr_db_close(db_t *db, const char *path) {
    test_db_teardown(db, path);
}

/* ── 8.1  get — existing ────────────────────────────────────────────────── */
static void test_sr_get_existing(void) {
    const char *path = "test/acta_test_sr_get.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill");

    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get_by_skill_and_rev(
                               db, skill_id, 1, &err);
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

    sr_db_close(db, path);
}

/* ── 8.2  get — non-existent ────────────────────────────────────────────── */
static void test_sr_get_nonexistent(void) {
    const char *path = "test/acta_test_sr_get404.db";
    db_t *db = sr_db_open(path);

    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get(db, 999999, &err);
    TEST_ASSERT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    sr_db_close(db, path);
}

/* ── 8.3  get — invalid / no-match args ──────────────────────────────── */
static void test_sr_get_invalid_args(void) {
    const char *path = "test/acta_test_sr_get_inv.db";
    db_t *db = sr_db_open(path);

    int err = 0;

    /* NULL db → ACTA_DB_ERR_INVALID */
    skill_revision_t *r = acta_db_skill_revision_get(NULL, 1, &err);
    TEST_ASSERT_NULL(r);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    /* id = 0: no such row → not-found (NULL + OK), same as any missing id */
    err = 0;
    r = acta_db_skill_revision_get(db, 0, &err);
    TEST_ASSERT_NULL(r);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    /* id = -1: same — not-found */
    err = 0;
    r = acta_db_skill_revision_get(db, -1, &err);
    TEST_ASSERT_NULL(r);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    sr_db_close(db, path);
}


/* ── 8.4  get_by_skill_and_rev — existing (after update) ────────────────── */
static void test_sr_get_by_skill_rev_existing(void) {
    const char *path = "test/acta_test_sr_gbsrev.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill84");
    sr_update_skill(db, skill_id, "Updated");

    int err = 0;
    skill_revision_t *rev2 = acta_db_skill_revision_get_by_skill_and_rev(
                                db, skill_id, 2, &err);
    TEST_ASSERT_NOT_NULL(rev2);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(rev2->skill_id, skill_id);
    TEST_ASSERT_EQ_INT(rev2->revision, 2);
    TEST_ASSERT_EQ_STR(rev2->name, "Updated");
    acta_db_skill_revision_free(rev2);

    sr_db_close(db, path);
}

/* ── 8.5  get_by_skill_and_rev — missing ────────────────────────────────── */
static void test_sr_get_by_skill_rev_missing(void) {
    const char *path = "test/acta_test_sr_gbsrev_miss.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill85");

    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get_by_skill_and_rev(
                               db, skill_id, 99, &err);
    TEST_ASSERT_NULL(rev);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    sr_db_close(db, path);
}

/* ── 8.6  list_by_skill — multiple rows ─────────────────────────────────── */
static void test_sr_list_multiple(void) {
    const char *path = "test/acta_test_sr_list.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill86");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 0, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    for (int i = 0; i < count; i++)
        TEST_ASSERT_EQ_INT(items[i]->skill_id, skill_id);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

/* ── 8.7  list_by_skill — includes deleted revisions ────────────────────── */
static void test_sr_list_includes_deleted(void) {
    const char *path = "test/acta_test_sr_list_del.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill87");
    sr_update_skill(db, skill_id, "v2");

    int rc = acta_db_skill_soft_delete(db, skill_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 0, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    TEST_ASSERT_NOT_NULL(items[2]->deleted_at);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

/* ── 8.8  list_by_skill — two skills isolated ───────────────────────────── */
static void test_sr_list_isolated_skills(void) {
    const char *path = "test/acta_test_sr_list_iso.db";
    db_t *db = sr_db_open(path);

    int skill_a = sr_create_skill(db, "SkillA");
    int skill_b = sr_create_skill(db, "SkillB");
    sr_update_skill(db, skill_a, "A2");
    sr_update_skill(db, skill_b, "B2");

    int count_a = 0, err = 0;
    skill_revision_t **items_a = acta_db_skill_revision_list_by_skill(
                                     db, skill_a, 0, 0, &count_a, &err);
    TEST_ASSERT_NOT_NULL(items_a);
    TEST_ASSERT_EQ_INT(count_a, 2);
    for (int i = 0; i < count_a; i++)
        TEST_ASSERT_EQ_INT(items_a[i]->skill_id, skill_a);
    acta_db_skill_revision_list_free(items_a, count_a);

    int count_b = 0;
    skill_revision_t **items_b = acta_db_skill_revision_list_by_skill(
                                     db, skill_b, 0, 0, &count_b, &err);
    TEST_ASSERT_NOT_NULL(items_b);
    TEST_ASSERT_EQ_INT(count_b, 2);
    for (int i = 0; i < count_b; i++)
        TEST_ASSERT_EQ_INT(items_b[i]->skill_id, skill_b);
    acta_db_skill_revision_list_free(items_b, count_b);

    sr_db_close(db, path);
}

/* ── 8.9  free — valid / NULL / list_free ───────────────────────────────── */
static void test_sr_free_valid(void) {
    const char *path = "test/acta_test_sr_free.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill89");
    int err = 0;
    skill_revision_t *rev = acta_db_skill_revision_get_by_skill_and_rev(
                               db, skill_id, 1, &err);
    TEST_ASSERT_NOT_NULL(rev);
    acta_db_skill_revision_free(rev);

    sr_db_close(db, path);
}

static void test_sr_free_null(void) {
    acta_db_skill_revision_free(NULL);
    TEST_ASSERT(1);
}

static void test_sr_list_free_valid(void) {
    const char *path = "test/acta_test_sr_lfree.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill89lf");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 0, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

static void test_sr_list_free_null(void) {
    acta_db_skill_revision_list_free(NULL, 0);
    TEST_ASSERT(1);
}

/* ── 8.10 get_latest — multiple revisions ───────────────────────────────── */
static void test_sr_get_latest_multiple(void) {
    const char *path = "test/acta_test_sr_latest.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill810");
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

    sr_db_close(db, path);
}

/* ── 8.11 get_latest — single revision ──────────────────────────────────── */
static void test_sr_get_latest_single(void) {
    const char *path = "test/acta_test_sr_latest_single.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill811");

    int err = 0;
    skill_revision_t *latest = acta_db_skill_revision_get_latest(db, skill_id, &err);
    TEST_ASSERT_NOT_NULL(latest);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(latest->revision, 1);
    TEST_ASSERT_EQ_STR(latest->name, "RevSkill811");
    acta_db_skill_revision_free(latest);

    sr_db_close(db, path);
}

/* ── 8.12 get_latest — non-existent skill ───────────────────────────────── */
static void test_sr_get_latest_nonexistent(void) {
    const char *path = "test/acta_test_sr_latest_404.db";
    db_t *db = sr_db_open(path);

    int err = 0;
    skill_revision_t *latest = acta_db_skill_revision_get_latest(db, 999999, &err);
    TEST_ASSERT_NULL(latest);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);

    sr_db_close(db, path);
}

/* ── 8.13 list — pagination: first page ─────────────────────────────────── */
static void test_sr_list_paged_first_page(void) {
    const char *path = "test/acta_test_sr_paged1.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill813");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");
    sr_update_skill(db, skill_id, "v4");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->revision, 1);
    TEST_ASSERT_EQ_INT(items[1]->revision, 2);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

/* ── 8.14 list — pagination: second page ────────────────────────────────── */
static void test_sr_list_paged_second_page(void) {
    const char *path = "test/acta_test_sr_paged2.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill814");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");
    sr_update_skill(db, skill_id, "v4");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 2, 2, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->revision, 3);
    TEST_ASSERT_EQ_INT(items[1]->revision, 4);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

/* ── 8.15 list — offset beyond total (empty) ────────────────────────────── */
static void test_sr_list_paged_offset_beyond(void) {
    const char *path = "test/acta_test_sr_paged_beyond.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill815");
    sr_update_skill(db, skill_id, "v2");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 10, 5, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 0);

    sr_db_close(db, path);
}

/* ── 8.16 list — limit=0 means no limit ─────────────────────────────────── */
static void test_sr_list_paged_no_limit(void) {
    const char *path = "test/acta_test_sr_paged_nolimit.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill816");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 0, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 3);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

/* ── 8.17 list — limit > total rows ─────────────────────────────────────── */
static void test_sr_list_paged_limit_exceeds(void) {
    const char *path = "test/acta_test_sr_paged_exceed.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill817");
    sr_update_skill(db, skill_id, "v2");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 100, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(count, 2);
    TEST_ASSERT_EQ_INT(items[0]->revision, 1);
    TEST_ASSERT_EQ_INT(items[1]->revision, 2);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}

/* ── 8.18 list — negative offset → ACTA_DB_ERR_INVALID ─────────────────── */
static void test_sr_list_paged_negative_offset(void) {
    const char *path = "test/acta_test_sr_paged_negoff.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill818");
    sr_update_skill(db, skill_id, "v2");

    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, -5, 2, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);

    sr_db_close(db, path);
}

/* ── 8.19 list — nullable out_count / err ───────────────────────────────── */
static void test_sr_list_nullables(void) {
    const char *path = "test/acta_test_sr_paged_nullables.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkill819");
    sr_update_skill(db, skill_id, "v2");

    /* We know the count is 2 from the setup above; use it for cleanup. */
    const int expected_count = 2;

    /* Both out-params NULL — must not crash */
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 0, NULL, NULL);
    TEST_ASSERT_NOT_NULL(items);
    acta_db_skill_revision_list_free(items, expected_count);

    /* Only err NULL */
    int count = 0;
    skill_revision_t **items2 = acta_db_skill_revision_list_by_skill(
                                   db, skill_id, 0, 0, &count, NULL);
    TEST_ASSERT_NOT_NULL(items2);
    TEST_ASSERT_EQ_INT(count, expected_count);
    acta_db_skill_revision_list_free(items2, count);

    /* Only out_count NULL */
    int err = 0;
    skill_revision_t **items3 = acta_db_skill_revision_list_by_skill(
                                   db, skill_id, 0, 0, NULL, &err);
    TEST_ASSERT_NOT_NULL(items3);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    acta_db_skill_revision_list_free(items3, expected_count);

    sr_db_close(db, path);
}

/* ── 8.20 list — NULL db ────────────────────────────────────────────────── */
static void test_sr_list_null_db(void) {
    int count = 0, err = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  NULL, 1, 0, 0, &count, &err);
    TEST_ASSERT_NULL(items);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_ERR_INVALID);
}

/* ── 8.21 count — multiple revisions ────────────────────────────────────── */
static void test_sr_count_multiple(void) {
    const char *path = "test/acta_test_sr_count.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkillC1");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");

    int err = 0;
    int total = acta_db_skill_revision_count(db, skill_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 3);

    sr_db_close(db, path);
}

/* ── 8.22 count — single revision ───────────────────────────────────────── */
static void test_sr_count_single(void) {
    const char *path = "test/acta_test_sr_count_single.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkillC2");

    int err = 0;
    int total = acta_db_skill_revision_count(db, skill_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 1);

    sr_db_close(db, path);
}

/* ── 8.23 count — non-existent skill (zero rows) ────────────────────────── */
static void test_sr_count_nonexistent(void) {
    const char *path = "test/acta_test_sr_count_404.db";
    db_t *db = sr_db_open(path);

    int err = 0;
    int total = acta_db_skill_revision_count(db, 999999, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 0);

    sr_db_close(db, path);
}

/* ── 8.24 count — includes soft-deleted revisions ───────────────────────── */
static void test_sr_count_includes_deleted(void) {
    const char *path = "test/acta_test_sr_count_del.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkillC4");
    sr_update_skill(db, skill_id, "v2");

    int rc = acta_db_skill_soft_delete(db, skill_id);
    TEST_ASSERT_EQ_INT(rc, ACTA_DB_OK);

    int err = 0;
    int total = acta_db_skill_revision_count(db, skill_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 3);

    sr_db_close(db, path);
}

/* ── 8.25 count — nullable err ──────────────────────────────────────────── */
static void test_sr_count_null_err(void) {
    const char *path = "test/acta_test_sr_count_nerr.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkillC5");

    int total = acta_db_skill_revision_count(db, skill_id, NULL);
    TEST_ASSERT_EQ_INT(total, 1);

    sr_db_close(db, path);
}

/* ── 8.26 count — consistency with lister ───────────────────────────────── */
static void test_sr_count_matches_lister(void) {
    const char *path = "test/acta_test_sr_count_match.db";
    db_t *db = sr_db_open(path);

    int skill_id = sr_create_skill(db, "RevSkillC6");
    sr_update_skill(db, skill_id, "v2");
    sr_update_skill(db, skill_id, "v3");
    sr_update_skill(db, skill_id, "v4");
    sr_update_skill(db, skill_id, "v5");

    int err = 0;
    int total = acta_db_skill_revision_count(db, skill_id, &err);
    TEST_ASSERT_EQ_INT(err, ACTA_DB_OK);
    TEST_ASSERT_EQ_INT(total, 5);

    int count = 0;
    skill_revision_t **items = acta_db_skill_revision_list_by_skill(
                                  db, skill_id, 0, 0, &count, &err);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQ_INT(count, total);
    acta_db_skill_revision_list_free(items, count);

    sr_db_close(db, path);
}


/* ── Runner ──────────────────────────────────────────────────────────────── */
int run_skill_revision_tests(void) {
    fprintf(stderr, "\n=== skill_revision tests (8.1 → 8.26) ===\n");

    /* Getters */
    test_sr_get_existing();
    test_sr_get_nonexistent();
    test_sr_get_invalid_args();
    test_sr_get_by_skill_rev_existing();
    test_sr_get_by_skill_rev_missing();

    /* Lister */
    test_sr_list_multiple();
    test_sr_list_includes_deleted();
    test_sr_list_isolated_skills();

    /* Free */
    test_sr_free_valid();
    test_sr_free_null();
    test_sr_list_free_valid();
    test_sr_list_free_null();

    /* get_latest */
    test_sr_get_latest_multiple();
    test_sr_get_latest_single();
    test_sr_get_latest_nonexistent();

    /* Pagination */
    test_sr_list_paged_first_page();
    test_sr_list_paged_second_page();
    test_sr_list_paged_offset_beyond();
    test_sr_list_paged_no_limit();
    test_sr_list_paged_limit_exceeds();
    test_sr_list_paged_negative_offset();
    test_sr_list_nullables();
    test_sr_list_null_db();

    /* Count */
    test_sr_count_multiple();
    test_sr_count_single();
    test_sr_count_nonexistent();
    test_sr_count_includes_deleted();
    test_sr_count_null_err();
    test_sr_count_matches_lister();

    return test_failures;
}
