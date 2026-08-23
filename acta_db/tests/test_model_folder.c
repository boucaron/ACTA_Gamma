/* test_model_folder.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"
#include "model_folder.h"

/* ── Minimal test harness ────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define T_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d): %s\n", \
                __func__, __LINE__, msg); \
        g_fail++; \
        return; \
    } else { \
        g_pass++; \
    } \
} while (0)

/* ── Fixture ─────────────────────────────────────────────────────── */

static db_t *g_db = NULL;

static void setup(void)
{
    g_db = acta_db_open(":memory:", NULL);
    if (!g_db) {
        fprintf(stderr, "  FATAL: cannot open in-memory db\n");
        return;
    }
    acta_db_exec(g_db,
        "CREATE TABLE model_folders ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT NOT NULL,"
        "  parent_id  INTEGER REFERENCES model_folders(id),"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now')),"
        "  deleted_at TEXT NULL"
        ");");
}

static void teardown(void)
{
    if (g_db) {
        acta_db_close(g_db);
        g_db = NULL;
    }
}

/* ── Helpers ─────────────────────────────────────────────────────── */

static int make_folder(const char *name, int parent_id, int *out_id)
{
    return acta_db_model_folder_create(g_db, name, parent_id, out_id);
}

/* ── create ──────────────────────────────────────────────────────── */

static void test_create_basic(void)
{
    int id = -1;
    int rc = make_folder("General", 0, &id);
    T_ASSERT(rc == ACTA_DB_OK, "create returns OK");
    T_ASSERT(id > 0, "create returns valid id");
}

static void test_create_child(void)
{
    int parent, child;
    T_ASSERT(make_folder("Parent", 0, &parent) == ACTA_DB_OK, "parent ok");
    T_ASSERT(make_folder("Child", parent, &child) == ACTA_DB_OK, "child ok");
    T_ASSERT(child != parent, "child id differs");
}

static void test_create_null_name(void)
{
    int id;
    int rc = acta_db_model_folder_create(g_db, NULL, 0, &id);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL name → INVALID");
}

static void test_create_null_db(void)
{
    int id;
    int rc = acta_db_model_folder_create(NULL, "x", 0, &id);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_create_null_out_id(void)
{
    int rc = acta_db_model_folder_create(g_db, "NoOutId", 0, NULL);
    T_ASSERT(rc == ACTA_DB_OK, "NULL out_id still succeeds");
}

/* ── get ─────────────────────────────────────────────────────────── */

static void test_get_found(void)
{
    int id;
    make_folder("FindMe", 0, &id);

    int err = -999;
    model_folder_t *f = acta_db_model_folder_get(g_db, id, &err);
    T_ASSERT(f != NULL, "row found");
    T_ASSERT(err == ACTA_DB_OK, "err is OK");
    T_ASSERT(f->id == id, "id matches");
    T_ASSERT(strcmp(f->name, "FindMe") == 0, "name matches");
    T_ASSERT(f->parent_id == 0, "parent_id is 0 for root");
    acta_db_model_folder_free(f);
}

static void test_get_not_found(void)
{
    int err = -999;
    model_folder_t *f = acta_db_model_folder_get(g_db, 99999, &err);
    T_ASSERT(f == NULL, "not-found returns NULL");
    T_ASSERT(err == ACTA_DB_OK, "not-found err is OK");
}

static void test_get_null_db(void)
{
    int err;
    model_folder_t *f = acta_db_model_folder_get(NULL, 1, &err);
    T_ASSERT(f == NULL, "NULL db → NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_get_invalid_id(void)
{
    int err;
    model_folder_t *f = acta_db_model_folder_get(g_db, 0, &err);
    T_ASSERT(f == NULL, "id=0 → NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "id=0 → INVALID");
}

static void test_get_null_err(void)
{
    int id;
    make_folder("NoErr", 0, &id);
    model_folder_t *f = acta_db_model_folder_get(g_db, id, NULL);
    T_ASSERT(f != NULL, "works with NULL err");
    acta_db_model_folder_free(f);
}

/* ── rename ──────────────────────────────────────────────────────── */

static void test_rename(void)
{
    int id;
    make_folder("Before", 0, &id);

    int rc = acta_db_model_folder_rename(g_db, id, "After");
    T_ASSERT(rc == ACTA_DB_OK, "rename ok");

    model_folder_t *f = acta_db_model_folder_get(g_db, id, NULL);
    T_ASSERT(f && strcmp(f->name, "After") == 0, "name changed");
    acta_db_model_folder_free(f);
}

static void test_rename_soft_deleted(void)
{
    int id;
    make_folder("ToDelete", 0, &id);
    acta_db_model_folder_soft_delete(g_db, id);

    int rc = acta_db_model_folder_rename(g_db, id, "Ghost");
    T_ASSERT(rc == ACTA_DB_OK, "rename on deleted still OK (no-op)");

    model_folder_t *f = acta_db_model_folder_get(g_db, id, NULL);
    T_ASSERT(f && strcmp(f->name, "ToDelete") == 0, "name unchanged");
    acta_db_model_folder_free(f);
}

static void test_rename_null_name(void)
{
    int id;
    make_folder("X", 0, &id);
    int rc = acta_db_model_folder_rename(g_db, id, NULL);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL name → INVALID");
}

static void test_rename_null_db(void)
{
    int rc = acta_db_model_folder_rename(NULL, 1, "x");
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_rename_invalid_id(void)
{
    int rc = acta_db_model_folder_rename(g_db, 0, "x");
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "id=0 → INVALID");
}

/* ── soft_delete / restore ───────────────────────────────────────── */

static void test_soft_delete_and_restore(void)
{
    int id;
    make_folder("Lifecycle", 0, &id);

    int rc = acta_db_model_folder_soft_delete(g_db, id);
    T_ASSERT(rc == ACTA_DB_OK, "soft_delete ok");

    model_folder_t *f = acta_db_model_folder_get(g_db, id, NULL);
    T_ASSERT(f->deleted_at != NULL, "deleted_at set");
    acta_db_model_folder_free(f);

    int count = -1;
    model_folder_t **items = acta_db_model_folder_list_all(g_db, 0, -1, &count, NULL);
    T_ASSERT(count == 0, "list_all empty after delete");
    acta_db_model_folder_list_free(items, count);

    rc = acta_db_model_folder_restore(g_db, id);
    T_ASSERT(rc == ACTA_DB_OK, "restore ok");

    f = acta_db_model_folder_get(g_db, id, NULL);
    T_ASSERT(f->deleted_at == NULL, "deleted_at cleared");
    acta_db_model_folder_free(f);

    items = acta_db_model_folder_list_all(g_db, 0, -1, &count, NULL);
    T_ASSERT(count == 1, "list_all has 1 after restore");
    acta_db_model_folder_list_free(items, count);
}

static void test_restore_nonexistent(void)
{
    int rc = acta_db_model_folder_restore(g_db, 99999);
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "restore missing → NOT_FOUND");
}

static void test_restore_null_db(void)
{
    int rc = acta_db_model_folder_restore(NULL, 1);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_restore_invalid_id(void)
{
    int rc = acta_db_model_folder_restore(g_db, 0);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "id=0 → INVALID");
}

static void test_restore_already_live(void)
{
    int id;
    make_folder("Live", 0, &id);

    int rc = acta_db_model_folder_restore(g_db, id);
    T_ASSERT(rc == ACTA_DB_OK, "restore on live folder → OK");

    int count = -1;
    model_folder_t **items = acta_db_model_folder_list_all(g_db, 0, -1, &count, NULL);
    T_ASSERT(count == 1, "still in list");
    acta_db_model_folder_list_free(items, count);
}

static void test_soft_delete_null_db(void)
{
    int rc = acta_db_model_folder_soft_delete(NULL, 1);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_soft_delete_invalid_id(void)
{
    int rc = acta_db_model_folder_soft_delete(g_db, 0);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "id=0 → INVALID");
}

/* ── list_children ───────────────────────────────────────────────── */

static void test_list_children_basic(void)
{
    int parent;
    make_folder("Root", 0, &parent);
    make_folder("A", parent, NULL);
    make_folder("B", parent, NULL);
    make_folder("C", parent, NULL);

    int count = -1, err;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 3, "3 children");
    T_ASSERT(items != NULL, "array non-null");
    T_ASSERT(strcmp(items[0]->name, "A") == 0, "order A");
    T_ASSERT(strcmp(items[1]->name, "B") == 0, "order B");
    T_ASSERT(strcmp(items[2]->name, "C") == 0, "order C");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_children_root_level(void)
{
    /* parent_id == 0 → root-level folders */
    make_folder("RootA", 0, NULL);
    make_folder("RootB", 0, NULL);
    int inner;
    make_folder("Inner", 0, &inner);
    make_folder("ChildOfInner", inner, NULL);

    int count = -1, err;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, 0, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 3, "3 root-level (not the nested child)");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_children_pagination(void)
{
    int parent;
    make_folder("Root", 0, &parent);
    for (int i = 0; i < 10; i++) {
        char name[16];
        snprintf(name, sizeof(name), "Child%02d", i);
        make_folder(name, parent, NULL);
    }

    int count = -1, err;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, 4, &count, &err);
    T_ASSERT(count == 4, "page1 has 4");
    T_ASSERT(strcmp(items[0]->name, "Child00") == 0, "first is Child00");
    acta_db_model_folder_list_free(items, count);

    items = acta_db_model_folder_list_children(g_db, parent, 4, 4, &count, &err);
    T_ASSERT(count == 4, "page2 has 4");
    T_ASSERT(strcmp(items[0]->name, "Child04") == 0, "first is Child04");
    acta_db_model_folder_list_free(items, count);

    items = acta_db_model_folder_list_children(g_db, parent, 8, 4, &count, &err);
    T_ASSERT(count == 2, "page3 has 2");
    acta_db_model_folder_list_free(items, count);

    items = acta_db_model_folder_list_children(g_db, parent, 10, 4, &count, &err);
    T_ASSERT(count == 0, "page4 empty");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_children_excludes_deleted(void)
{
    int parent, child;
    make_folder("Root", 0, &parent);
    make_folder("Live", parent, NULL);
    make_folder("Dead", parent, &child);
    acta_db_model_folder_soft_delete(g_db, child);

    int count = -1;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, -1, &count, NULL);
    T_ASSERT(count == 1, "only live child");
    T_ASSERT(strcmp(items[0]->name, "Live") == 0, "it's the live one");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_children_null_db(void)
{
    int count, err;
    model_folder_t **items =
        acta_db_model_folder_list_children(NULL, 0, 0, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_list_children_negative_offset(void)
{
    int count, err;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, 0, -1, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "negative offset → INVALID");
}

static void test_list_children_null_out_count(void)
{
    int id;
    make_folder("Root", 0, &id);
    make_folder("Kid", id, NULL);

    int err;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, id, 0, -1, NULL, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok with NULL out_count");
    acta_db_model_folder_list_free(items, 1);
}

/* ── list_all ────────────────────────────────────────────────────── */

static void test_list_all_basic(void)
{
    make_folder("Zebra", 0, NULL);
    make_folder("Apple", 0, NULL);
    make_folder("Mango", 0, NULL);

    int count = -1, err;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count, &err);
    T_ASSERT(count == 3, "3 items");
    T_ASSERT(strcmp(items[0]->name, "Apple") == 0, "alpha order");
    T_ASSERT(strcmp(items[2]->name, "Zebra") == 0, "alpha order");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_all_empty(void)
{
    int count = -1, err;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 0, "zero rows");
    T_ASSERT(items == NULL, "NULL array for empty");
    acta_db_model_folder_list_free(items, 0);
}

static void test_list_all_excludes_deleted(void)
{
    int id1, id2;
    make_folder("Live", 0, &id1);
    make_folder("Dead", 0, &id2);
    acta_db_model_folder_soft_delete(g_db, id2);

    int count = -1;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count, NULL);
    T_ASSERT(count == 1, "only live");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_all_null_db(void)
{
    int count, err;
    model_folder_t **items =
        acta_db_model_folder_list_all(NULL, 0, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_list_all_negative_offset(void)
{
    int count, err;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, -1, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "negative offset → INVALID");
}

static void test_list_all_pagination(void)
{
    for (int i = 0; i < 6; i++) {
        char name[8];
        snprintf(name, sizeof(name), "F%02d", i);
        make_folder(name, 0, NULL);
    }

    int count = -1, err;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, 2, &count, &err);
    T_ASSERT(count == 2, "page1: 2 items");
    T_ASSERT(strcmp(items[0]->name, "F00") == 0, "F00");
    acta_db_model_folder_list_free(items, count);

    items = acta_db_model_folder_list_all(g_db, 2, 2, &count, &err);
    T_ASSERT(count == 2, "page2: 2 items");
    T_ASSERT(strcmp(items[0]->name, "F02") == 0, "F02");
    acta_db_model_folder_list_free(items, count);

    items = acta_db_model_folder_list_all(g_db, 4, 2, &count, &err);
    T_ASSERT(count == 2, "page3: 2 items");
    acta_db_model_folder_list_free(items, count);

    items = acta_db_model_folder_list_all(g_db, 6, 2, &count, &err);
    T_ASSERT(count == 0, "page4: exhausted");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_all_null_out_count(void)
{
    make_folder("X", 0, NULL);
    int err;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, NULL, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    acta_db_model_folder_list_free(items, 1);
}

static void test_list_all_null_err(void)
{
    make_folder("X", 0, NULL);
    int count;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count, NULL);
    T_ASSERT(count == 1, "ok");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_all_both_null(void)
{
    make_folder("X", 0, NULL);
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, NULL, NULL);
    acta_db_model_folder_list_free(items, 1);
    g_pass++;
}

/* ── count_all ───────────────────────────────────────────────────── */

static void test_count_all_basic(void)
{
    make_folder("A", 0, NULL);
    make_folder("B", 0, NULL);
    make_folder("C", 0, NULL);

    int err;
    int total = acta_db_model_folder_count_all(g_db, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(total == 3, "count is 3");
}

static void test_count_all_empty(void)
{
    int total = acta_db_model_folder_count_all(g_db, NULL);
    T_ASSERT(total == 0, "zero rows → 0");
}

static void test_count_all_excludes_deleted(void)
{
    int id;
    make_folder("Live", 0, NULL);
    make_folder("Dead", 0, &id);
    acta_db_model_folder_soft_delete(g_db, id);

    int total = acta_db_model_folder_count_all(g_db, NULL);
    T_ASSERT(total == 1, "only live counted");
}

static void test_count_all_null_db(void)
{
    int err;
    int n = acta_db_model_folder_count_all(NULL, &err);
    T_ASSERT(n == -1, "returns -1");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_count_all_null_err(void)
{
    make_folder("A", 0, NULL);
    int n = acta_db_model_folder_count_all(g_db, NULL);
    T_ASSERT(n == 1, "works with NULL err");
}

/* ── count_children ──────────────────────────────────────────────── */

static void test_count_children_basic(void)
{
    int parent;
    make_folder("Parent", 0, &parent);
    make_folder("C1", parent, NULL);
    make_folder("C2", parent, NULL);
    make_folder("Other", 0, NULL);

    int err;
    int n = acta_db_model_folder_count_children(g_db, parent, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(n == 2, "2 children");
}

static void test_count_children_empty(void)
{
    int parent;
    make_folder("Lonely", 0, &parent);

    int n = acta_db_model_folder_count_children(g_db, parent, NULL);
    T_ASSERT(n == 0, "no children → 0");
}

static void test_count_children_nonexistent_parent(void)
{
    make_folder("A", 0, NULL);

    int n = acta_db_model_folder_count_children(g_db, 99999, NULL);
    T_ASSERT(n == 0, "missing parent → 0");
}

static void test_count_children_excludes_deleted(void)
{
    int parent, dead;
    make_folder("P", 0, &parent);
    make_folder("Live", parent, NULL);
    make_folder("Dead", parent, &dead);
    acta_db_model_folder_soft_delete(g_db, dead);

    int n = acta_db_model_folder_count_children(g_db, parent, NULL);
    T_ASSERT(n == 1, "only live child counted");
}

static void test_count_children_root_level(void)
{
    make_folder("R1", 0, NULL);
    make_folder("R2", 0, NULL);
    int inner;
    make_folder("Inner", 0, &inner);
    make_folder("Nested", inner, NULL);

    int n = acta_db_model_folder_count_children(g_db, 0, NULL);
    T_ASSERT(n == 3, "3 root-level, not the nested one");

    /* And the nested one is counted under its real parent: */
    int nested = acta_db_model_folder_count_children(g_db, inner, NULL);
    T_ASSERT(nested == 1, "Inner has exactly 1 child");
}


static void test_count_children_null_db(void)
{
    int err;
    int n = acta_db_model_folder_count_children(NULL, 1, &err);
    T_ASSERT(n == -1, "returns -1");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_count_children_null_err(void)
{
    int parent;
    make_folder("P", 0, &parent);
    make_folder("C", parent, NULL);

    int n = acta_db_model_folder_count_children(g_db, parent, NULL);
    T_ASSERT(n == 1, "works with NULL err");
}

/* ── free ────────────────────────────────────────────────────────── */

static void test_free_null(void)
{
    acta_db_model_folder_free(NULL);
    g_pass++;
}

static void test_list_free_null(void)
{
    acta_db_model_folder_list_free(NULL, 0);
    g_pass++;
}

/* ── integration ─────────────────────────────────────────────────── */

static void test_count_all_matches_lister(void)
{
    int parent;
    make_folder("P", 0, &parent);
    for (int i = 0; i < 7; i++) {
        char name[8];
        snprintf(name, sizeof(name), "F%d", i);
        make_folder(name, parent, NULL);
    }

    int count_total = acta_db_model_folder_count_all(g_db, NULL);
    int count_child = acta_db_model_folder_count_children(g_db, parent, NULL);

    int list_count, err;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, -1, &list_count, &err);

    T_ASSERT(count_child == list_count, "count_children == lister count");
    T_ASSERT(count_total == count_child + 1, "total = children + parent");
    acta_db_model_folder_list_free(items, list_count);
}

static void test_count_all_consistent_with_list_all(void)
{
    make_folder("A", 0, NULL);
    make_folder("B", 0, NULL);
    make_folder("C", 0, NULL);
    make_folder("D", 0, NULL);

    int count_fn  = acta_db_model_folder_count_all(g_db, NULL);
    int count_lst = -1;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count_lst, NULL);

    T_ASSERT(count_fn == count_lst, "count_all == list_all count");
    acta_db_model_folder_list_free(items, count_lst);
}



/* ================================================================== */
/*  Runner                                                            */
/* ================================================================== */


int run_model_folder_tests(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        /* create */
        {"create_basic",                test_create_basic},
        {"create_child",                test_create_child},
        {"create_null_name",            test_create_null_name},
        {"create_null_db",              test_create_null_db},
        {"create_null_out_id",          test_create_null_out_id},

        /* get */
        {"get_found",                   test_get_found},
        {"get_not_found",               test_get_not_found},
        {"get_null_db",                 test_get_null_db},
        {"get_invalid_id",              test_get_invalid_id},
        {"get_null_err",                test_get_null_err},

        /* rename */
        {"rename",                      test_rename},
        {"rename_soft_deleted",         test_rename_soft_deleted},
        {"rename_null_name",            test_rename_null_name},
        {"rename_null_db",              test_rename_null_db},
        {"rename_invalid_id",           test_rename_invalid_id},

        /* soft_delete / restore */
        {"soft_delete_null_db",         test_soft_delete_null_db},
        {"soft_delete_invalid_id",      test_soft_delete_invalid_id},
        {"soft_delete_and_restore",     test_soft_delete_and_restore},
        {"restore_nonexistent",         test_restore_nonexistent},
        {"restore_null_db",             test_restore_null_db},
        {"restore_invalid_id",          test_restore_invalid_id},
        {"restore_already_live",        test_restore_already_live},

        /* list_children */
        {"list_children_basic",         test_list_children_basic},
        {"list_children_root_level",    test_list_children_root_level},
        {"list_children_pagination",    test_list_children_pagination},
        {"list_children_excl_deleted",  test_list_children_excludes_deleted},
        {"list_children_null_db",       test_list_children_null_db},
        {"list_children_neg_offset",    test_list_children_negative_offset},
        {"list_children_null_outcount", test_list_children_null_out_count},

        /* list_all */
        {"list_all_basic",              test_list_all_basic},
        {"list_all_pagination",         test_list_all_pagination},
        {"list_all_empty",              test_list_all_empty},
        {"list_all_excl_deleted",       test_list_all_excludes_deleted},
        {"list_all_null_db",            test_list_all_null_db},
        {"list_all_neg_offset",         test_list_all_negative_offset},
        {"list_all_null_outcount",      test_list_all_null_out_count},
        {"list_all_null_err",           test_list_all_null_err},
        {"list_all_both_null",          test_list_all_both_null},

        /* count_all */
        {"count_all_basic",             test_count_all_basic},
        {"count_all_empty",             test_count_all_empty},
        {"count_all_excl_deleted",      test_count_all_excludes_deleted},
        {"count_all_null_db",           test_count_all_null_db},
        {"count_all_null_err",          test_count_all_null_err},

        /* count_children */
        {"count_children_basic",        test_count_children_basic},
        {"count_children_empty",        test_count_children_empty},
        {"count_children_nonexist",     test_count_children_nonexistent_parent},
        {"count_children_excl_deleted", test_count_children_excludes_deleted},
        {"count_children_root_level",   test_count_children_root_level},
        {"count_children_null_db",      test_count_children_null_db},
        {"count_children_null_err",     test_count_children_null_err},

        /* free */
        {"free_null",                   test_free_null},
        {"list_free_null",              test_list_free_null},

        /* integration */
        {"count_all_matches_lister",    test_count_all_matches_lister},
        {"count_all_consistent_listall", test_count_all_consistent_with_list_all},
    };

    int total = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < total; i++) {
        int fail_before = g_fail;

        setup();
        tests[i].fn();
        teardown();

        if (g_fail > fail_before)
            printf("[FAIL] %s\n", tests[i].name);
        else
            printf("[ ok ] %s\n", tests[i].name);
    }

    printf("\n  model_folder: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
