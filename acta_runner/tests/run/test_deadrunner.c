/*
 * test_deadrunner.c — end-to-end dead-runner recovery.
 *
 * The in-process unit suites (test_sweep.c etc.) cover the sweep logic
 * against a seeded `:memory:` DB. This suite covers the real failure
 * loop with real processes:
 *
 *   1. a real `acta_runner run <id>` child claims a pending execution
 *      against the in-process stub server (delay_ms long enough that it
 *      blocks on the /health preflight call, sitting in `running`);
 *   2. the child is SIGKILL'd (POSIX) / TerminateProcess'd (Windows)
 *      mid-run — the realistic "dead runner process";
 *   3. the row is stuck in `running` (nothing auto-recovered it);
 *   4. a second real `acta_runner sweep --stale-seconds N` child
 *      transitions it `running -> failed` with the exact stale error
 *      and an `execution_failed` log row;
 *   5. a repeat sweep is a clean no-op (exit 0, row unchanged).
 *
 * Cross-platform: fork/execv/SIGKILL + waitpid on POSIX,
 * CreateProcess/TerminateProcess + WaitForSingleObject on Windows.
 *
 * The scratch DB is a REAL FILE (tests/run/test_deadrunner.db) because
 * the child processes must share it; `:memory:` is impossible here.
 *
 * Stub delay choice: delay_ms must outlive the window between the
 * child's /health connect and the kill (so the child is killed while
 * blocked), and must finish before stub_server_stop() (so the thread
 * can be joined quickly). 4000 ms satisfies both with margin.
 *
 * Test duration: ~10-15 s (dominated by the stale-aging sleep). Kept
 * OUT of the default `test` target — run it with `make test-e2e`.
 *
 * Run from acta_runner/: `make test-e2e`
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "stub_server.h"
#include "acta_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
static void msleep(unsigned int ms) { Sleep(ms); }
#else
#  include <signal.h>
#  include <unistd.h>
#  include <sys/wait.h>
static void msleep(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

#define STUB_PORT 8918

static int checks = 0;
static int failures = 0;

static int check(int cond, const char *what)
{
    checks++;
    if (cond)
        printf("  PASS %s\n", what);
    else {
        printf("  FAIL %s\n", what);
        failures++;
    }
    return cond;
}

/* Path to the runner binary under test. `make test-e2e` builds it
 * first and runs from acta_runner/, so the relative name works;
 * override with $ACTA_RUNNER_BIN if the binary lives elsewhere. */
static const char *runner_bin(void)
{
    const char *env = getenv("ACTA_RUNNER_BIN");
    if (env && env[0])
        return env;
#ifdef _WIN32
    return "acta_runner.exe";
#else
    return "acta_runner";
#endif
}

/* ── process helpers ─────────────────────────────────────────────── */

typedef struct {
#ifdef _WIN32
    HANDLE process;
#else
    pid_t pid;
#endif
} child_t;

/* Spawn bin with args[0] = bin and a NULL-terminated flag list. */
static int spawn(const char *bin, char *const *args, child_t *out)
{
    memset(out, 0, sizeof *out);
#ifdef _WIN32
    char cmd[1024];
    int off = 0;
    off = snprintf(cmd + off, sizeof cmd - off, "\"%s\"", bin);
    for (char *const *a = args + 1; *a; a++)
        off += snprintf(cmd + off, sizeof cmd - off, " \"%s\"", *a);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL,
                        &si, &pi))
        return -1;
    out->process = pi.hProcess;
    CloseHandle(pi.hThread);
    return 0;
#else
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        char *argv[16];
        int i = 0;
        for (char *const *a = args; *a && i < 15; a++)
            argv[i++] = *a;
        argv[i] = NULL;
        execv(bin, argv);
        _exit(127);
    }
    out->pid = pid;
    return 0;
#endif
}

/* Kill the child hard (no cleanup handlers run, on either platform). */
static int kill_child(child_t *c)
{
#ifdef _WIN32
    return TerminateProcess(c->process, 0) ? 0 : -1;
#else
    return kill(c->pid, SIGKILL) == 0 ? 0 : -1;
#endif
}

/* Reap the child; *exit_code gets the process exit code (POSIX: -1 if
 * killed by signal). Returns 0 on success, -1 if it did not exit
 * within 10 s. */
static int wait_child(child_t *c, int *exit_code)
{
    *exit_code = -1;
#ifdef _WIN32
    DWORD rc = WaitForSingleObject(c->process, 10000);
    DWORD code = 0;
    (void)GetExitCodeProcess(c->process, &code);
    CloseHandle(c->process);
    *exit_code = (int)code;
    return rc == WAIT_OBJECT_0 ? 0 : -1;
#else
    int st = 0;
    if (waitpid(c->pid, &st, 0) < 0)
        return -1;
    if (WIFEXITED(st))
        *exit_code = WEXITSTATUS(st);
    return 0;
#endif
}

/* ── DB setup (same schema loading as test_sweep.c) ───────────────── */

static int load_schema(db_t *db)
{
    static const char *paths[] = {
        "../../acta_gui/db/schema.sql",
        "../acta_gui/db/schema.sql",
        "acta_gui/db/schema.sql",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *f = fopen(paths[i], "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *sql = (char *)malloc((size_t)len + 1);
        if (!sql) {
            fclose(f);
            return -1;
        }
        if (fread(sql, 1, (size_t)len, f) != (size_t)len) {
            free(sql);
            fclose(f);
            return -1;
        }
        sql[len] = '\0';
        fclose(f);
        int rc = acta_db_exec(db, sql);
        free(sql);
        return rc;
    }
    return -1;
}

/* Seed one PENDING execution (context + skill + model revisions)
 * pointed at the stub server. Returns the execution id or -1. */
static int seed_pending(db_t *db)
{
    int err = ACTA_DB_OK;

    context_t c;
    memset(&c, 0, sizeof c);
    c.type = "text";
    c.content = "CTX-CONTENT";
    c.content_hash = "test-hash";
    int ctx_id = 0;
    if (acta_db_context_create(db, &c, &ctx_id) != ACTA_DB_OK)
        return -1;

    skill_t s;
    memset(&s, 0, sizeof s);
    s.name = "deadrunner-skill";
    s.prompt_template = "SYS-TEMPLATE";
    int skill_id = 0;
    if (acta_db_skill_create(db, &s, &skill_id) != ACTA_DB_OK)
        return -1;

    skill_revision_t *sr =
        acta_db_skill_revision_get_latest(db, skill_id, &err);
    if (!sr)
        return -1;
    int skill_rev_id = sr->id;
    acta_db_skill_revision_free(sr);

    model_t m;
    memset(&m, 0, sizeof m);
    m.name = "deadrunner-model";
    m.backend = "llama";
    m.base_url = "http://127.0.0.1:8918";
    m.model_identifier = "stub-model";
    int model_id = 0;
    if (acta_db_model_create(db, &m, &model_id) != ACTA_DB_OK)
        return -1;

    model_revision_t *mr =
        acta_db_model_revision_get_latest(db, model_id, &err);
    if (!mr)
        return -1;
    int model_rev_id = mr->id;
    acta_db_model_revision_free(mr);

    execution_t e;
    memset(&e, 0, sizeof e);
    e.context_id = ctx_id;
    e.skill_revision_id = skill_rev_id;
    e.model_revision_id = model_rev_id;
    e.prompt = "USER-PROMPT";
    int id = 0;
    if (acta_db_execution_create(db, &e, &id) != ACTA_DB_OK)
        return -1;
    return id;
}

/* ── verification helpers ─────────────────────────────────────────── */

static int status_now(db_t *db, int id, char *out, size_t outsz)
{
    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, id, &err);
    if (err != ACTA_DB_OK || !e)
        return 0;
    snprintf(out, outsz, "%s", e->status ? e->status : "(null)");
    acta_db_execution_free(e);
    return 1;
}

static int error_contains(db_t *db, int id, const char *needle)
{
    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, id, &err);
    if (err != ACTA_DB_OK || !e)
        return 0;
    int found = e->error && e->error[0] && strstr(e->error, needle) != NULL;
    acta_db_execution_free(e);
    return found;
}

static int log_has_event(db_t *db, int exec_id, const char *event)
{
    int err = ACTA_DB_OK, n = 0;
    execution_log_t **rows = acta_db_execution_log_list_by_execution(
        db, exec_id, NULL, 0, ACTA_DB_MAX_PAGE, &n, &err);
    if (err != ACTA_DB_OK || !rows)
        return 0;
    int found = 0;
    for (int i = 0; i < n; i++)
        if (rows[i]->event && strcmp(rows[i]->event, event) == 0) {
            found = 1;
            break;
        }
    acta_db_execution_log_list_free(rows, n);
    return found;
}

/* Remove a file and its WAL/SHM companions if present. */
static void remove_db_files(const char *path)
{
    char p[256];
    remove(path);
    snprintf(p, sizeof p, "%s-wal", path);
    remove(p);
    snprintf(p, sizeof p, "%s-shm", path);
    remove(p);
}

/* Poll the file DB (WAL: fresh statements see the child's commits)
 * until the execution's status equals `want`, or timeout_ms elapses.
 * On timeout, fills `got` with whatever status was last seen. */
static int wait_status(db_t *db, int id, const char *want, int timeout_ms,
                       char *got, size_t gotsz)
{
    for (int waited = 0; waited < timeout_ms; waited += 200) {
        if (status_now(db, id, got, gotsz) && strcmp(got, want) == 0)
            return 1;
        msleep(200);
    }
    status_now(db, id, got, gotsz);
    return 0;
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(void)
{
    const char *db_path = "tests/run/test_deadrunner.db";
    const int stale_seconds = 5;

    /* Fresh scratch file DB (removed from any previous run). */
    remove_db_files(db_path);

    int err = ACTA_DB_OK;
    db_t *db = acta_db_open(db_path, &err, ACTA_DB_OPEN_CREATE);
    if (!db) {
        fprintf(stderr, "cannot open scratch db %s: %s\n",
                db_path, acta_db_strerror(err));
        return 1;
    }
    if (load_schema(db) != ACTA_DB_OK) {
        fprintf(stderr, "cannot load schema: %s\n",
                acta_db_last_error(db) ? acta_db_last_error(db) : "unknown");
        acta_db_close(db);
        return 1;
    }

    /* Hanging stub: every response is delayed long enough that the
     * child blocks on the /health preflight call while in `running`. */
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.port = STUB_PORT;
    cfg.health_status = 200;
    cfg.model_id = "stub-model";
    cfg.chat_status = 200;
    cfg.chat_content = "never";
    cfg.catalog_status = 200;
    cfg.delay_ms = 4000;
    if (stub_server_start(&cfg) != 0) {
        fprintf(stderr, "stub server start failed\n");
        acta_db_close(db);
        return 1;
    }

    int exec_id = seed_pending(db);
    if (exec_id < 0) {
        fprintf(stderr, "seeding failed\n");
        stub_server_stop();
        acta_db_close(db);
        return 1;
    }
    check(exec_id > 0, "seeded pending execution");

    /* 1. Spawn the real runner: run <id> --db <path> --timeout 600 */
    char idstr[16];
    snprintf(idstr, sizeof idstr, "%d", exec_id);
    char *run_args[8];
    int ri = 0;
    run_args[ri++] = (char *)runner_bin();
    run_args[ri++] = (char *)"run";
    run_args[ri++] = idstr;
    run_args[ri++] = (char *)"--db";
    run_args[ri++] = (char *)db_path;
    run_args[ri++] = (char *)"--timeout";
    run_args[ri++] = (char *)"600";
    run_args[ri++] = NULL;

    child_t runner;
    int ok = check(spawn(runner_bin(), run_args, &runner) == 0,
                   "spawned real acta_runner child");

    /* 2. Watch for pending -> running (15 s watchdog; fail fast
     *    instead of hanging CI if the child never claims). */
    char status[32];
    if (ok &&
        check(wait_status(db, exec_id, "running", 15000, status, sizeof status),
              "runner claimed the execution (pending -> running)")) {
        /* 3. Kill the runner hard mid-run (the dead-runner scenario). */
        check(kill_child(&runner) == 0, "killed the runner process");
        int rc = -1;
        check(wait_child(&runner, &rc) == 0, "reaped the runner process");

        /* 4. The row is stuck in `running` — nothing auto-recovered it. */
        check(status_now(db, exec_id, status, sizeof status) &&
              strcmp(status, "running") == 0,
              "row stuck in `running` after the kill");

        /* 5. Age past the sweep threshold (no timestamp backdating). */
        msleep((stale_seconds + 2) * 1000);

        /* 6. Second real process: sweep --stale-seconds 5 --db <path>. */
        char *sweep_args[7];
        int si2 = 0;
        sweep_args[si2++] = (char *)runner_bin();
        sweep_args[si2++] = (char *)"sweep";
        sweep_args[si2++] = (char *)"--stale-seconds";
        char nstr[16];
        snprintf(nstr, sizeof nstr, "%d", stale_seconds);
        sweep_args[si2++] = nstr;
        sweep_args[si2++] = (char *)"--db";
        sweep_args[si2++] = (char *)db_path;
        sweep_args[si2++] = NULL;

        child_t sweeper;
        if (spawn(runner_bin(), sweep_args, &sweeper) == 0) {
            int rc2 = -1;
            check(wait_child(&sweeper, &rc2) == 0 && rc2 == 0,
                  "sweep child exited 0");
        } else {
            check(0, "spawning the sweep child");
        }

        /* 7. Assert the recovery. */
        check(status_now(db, exec_id, status, sizeof status) &&
              strcmp(status, "failed") == 0,
              "sweep transitioned running -> failed");
        check(error_contains(db, exec_id,
                            "stale running: no runner activity for 5 s"),
              "failed with the exact stale error");
        check(log_has_event(db, exec_id, "execution_failed"),
              "execution_failed log row present");
        check(log_has_event(db, exec_id, "execution_started"),
              "execution_started log row still present");
        check(!log_has_event(db, exec_id, "execution_completed"),
              "no execution_completed log row");

        /* 8. Repeat sweep: clean no-op (exit 0, row unchanged). */
        child_t sweeper2;
        if (spawn(runner_bin(), sweep_args, &sweeper2) == 0) {
            int rc3 = -1;
            check(wait_child(&sweeper2, &rc3) == 0 && rc3 == 0,
                  "repeat sweep exited 0");
            check(status_now(db, exec_id, status, sizeof status) &&
                  strcmp(status, "failed") == 0,
                  "row still `failed` after repeat sweep");
        } else {
            check(0, "spawning the repeat sweep child");
        }
    }

    /* Cleanup. The stub's in-flight 4 s delay has long since ended by
     * now, so the thread is at accept() and joins immediately. */
    stub_server_stop();
    acta_db_close(db);
    remove_db_files(db_path);

    printf("== dead-runner e2e: %d checks, %d failures\n",
           checks, failures);
    return failures ? 1 : 0;
}
