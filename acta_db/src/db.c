#include "internal.h"
#include "db.h"

db_t *acta_db_open(const char *path) {
    if (!path) return NULL;

    sqlite3 *handle;
    if (sqlite3_open(path, &handle) != SQLITE_OK) {
        sqlite3_close(handle);
        return NULL;
    }
    /* Enable WAL and foreign keys */
    sqlite3_exec(handle, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(handle, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    db_t *db = malloc(sizeof(db_t));
    if (!db) {
        sqlite3_close(handle);
        return NULL;
    }
    db->handle = handle;
    db->last_error = NULL;
    return db;
}


void acta_db_close(db_t *db) {
    if (!db) return;
    sqlite3_close(db->handle);
    free(db);
}

int acta_db_exec(db_t *db, const char *sql) {
    if (!db || !sql) return -1;
    free(db->last_error);
    db->last_error = NULL;

    char *err = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        db->last_error = err;   /* take ownership */
        return -1;
    }
    return 0;
}


const char *acta_db_last_error(db_t *db) {
    if (!db) return NULL;
    return db->last_error;
}


int acta_db_transaction(db_t *db, int (*fn)(db_t *, void *), void *user_data) {
    if (!db || !fn) return -1;
    if (sqlite3_exec(db->handle, "BEGIN;", NULL, NULL, NULL) != SQLITE_OK) return -1;

    int result = fn(db, user_data);

    if (result == 0) {
        sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
    } else {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
    }
    return result;
}
