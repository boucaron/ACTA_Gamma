/* test_model_folder.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"
#include "model_folder.h"
#include "model.h"

/* ── Minimal test harness ────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define T_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d): %s\n", \
                __func__, __LINE__, msg); \
        g_fail++; \
        return; \
    } \
    g_pass++; \
} while (0)

/* Sentinel value to detect *err not being written. */
#define ERR_SENTINEL (-999)

/* ── Fixture ─────────────────────────────────────────────────────── */

static db_t *g_db = NULL;

static void setup(void)
{
    int err = ERR_SENTINEL;
    g_db = acta_db_open(":memory:", &err, ACTA_DB_OPEN_CREATE);
    if (!g_db) {
        fprintf(stderr, "  FATAL: cannot open in-memory db (err=%d)\n", err);
        return;
    }

    int rc = acta_db_exec(g_db,
        "CREATE TABLE model_folders ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT NOT NULL,"
        "  parent_id  INTEGER REFERENCES model_folders(id),"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now')),"
        "  deleted_at TEXT NULL"
        ");");
    if (rc != ACTA_DB_OK) {
        fprintf(stderr, "  FATAL: CREATE TABLE model_folders failed (%s)\n",
                acta_db_strerror(rc));
        acta_db_close(g_db);
        g_db = NULL;
        return;
    }

    /* The partial unique indexes from the production schema
     * (acta_gamma/db/schema.sql). Duplicate detection in
     * acta_db_model_folder_create/rename relies on these: the C layer
     * only maps SQLITE_CONSTRAINT_UNIQUE to ACTA_DB_ERR_DUPLICATE. */
    rc = acta_db_exec(g_db,
        "CREATE UNIQUE INDEX uq_model_folders_root "
        "ON model_folders(name) WHERE parent_id IS NULL;");
    if (rc != ACTA_DB_OK) {
        fprintf(stderr, "  FATAL: CREATE UNIQUE INDEX uq_model_folders_root "
                "failed (%s)\n", acta_db_strerror(rc));
        acta_db_close(g_db);
        g_db = NULL;
        return;
    }
    rc = acta_db_exec(g_db,
        "CREATE UNIQUE INDEX uq_model_folders_child "
        "ON model_folders(parent_id, name) WHERE parent_id IS NOT NULL;");
    if (rc != ACTA_DB_OK) {
        fprintf(stderr, "  FATAL: CREATE UNIQUE INDEX uq_model_folders_child "
                "failed (%s)\n", acta_db_strerror(rc));
        acta_db_close(g_db);
        g_db = NULL;
        return;
    }

    /* Needed by soft_delete's contained-models guard. */
    rc = acta_db_exec(g_db,
        "CREATE TABLE models ("
        "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  folder_id        INTEGER REFERENCES model_folders(id),"
        "  name             TEXT NOT NULL,"
        "  description      TEXT,"
        "  backend          TEXT NOT NULL,"
        "  base_url         TEXT,"
        "  model_identifier TEXT NOT NULL,"
        "  configuration    TEXT,"
        "  created_at       TEXT DEFAULT (datetime('now')),"
        "  updated_at       TEXT DEFAULT (datetime('now')),"
        "  deleted_at       TEXT NULL"
        ");");
    if (rc != ACTA_DB_OK) {
        fprintf(stderr, "  FATAL: CREATE TABLE failed (%s)\n",
                acta_db_strerror(rc));
        acta_db_close(g_db);
        g_db = NULL;
    }
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

static int make_model(const char *name, int folder_id, int *out_id)
{
    model_t m;
    m.id = 0;
    m.folder_id = folder_id;
    m.name = (char *)name;
    m.description = (char *)"";
    m.backend = (char *)"openai";
    m.base_url = NULL;
    m.model_identifier = (char *)"gpt-test";
    m.configuration = NULL;
    m.created_at = NULL;
    m.updated_at = NULL;
    m.deleted_at = NULL;
    return acta_db_model_create(g_db, &m, out_id);
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

static void test_create_empty_name(void)
{
    int id;
    int rc = acta_db_model_folder_create(g_db, "", 0, &id);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "empty name → INVALID");
}

static void test_create_null_out_id(void)
{
    int rc = acta_db_model_folder_create(g_db, "NoOutId", 0, NULL);
    T_ASSERT(rc == ACTA_DB_OK, "NULL out_id still succeeds");
}

static void test_create_duplicate_root(void)
{
    int id1, id2;
    T_ASSERT(make_folder("Dup", 0, &id1) == ACTA_DB_OK, "first create ok");
    int rc = acta_db_model_folder_create(g_db, "Dup", 0, &id2);
    T_ASSERT(rc == ACTA_DB_ERR_DUPLICATE, "duplicate root name → DUPLICATE");
}

static void test_create_duplicate_child(void)
{
    int parent, id1, id2;
    T_ASSERT(make_folder("Parent", 0, &parent) == ACTA_DB_OK, "parent ok");
    T_ASSERT(make_folder("Child", parent, &id1) == ACTA_DB_OK, "first child ok");
    int rc = acta_db_model_folder_create(g_db, "Child", parent, &id2);
    T_ASSERT(rc == ACTA_DB_ERR_DUPLICATE, "duplicate sibling name → DUPLICATE");
}

static void test_create_invalid_parent(void)
{
    int id;
    int rc = acta_db_model_folder_create(g_db, "Orphan", 999999, &id);
    T_ASSERT(rc == ACTA_DB_ERR_FK, "nonexistent parent → FK");
}

/* ── get ─────────────────────────────────────────────────────────── */

static void test_get_found(void)
{
    int id;
    make_folder("FindMe", 0, &id);

    int err = ERR_SENTINEL;
    model_folder_t *f = acta_db_model_folder_get(g_db, id, &err);
    T_ASSERT(f != NULL, "row found");
    T_ASSERT(err == ACTA_DB_OK, "err is OK");
    T_ASSERT(f->id == id, "id matches");
    T_ASSERT(f->name && strcmp(f->name, "FindMe") == 0, "name matches");
    T_ASSERT(f->parent_id == 0, "parent_id is 0 for root");
    T_ASSERT(f->created_at != NULL, "created_at populated by DEFAULT");
    T_ASSERT(f->deleted_at == NULL, "deleted_at is NULL for live row");
    acta_db_model_folder_free(f);
}

static void test_get_not_found(void)
{
    int err = ERR_SENTINEL;
    model_folder_t *f = acta_db_model_folder_get(g_db, 99999, &err);
    T_ASSERT(f == NULL, "not-found returns NULL");
    T_ASSERT(err == ACTA_DB_OK, "not-found err is OK (not an error)");
}

static void test_get_null_db(void)
{
    int err = ERR_SENTINEL;
    model_folder_t *f = acta_db_model_folder_get(NULL, 1, &err);
    T_ASSERT(f == NULL, "NULL db → NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_get_invalid_id(void)
{
    int err = ERR_SENTINEL;
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
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "rename on deleted → NOT_FOUND");
}

static void test_rename_not_found(void)
{
    int rc = acta_db_model_folder_rename(g_db, 99999, "x");
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "nonexistent id → NOT_FOUND");
}

static void test_rename_null_name(void)
{
    int id;
    make_folder("X", 0, &id);
    int rc = acta_db_model_folder_rename(g_db, id, NULL);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL name → INVALID");
}

static void test_rename_empty_name(void)
{
    int id;
    T_ASSERT(make_folder("Before", 0, &id) == ACTA_DB_OK, "setup");
    int rc = acta_db_model_folder_rename(g_db, id, "");
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "empty name → INVALID");
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

static void test_rename_duplicate_child(void)
{
    int parent, id_a, id_b;
    T_ASSERT(make_folder("Parent", 0, &parent) == ACTA_DB_OK, "parent ok");
    T_ASSERT(make_folder("SiblingA", parent, &id_a) == ACTA_DB_OK, "A ok");
    T_ASSERT(make_folder("SiblingB", parent, &id_b) == ACTA_DB_OK, "B ok");
    int rc = acta_db_model_folder_rename(g_db, id_b, "SiblingA");
    T_ASSERT(rc == ACTA_DB_ERR_DUPLICATE, "duplicate sibling name → DUPLICATE");
}

static void test_rename_duplicate_root(void)
{
    int id_a, id_b;
    T_ASSERT(make_folder("RootA", 0, &id_a) == ACTA_DB_OK, "A ok");
    T_ASSERT(make_folder("RootB", 0, &id_b) == ACTA_DB_OK, "B ok");
    int rc = acta_db_model_folder_rename(g_db, id_b, "RootA");
    T_ASSERT(rc == ACTA_DB_ERR_DUPLICATE, "duplicate root name → DUPLICATE");
}

static void test_rename_cross_scope_ok(void)
{
    int parent_a, parent_b, id_a;
    T_ASSERT(make_folder("ParentA", 0, &parent_a) == ACTA_DB_OK, "A ok");
    T_ASSERT(make_folder("ParentB", 0, &parent_b) == ACTA_DB_OK, "B ok");
    T_ASSERT(make_folder("NameX", parent_a, &id_a) == ACTA_DB_OK, "child ok");
    int rc = acta_db_model_folder_rename(g_db, id_a, "ParentB");
    T_ASSERT(rc == ACTA_DB_OK, "different scope → OK");
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
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count, NULL);
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

static void test_soft_delete_with_live_children(void)
{
    int parent, child;
    make_folder("Parent", 0, &parent);
    make_folder("Child", parent, &child);

    int rc = acta_db_model_folder_soft_delete(g_db, parent);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "delete with live children → INVALID");

    /* Parent should still be live. */
    model_folder_t *f = acta_db_model_folder_get(g_db, parent, NULL);
    T_ASSERT(f->deleted_at == NULL, "parent still live");
    acta_db_model_folder_free(f);
}

static void test_soft_delete_with_deleted_children_ok(void)
{
    int parent, child;
    make_folder("Parent", 0, &parent);
    make_folder("Child", parent, &child);
    acta_db_model_folder_soft_delete(g_db, child);

    int rc = acta_db_model_folder_soft_delete(g_db, parent);
    T_ASSERT(rc == ACTA_DB_OK, "delete ok when children are all deleted");
}

static void test_soft_delete_with_live_models(void)
{
    int parent, model_id;
    make_folder("Parent", 0, &parent);
    make_model("M1", parent, &model_id);

    int rc = acta_db_model_folder_soft_delete(g_db, parent);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "delete with live models → INVALID");

    /* Parent and model should both still be live. */
    model_folder_t *f = acta_db_model_folder_get(g_db, parent, NULL);
    T_ASSERT(f->deleted_at == NULL, "parent still live");
    acta_db_model_folder_free(f);

    model_t *m = acta_db_model_get_live(g_db, model_id, NULL);
    T_ASSERT(m != NULL, "model still live");
    acta_db_model_free(m);
}

static void test_soft_delete_with_deleted_models_ok(void)
{
    int parent, model_id;
    make_folder("Parent", 0, &parent);
    make_model("M1", parent, &model_id);
    acta_db_model_soft_delete(g_db, model_id);

    int rc = acta_db_model_folder_soft_delete(g_db, parent);
    T_ASSERT(rc == ACTA_DB_OK, "delete ok when models are all deleted");
}

static void test_soft_delete_double(void)
{
    int id;
    make_folder("Once", 0, &id);
    T_ASSERT(acta_db_model_folder_soft_delete(g_db, id) == ACTA_DB_OK, "first delete ok");

    int rc = acta_db_model_folder_soft_delete(g_db, id);
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "second delete → NOT_FOUND");
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
    T_ASSERT(rc == ACTA_DB_OK, "restore on live folder → OK (no-op)");
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

/* ── move_to ─────────────────────────────────────────────────────── */

static void test_move_to_basic(void)
{
    int p1, p2, child;
    make_folder("P1", 0, &p1);
    make_folder("P2", 0, &p2);
    make_folder("Child", p1, &child);

    int rc = acta_db_model_folder_move_to(g_db, child, p2);
    T_ASSERT(rc == ACTA_DB_OK, "move ok");

    model_folder_t *f = acta_db_model_folder_get(g_db, child, NULL);
    T_ASSERT(f != NULL, "row exists");
    T_ASSERT(f->parent_id == p2, "parent changed to p2");
    acta_db_model_folder_free(f);
}

static void test_move_to_root(void)
{
    int parent, child;
    make_folder("Parent", 0, &parent);
    make_folder("Child", parent, &child);

    int rc = acta_db_model_folder_move_to(g_db, child, 0);
    T_ASSERT(rc == ACTA_DB_OK, "move to root ok");

    model_folder_t *f = acta_db_model_folder_get(g_db, child, NULL);
    T_ASSERT(f->parent_id == 0, "parent is now root");
    acta_db_model_folder_free(f);
}

static void test_move_to_same_parent_noop(void)
{
    int parent, child;
    make_folder("Parent", 0, &parent);
    make_folder("Child", parent, &child);

    int rc = acta_db_model_folder_move_to(g_db, child, parent);
    T_ASSERT(rc == ACTA_DB_OK, "no-op returns OK");
}

static void test_move_to_self(void)
{
    int id;
    make_folder("Lonely", 0, &id);

    int rc = acta_db_model_folder_move_to(g_db, id, id);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "self-move → INVALID (cycle)");
}

static void test_move_to_own_child_cycle(void)
{
    int a, b;
    make_folder("A", 0, &a);
    make_folder("B", a, &b);

    int rc = acta_db_model_folder_move_to(g_db, a, b);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "move to own child → INVALID");
}

static void test_move_to_grandchild_cycle(void)
{
    int a, b, c;
    make_folder("A", 0, &a);
    make_folder("B", a, &b);
    make_folder("C", b, &c);

    int rc = acta_db_model_folder_move_to(g_db, a, c);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "move to grandchild → INVALID");
}

static void test_move_to_nonexistent_folder(void)
{
    int parent;
    make_folder("P", 0, &parent);
    int rc = acta_db_model_folder_move_to(g_db, 99999, parent);
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "missing source → NOT_FOUND");
}

static void test_move_to_nonexistent_target(void)
{
    int child;
    make_folder("Child", 0, &child);
    int rc = acta_db_model_folder_move_to(g_db, child, 99999);
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "missing target → NOT_FOUND");
}

static void test_move_soft_deleted_folder(void)
{
    int parent, target, id;
    make_folder("P", 0, &parent);
    make_folder("T", 0, &target);
    make_folder("Dead", parent, &id);
    acta_db_model_folder_soft_delete(g_db, id);

    int rc = acta_db_model_folder_move_to(g_db, id, target);
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "deleted source → NOT_FOUND");
}

static void test_move_to_soft_deleted_target(void)
{
    int parent, target, child;
    make_folder("P", 0, &parent);
    make_folder("T", 0, &target);
    make_folder("Child", parent, &child);
    acta_db_model_folder_soft_delete(g_db, target);

    int rc = acta_db_model_folder_move_to(g_db, child, target);
    T_ASSERT(rc == ACTA_DB_ERR_NOT_FOUND, "deleted target → NOT_FOUND");
}

static void test_move_null_db(void)
{
    int rc = acta_db_model_folder_move_to(NULL, 1, 0);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "NULL db → INVALID");
}

static void test_move_invalid_folder_id(void)
{
    int rc = acta_db_model_folder_move_to(g_db, 0, 0);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "folder_id=0 → INVALID");
}

static void test_move_invalid_negative_id(void)
{
    int rc = acta_db_model_folder_move_to(g_db, -1, 0);
    T_ASSERT(rc == ACTA_DB_ERR_INVALID, "negative id → INVALID");
}

static void test_move_updates_reflected_in_list(void)
{
    int p1, p2, child;
    make_folder("P1", 0, &p1);
    make_folder("P2", 0, &p2);
    make_folder("Child", p1, &child);

    acta_db_model_folder_move_to(g_db, child, p2);

    int count1 = -1;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, p1, 0, -1, &count1, NULL);
    T_ASSERT(count1 == 0, "p1 has no children after move");
    acta_db_model_folder_list_free(items, count1);

    int count2 = -1;
    items = acta_db_model_folder_list_children(g_db, p2, 0, -1, &count2, NULL);
    T_ASSERT(count2 == 1, "p2 has 1 child after move");
    T_ASSERT(items[0]->name && strcmp(items[0]->name, "Child") == 0,
             "it's the moved child");
    acta_db_model_folder_list_free(items, count2);
}

/* ── list_children ───────────────────────────────────────────────── */

static void test_list_children_basic(void)
{
    int parent;
    make_folder("Root", 0, &parent);
    make_folder("A", parent, NULL);
    make_folder("B", parent, NULL);
    make_folder("C", parent, NULL);

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 3, "3 children");
    T_ASSERT(items != NULL, "array non-null");
    T_ASSERT(strcmp(items[0]->name, "A") == 0, "order A (id 1)");
    T_ASSERT(strcmp(items[1]->name, "B") == 0, "order B (id 2)");
    T_ASSERT(strcmp(items[2]->name, "C") == 0, "order C (id 3)");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_children_empty(void)
{
    int parent;
    make_folder("Lonely", 0, &parent);

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 0, "zero children");
    T_ASSERT(items == NULL, "NULL array for empty");
    acta_db_model_folder_list_free(items, 0);
}

static void test_list_children_root_level(void)
{
    make_folder("RootA", 0, NULL);
    make_folder("RootB", 0, NULL);
    int inner;
    make_folder("Inner", 0, &inner);
    make_folder("ChildOfInner", inner, NULL);

    int count = -1, err = ERR_SENTINEL;
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

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, parent, 0, 4, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "page1 ok");
    T_ASSERT(count == 4, "page1 has 4");
    T_ASSERT(strcmp(items[0]->name, "Child00") == 0, "first is Child00");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
    items = acta_db_model_folder_list_children(g_db, parent, 4, 4, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "page2 ok");
    T_ASSERT(count == 4, "page2 has 4");
    T_ASSERT(strcmp(items[0]->name, "Child04") == 0, "first is Child04");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
    items = acta_db_model_folder_list_children(g_db, parent, 8, 4, &count, &err);
    T_ASSERT(count == 2, "page3 has 2");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
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
    int count, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_children(NULL, 0, 0, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_list_children_negative_offset(void)
{
    int count, err = ERR_SENTINEL;
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

    int err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_children(g_db, id, 0, -1, NULL, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok with NULL out_count");
    T_ASSERT(items != NULL, "items non-null");
    acta_db_model_folder_list_free(items, 1);
}

/* ── list_all ────────────────────────────────────────────────────── */

static void test_list_all_basic(void)
{
    /*
     * API orders by id ASC, not by name.
     * Creation order determines id order here.
     */
    make_folder("Zebra", 0, NULL);   /* id=1 */
    make_folder("Apple", 0, NULL);   /* id=2 */
    make_folder("Mango", 0, NULL);   /* id=3 */

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 3, "3 items");
    T_ASSERT(strcmp(items[0]->name, "Zebra") == 0, "id order: Zebra(1)");
    T_ASSERT(strcmp(items[1]->name, "Apple") == 0, "id order: Apple(2)");
    T_ASSERT(strcmp(items[2]->name, "Mango") == 0, "id order: Mango(3)");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_all_empty(void)
{
    int count = -1, err = ERR_SENTINEL;
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
    int count, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all(NULL, 0, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_list_all_negative_offset(void)
{
    int count, err = ERR_SENTINEL;
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

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, 2, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 2, "page1: 2 items");
    T_ASSERT(strcmp(items[0]->name, "F00") == 0, "F00");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
    items = acta_db_model_folder_list_all(g_db, 2, 2, &count, &err);
    T_ASSERT(count == 2, "page2: 2 items");
    T_ASSERT(strcmp(items[0]->name, "F02") == 0, "F02");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
    items = acta_db_model_folder_list_all(g_db, 4, 2, &count, &err);
    T_ASSERT(count == 2, "page3: 2 items");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
    items = acta_db_model_folder_list_all(g_db, 6, 2, &count, &err);
    T_ASSERT(count == 0, "page4: exhausted");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_all_null_out_count(void)
{
    make_folder("X", 0, NULL);
    int err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all(g_db, 0, -1, NULL, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(items != NULL, "items non-null");
    acta_db_model_folder_list_free(items, 1);
}

static void test_list_all_null_err(void)
{
    make_folder("X", 0, NULL);
    int count = -1;
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
    T_ASSERT(items != NULL, "items non-null");
    acta_db_model_folder_list_free(items, 1);
}

/* ── list_all_with_deleted ───────────────────────────────────────── */

static void test_list_allwd_includes_deleted(void)
{
    int id_live, id_deleted;
    T_ASSERT(make_folder("Keep", 0, &id_live) == ACTA_DB_OK, "live ok");
    T_ASSERT(make_folder("Gone", 0, &id_deleted) == ACTA_DB_OK, "del ok");
    T_ASSERT(acta_db_model_folder_soft_delete(g_db, id_deleted) ==
               ACTA_DB_OK,
             "soft_delete ok");

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 2, "live + deleted");
    for (int i = 0; i < count; i++) {
        if (items[i]->id == id_live) {
            T_ASSERT(strcmp(items[i]->name, "Keep") == 0, "name");
            T_ASSERT(items[i]->deleted_at == NULL, "live row");
        } else if (items[i]->id == id_deleted) {
            T_ASSERT(strcmp(items[i]->name, "Gone") == 0, "name");
            T_ASSERT(items[i]->deleted_at != NULL, "deleted row");
        } else {
            T_ASSERT(0, "unexpected row");
        }
    }
    acta_db_model_folder_list_free(items, count);
}

static void test_list_allwd_empty(void)
{
    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 0, "zero rows");
    T_ASSERT(items == NULL, "NULL array for empty");
    acta_db_model_folder_list_free(items, 0);
}

static void test_list_allwd_null_db(void)
{
    int count, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(NULL, 0, -1, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "INVALID");
}

static void test_list_allwd_neg_offset(void)
{
    int count, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, -5, 10, &count, &err);
    T_ASSERT(items == NULL, "NULL");
    T_ASSERT(err == ACTA_DB_ERR_INVALID, "negative offset → INVALID");
}

static void test_list_allwd_paged(void)
{
    const char *names[] = {"F1", "F2", "F3", "F4"};
    int ids[4] = {0};
    for (int i = 0; i < 4; i++)
        T_ASSERT(make_folder(names[i], 0, &ids[i]) == ACTA_DB_OK, "create");
    T_ASSERT(acta_db_model_folder_soft_delete(g_db, ids[1]) == ACTA_DB_OK,
             "delete F2");
    T_ASSERT(acta_db_model_folder_soft_delete(g_db, ids[3]) == ACTA_DB_OK,
             "delete F4");

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, 0, 2, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 2, "page1: 2 items");
    T_ASSERT(strcmp(items[0]->name, "F1") == 0, "F1");
    T_ASSERT(items[0]->deleted_at == NULL, "F1 live");
    T_ASSERT(strcmp(items[1]->name, "F2") == 0, "F2");
    T_ASSERT(items[1]->deleted_at != NULL, "F2 deleted");
    acta_db_model_folder_list_free(items, count);

    count = -1; err = ERR_SENTINEL;
    items = acta_db_model_folder_list_all_with_deleted(g_db, 2, 2, &count, &err);
    T_ASSERT(count == 2, "page2: 2 items");
    T_ASSERT(strcmp(items[0]->name, "F3") == 0, "F3");
    T_ASSERT(items[0]->deleted_at == NULL, "F3 live");
    T_ASSERT(strcmp(items[1]->name, "F4") == 0, "F4");
    T_ASSERT(items[1]->deleted_at != NULL, "F4 deleted");
    acta_db_model_folder_list_free(items, count);
}

static void test_list_allwd_nested(void)
{
    int parent_id, child_id;
    T_ASSERT(make_folder("Parent", 0, &parent_id) == ACTA_DB_OK, "parent");
    T_ASSERT(make_folder("Child", parent_id, &child_id) == ACTA_DB_OK,
             "child");
    T_ASSERT(acta_db_model_folder_soft_delete(g_db, child_id) == ACTA_DB_OK,
             "delete child");

    int count = -1, err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, 0, -1, &count, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(count == 2, "parent + child");
    for (int i = 0; i < count; i++) {
        if (items[i]->id == child_id) {
            /* Soft-deleted child still reports its (live) parent. */
            T_ASSERT(items[i]->parent_id == parent_id, "parent link kept");
            T_ASSERT(items[i]->deleted_at != NULL, "child deleted");
        } else if (items[i]->id == parent_id) {
            T_ASSERT(items[i]->parent_id == 0, "root");
            T_ASSERT(items[i]->deleted_at == NULL, "parent live");
        } else {
            T_ASSERT(0, "unexpected row");
        }
    }
    acta_db_model_folder_list_free(items, count);
}

static void test_list_allwd_null_outcount(void)
{
    make_folder("X", 0, NULL);
    int err = ERR_SENTINEL;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, 0, -1, NULL, &err);
    T_ASSERT(err == ACTA_DB_OK, "ok");
    T_ASSERT(items != NULL, "items non-null");
    acta_db_model_folder_list_free(items, 1);
}

static void test_list_allwd_null_err(void)
{
    make_folder("X", 0, NULL);
    int count = -1;
    model_folder_t **items =
        acta_db_model_folder_list_all_with_deleted(g_db, 0, -1, &count, NULL);
    T_ASSERT(count == 1, "ok");
    acta_db_model_folder_list_free(items, count);
}

/* ── count_all ───────────────────────────────────────────────────── */

static void test_count_all_basic(void)
{
    make_folder("A", 0, NULL);
    make_folder("B", 0, NULL);
    make_folder("C", 0, NULL);

    int err = ERR_SENTINEL;
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
    int err = ERR_SENTINEL;
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

    int err = ERR_SENTINEL;
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

    int nested = acta_db_model_folder_count_children(g_db, inner, NULL);
    T_ASSERT(nested == 1, "Inner has exactly 1 child");
}

static void test_count_children_null_db(void)
{
    int err = ERR_SENTINEL;
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

    int list_count = -1, err = ERR_SENTINEL;
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

static void test_move_then_soft_delete_parent(void)
{
    int parent, child, other;
    make_folder("Parent", 0, &parent);
    make_folder("Child", parent, &child);
    make_folder("Other", 0, &other);

    acta_db_model_folder_move_to(g_db, child, other);

    int rc = acta_db_model_folder_soft_delete(g_db, parent);
    T_ASSERT(rc == ACTA_DB_OK, "parent has no live children after move");
}

/* ================================================================== */
/*  Runner                                                            */
/* ================================================================== */

int run_model_folder_tests(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        /* create */
        {"create_basic",                 test_create_basic},
        {"create_child",                 test_create_child},
        {"create_null_name",             test_create_null_name},
        {"create_empty_name",            test_create_empty_name},
        {"create_null_db",               test_create_null_db},
        {"create_null_out_id",           test_create_null_out_id},
        {"create_duplicate_root",        test_create_duplicate_root},
        {"create_duplicate_child",       test_create_duplicate_child},
        {"create_invalid_parent",        test_create_invalid_parent},

        /* get */
        {"get_found",                    test_get_found},
        {"get_not_found",                test_get_not_found},
        {"get_null_db",                  test_get_null_db},
        {"get_invalid_id",               test_get_invalid_id},
        {"get_null_err",                 test_get_null_err},

        /* rename */
        {"rename",                       test_rename},
        {"rename_soft_deleted",          test_rename_soft_deleted},
        {"rename_not_found",             test_rename_not_found},
        {"rename_null_name",             test_rename_null_name},
        {"rename_empty_name",            test_rename_empty_name},
        {"rename_null_db",               test_rename_null_db},
        {"rename_invalid_id",            test_rename_invalid_id},
        {"rename_duplicate_child",       test_rename_duplicate_child},
        {"rename_duplicate_root",        test_rename_duplicate_root},
        {"rename_cross_scope_ok",        test_rename_cross_scope_ok},

        /* soft_delete / restore */
        {"soft_delete_null_db",          test_soft_delete_null_db},
        {"soft_delete_invalid_id",       test_soft_delete_invalid_id},
        {"soft_delete_and_restore",      test_soft_delete_and_restore},
        {"soft_delete_live_children",    test_soft_delete_with_live_children},
        {"soft_delete_deleted_children", test_soft_delete_with_deleted_children_ok},
        {"soft_delete_live_models",      test_soft_delete_with_live_models},
        {"soft_delete_deleted_models",   test_soft_delete_with_deleted_models_ok},
        {"soft_delete_double",           test_soft_delete_double},
        {"restore_nonexistent",          test_restore_nonexistent},
        {"restore_null_db",              test_restore_null_db},
        {"restore_invalid_id",           test_restore_invalid_id},
        {"restore_already_live",         test_restore_already_live},

        /* move_to */
        {"move_to_basic",                test_move_to_basic},
        {"move_to_root",                test_move_to_root},
        {"move_to_same_parent_noop",    test_move_to_same_parent_noop},
        {"move_to_self",               test_move_to_self},
        {"move_to_own_child_cycle",    test_move_to_own_child_cycle},
        {"move_to_grandchild_cycle",   test_move_to_grandchild_cycle},
        {"move_to_nonexistent_folder", test_move_to_nonexistent_folder},
        {"move_to_nonexistent_target", test_move_to_nonexistent_target},
        {"move_soft_deleted_folder",   test_move_soft_deleted_folder},
        {"move_to_soft_deleted_target",test_move_to_soft_deleted_target},
        {"move_null_db",               test_move_null_db},
        {"move_invalid_folder_id",     test_move_invalid_folder_id},
        {"move_invalid_negative_id",   test_move_invalid_negative_id},
        {"move_updates_list",          test_move_updates_reflected_in_list},
        {"move_then_soft_delete_parent",test_move_then_soft_delete_parent},

        /* list_children */
        {"list_children_basic",         test_list_children_basic},
        {"list_children_empty",         test_list_children_empty},
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

        /* list_all_with_deleted */
        {"list_allwd_includes_deleted", test_list_allwd_includes_deleted},
        {"list_allwd_empty",            test_list_allwd_empty},
        {"list_allwd_null_db",          test_list_allwd_null_db},
        {"list_allwd_neg_offset",       test_list_allwd_neg_offset},
        {"list_allwd_paged",            test_list_allwd_paged},
        {"list_allwd_nested",           test_list_allwd_nested},
        {"list_allwd_null_outcount",    test_list_allwd_null_outcount},
        {"list_allwd_null_err",         test_list_allwd_null_err},

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
        {"count_all_consistent_listall",test_count_all_consistent_with_list_all},
    };

    int total = (int)(sizeof(tests) / sizeof(tests[0]));

    for (int i = 0; i < total; i++) {
        int fail_before = g_fail;

        setup();
        if (!g_db) {
            printf("[SKIP] %s (setup failed)\n", tests[i].name);
            continue;
        }
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
