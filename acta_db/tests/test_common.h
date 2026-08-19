#ifndef ACTA_DB_TEST_COMMON_H
#define ACTA_DB_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "acta_db.h"

static int test_count = 0;
static int test_failures = 0;

#define TEST_ASSERT(expr) do { \
    test_count++; \
    if (!(expr)) { \
        test_failures++; \
        fprintf(stderr, "  ASSERT FAILED: %s (line %d)\n", #expr, __LINE__); \
    } \
} while (0)

#define TEST_ASSERT_EQ_INT(actual, expected) do { \
    test_count++; \
    long _a = (long)(actual); \
    long _e = (long)(expected); \
    if (_a != _e) { \
        test_failures++; \
        fprintf(stderr, "  ASSERT FAILED: %s == %s -> %ld != %ld (line %d)\n", \
                #actual, #expected, _a, _e, __LINE__); \
    } \
} while (0)

#define TEST_ASSERT_EQ_STR(actual, expected) do { \
    test_count++; \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if ((_a == NULL) != (_e == NULL) || (_a && _e && strcmp(_a, _e) != 0)) { \
        test_failures++; \
        fprintf(stderr, "  ASSERT FAILED: %s == %s -> \"%s\" != \"%s\" (line %d)\n", \
                #actual, #expected, _a ? _a : "(null)", _e ? _e : "(null)", __LINE__); \
    } \
} while (0)

#define TEST_ASSERT_NULL(ptr) do { \
    test_count++; \
    if ((ptr) != NULL) { \
        test_failures++; \
        fprintf(stderr, "  ASSERT FAILED: %s == NULL (line %d)\n", #ptr, __LINE__); \
    } \
} while (0)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
    test_count++; \
    if ((ptr) == NULL) { \
        test_failures++; \
        fprintf(stderr, "  ASSERT FAILED: %s != NULL (line %d)\n", #ptr, __LINE__); \
    } \
} while (0)

/* Helper: create a temp db path, open, exec schema, return handle. Caller must close. */
static db_t *test_db_open(const char *path) {
    int err = 0;
    db_t *db = acta_db_open(path, &err);
    if (!db) return NULL;
    /* Schema is expected to be applied at open time by the library.
     * If not, we'd exec it here. Adjust as needed. */

    FILE *f = fopen("../acta_gamma/db/schema.sql", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *sql = malloc(len + 1);
        if (sql) {
            fread(sql, 1, len, f);
            sql[len] = '\0';
            acta_db_exec(db, sql);
            free(sql);
        }
        fclose(f);
    }

    return db;
}




static void test_db_teardown(db_t *db, const char *path) {
    acta_db_close(db);
    remove(path);
}

#endif /* ACTA_DB_TEST_COMMON_H */
