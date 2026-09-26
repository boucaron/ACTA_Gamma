/* test_busy.c — busy_timeout + wrapped SQLITE_BUSY
 *
 * Pins the busy_timeout contract: every
 * acta_db_open sets sqlite3_busy_timeout(ACTA_DB_BUSY_TIMEOUT_MS =
 * 5000), so a transient write contention (a WAL checkpoint, a
 * concurrent `db backup`) is absorbed by waiting rather than failing
 * with an immediate raw SQLITE_BUSY.
 *
 *   A. A second connection holds the write lock (BEGIN IMMEDIATE) for
 *      ~2 s — inside the budget: the first connection's write succeeds
 *      after actually waiting (elapsed in the ~2 s band; without the
 *      timeout it would have failed at once with SQLITE_BUSY).
 *   B. The second connection holds the lock for ~6 s — past the budget:
 *      the write fails after ~5 s with ACTA_DB_ERR_SQL and
 *      acta_db_last_error() carries the wrapped single-writer
 *      diagnostic, not the raw "database is locked" text.
 *
 * Skip-guard: if the second connection cannot take the write lock
 * deterministically (open or BEGIN IMMEDIATE fails — e.g. an exotic
 * filesystem), the suite skips instead of flaking.
 */

#include "test_common.h"

#include <sqlite3.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

/* ── timing helpers ───────────────────────────────────────────────── */

static long long now_ms(void)
{
#ifdef _WIN32
    return (long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
#endif
}

static void sleep_ms(long ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* ── the writer under test (runs on its own thread) ───────────────── */

static db_t *g_db;
static int g_rc;
static long long g_elapsed_ms;

static void *writer_thread(void *arg)
{
    (void)arg;
    long long t0 = now_ms();
    g_rc = acta_db_exec(g_db,
                        "INSERT INTO skill_folders(name) VALUES ('busy')");
    g_elapsed_ms = now_ms() - t0;
    return NULL;
}

int run_busy_tests(void)
{
    const char *path = "test/acta_test_busy.db";

    remove(path);
    remove("test/acta_test_busy.db-wal");
    remove("test/acta_test_busy.db-shm");

    db_t *db = test_db_open(path);
    if (!db) {
        fprintf(stderr, "SKIP: cannot open the busy test db\n");
        return 0;
    }
    g_db = db;

    /* The lock holder: a second raw connection on the same file.
     * BEGIN IMMEDIATE takes the write lock immediately and holds it
     * until COMMIT. */
    sqlite3 *h2 = NULL;
    int skip = 0;
    if (sqlite3_open(path, &h2) != SQLITE_OK) {
        skip = 1;
    } else {
        char *e2 = NULL;
        int rc2 = sqlite3_exec(h2, "BEGIN IMMEDIATE;", NULL, NULL, &e2);
        sqlite3_free(e2);
        if (rc2 != SQLITE_OK)
            skip = 1;
    }
    if (skip) {
        fprintf(stderr,
                "SKIP: cannot hold the write lock deterministically here\n");
        sqlite3_close(h2);
        acta_db_close(db);
        remove(path);
        remove("test/acta_test_busy.db-wal");
        remove("test/acta_test_busy.db-shm");
        return 0;
    }

    /* A. Contention inside the budget (~2 s hold < 5 s): the write
     *    succeeds after waiting — proves the busy_timeout is in effect. */
    {
        pthread_t th;
        if (pthread_create(&th, NULL, writer_thread, NULL) != 0) {
            fprintf(stderr, "SKIP: cannot start the writer thread\n");
            sqlite3_exec(h2, "COMMIT;", NULL, NULL, NULL);
        } else {
            sleep_ms(2000);
            sqlite3_exec(h2, "COMMIT;", NULL, NULL, NULL);
            pthread_join(th, NULL);
            TEST_ASSERT(g_rc == ACTA_DB_OK);
            TEST_ASSERT(g_elapsed_ms >= 1500);   /* actually waited */
            TEST_ASSERT(g_elapsed_ms < 4800);    /* well inside the budget */
        }
    }

    /* B. Contention past the budget (~6 s hold > 5 s): the write fails
     *    after ~5 s with the wrapped single-writer diagnostic. */
    {
        char *e2 = NULL;
        int rc2 = sqlite3_exec(h2, "BEGIN IMMEDIATE;", NULL, NULL, &e2);
        sqlite3_free(e2);
        if (rc2 != SQLITE_OK) {
            fprintf(stderr, "SKIP: cannot re-take the write lock\n");
        } else {
            pthread_t th;
            if (pthread_create(&th, NULL, writer_thread, NULL) != 0) {
                fprintf(stderr, "SKIP: cannot start the writer thread\n");
                sqlite3_exec(h2, "COMMIT;", NULL, NULL, NULL);
            } else {
                sleep_ms(6000);
                sqlite3_exec(h2, "COMMIT;", NULL, NULL, NULL);
                pthread_join(th, NULL);
                TEST_ASSERT(g_rc == ACTA_DB_ERR_SQL);
                TEST_ASSERT(g_elapsed_ms >= 4500);   /* waited the budget */
                const char *le = acta_db_last_error(db);
                TEST_ASSERT(le != NULL &&
                            strstr(le, "SQLITE_BUSY: concurrent write")
                                != NULL);
                TEST_ASSERT(le != NULL &&
                            strstr(le, "single-writer by design") != NULL);
            }
        }
    }

    sqlite3_close(h2);
    acta_db_close(db);
    remove(path);
    remove("test/acta_test_busy.db-wal");
    remove("test/acta_test_busy.db-shm");

    if (test_failures == 0)
        printf("  busy: all checks passed\n");
    return test_failures;
}
