/*
 * test_autopreflight.c — auto preflight before the atomic claim
 * (docs/plans/runner-ops-hardening.md, item 4).
 *
 * Covers:
 *   1. `run --pending`, dead server (no stub)     -> exit 12, the `check`
 *      JSON verdict line on stdout ("server unreachable"), rows stay
 *      `pending`, no execution_log rows, zero chat requests (no claim,
 *      no DB writes).
 *   2. `run --pending`, /health 503               -> exit 12, "model
 *      still loading", rows stay pending, no DB writes.
 *   3. `run --pending`, model not served          -> exit 12, "model not
 *      served", rows stay pending, no DB writes.
 *   4. `run --pending`, slow server + --timeout   -> exit 13, "server
 *      unreachable", rows stay pending, no DB writes.
 *   5. `run <id>`, dead server (no stub)          -> exit 12, "server
 *      unreachable", row stays pending, no DB writes.
 *   6. healthy server, 2 pending rows             -> exit 0, both
 *      `completed`; /health + /v1/models counts pin "once per
 *      acta_runner invocation": 1 pre-claim auto preflight + 1
 *      per-execution pipeline preflight each = 3 calls, not 4.
 *
 * Same harness as test_pending.c: scratch `:memory:` DB seeded from
 * `acta_db/schema.sql` + in-process stub server. `cmd_run` is called
 * directly with a constructed argv (no process spawn); the stdout verdict
 * line is captured the same way as in tests/check/test_check.c.
 *
 * Run from tests/run/ (or anywhere): `make test` in acta_runner/.
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "runner.h"
#include "runner_util.h"
#include "argparse.h"
#include "acta_db.h"
#include "stub_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#define STUB_PORT 8919
#define STUB_BASE_URL "http://127.0.0.1:8919"

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (cond)
        printf("  PASS %s\n", what);
    else {
        printf("  FAIL %s\n", what);
        failures++;
    }
}

/* main.c owns runner_gopts in the real binary; the test binary links
 * run.c without main.c, so the definition lives here. */
const global_opts_t *runner_gopts;

/* ── stdout capture (the verdict line is the result contract) ─────── */

/* Redirect stdout to a temp file in the CWD so the verdict JSON can be
 * inspected; cap_end() restores the original stdout and copies the
 * captured output into buf. The file is created with an mkstemp
 * template in the current working directory (NOT the system temp dir,
 * which is unwritable on this MSYS2 host) and unlinked afterwards.
 * dup/dup2 on the fd — no freopen, so it works identically on POSIX
 * and MSYS2/MinGW. */
typedef struct { int saved; int fd; char path[128]; } cap_t;

static int cap_begin(cap_t *c)
{
    c->saved = dup(STDOUT_FILENO);
    if (c->saved < 0)
        return -1;
    snprintf(c->path, sizeof c->path, ".test_autopreflight.XXXXXX");
    c->fd = mkstemp(c->path);
    if (c->fd < 0) {
        close(c->saved);
        c->saved = -1;
        return -1;
    }
    fflush(stdout);
    if (dup2(c->fd, STDOUT_FILENO) < 0) {
        close(c->fd);
        unlink(c->path);
        close(c->saved);
        c->fd = -1;
        c->saved = -1;
        return -1;
    }
    return 0;
}

static void cap_end(cap_t *c, char *buf, size_t bufsz)
{
    fflush(stdout);
    dup2(c->saved, STDOUT_FILENO);
    close(c->saved);
    lseek(c->fd, 0, SEEK_SET);
    size_t n = 0;
    while (n + 1 < bufsz) {
        int r = read(c->fd, buf + n, 1);
        if (r <= 0)
            break;
        n++;
    }
    buf[n] = '\0';
    close(c->fd);
    unlink(c->path);
}

/* ── DB setup ─────────────────────────────────────────────────────── */

static int load_schema(db_t *db)
{
    static const char *paths[] = {
        "../../acta_db/schema.sql",
        "../acta_db/schema.sql",
        "acta_db/schema.sql",
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

/* Seed one pending execution (context + skill + model, each with a
 * revision); returns the execution id or -1. `model_id` is the
 * model_identifier the model revision carries (the stub serves
 * "stub-model"; a different value makes the model-not-served check
 * fail). */
static int seed_pending(db_t *db, const char *model_id)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "auto-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "auto-model-%d", seed_calls);

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
    s.name = skill_name;
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
    m.name = model_name;
    m.backend = "llama";
    m.base_url = STUB_BASE_URL;
    m.model_identifier = (char *)model_id;
    int model_id_ = 0;
    if (acta_db_model_create(db, &m, &model_id_) != ACTA_DB_OK)
        return -1;

    model_revision_t *mr =
        acta_db_model_revision_get_latest(db, model_id_, &err);
    if (!mr)
        return -1;
    int model_rev_id = mr->id;
    acta_db_model_revision_free(mr);

    execution_t e;
    memset(&e, 0, sizeof e);
    e.context_id = ctx_id;
    e.skill_revision_id = skill_rev_id;
    e.model_revision_id = model_rev_id;
    int id = 0;
    if (acta_db_execution_create(db, &e, &id) != ACTA_DB_OK)
        return -1;
    return id;
}

/* ── verification helpers ─────────────────────────────────────────── */

static int execution_status(db_t *db, int id, char *out, size_t outsz)
{
    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, id, &err);
    if (err != ACTA_DB_OK || !e)
        return 0;
    snprintf(out, outsz, "%s", e->status ? e->status : "(null)");
    acta_db_execution_free(e);
    return 1;
}

/* No execution_log rows at all for the execution (the "no DB writes"
 * assertion: a pre-claim abort must leave no audit trace). */
static int no_log_rows(db_t *db, int exec_id)
{
    int err = ACTA_DB_OK, n = 0;
    execution_log_t **rows = acta_db_execution_log_list_by_execution(
        db, exec_id, NULL, 0, ACTA_DB_MAX_PAGE, &n, &err);
    int ok = err == ACTA_DB_OK && n == 0;
    if (rows)
        acta_db_execution_log_list_free(rows, n);
    return ok;
}

/* Count executions with the given status.
 * The lister returns NULL for an empty result (n == 0, err == OK),
 * so only err != OK is a failure. */
static int count_status(db_t *db, const char *status)
{
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.status = status;
    int err = ACTA_DB_OK, n = 0;
    execution_t **rows =
        acta_db_execution_query(db, &q, 0, ACTA_DB_MAX_PAGE, &n, &err);
    if (err != ACTA_DB_OK)
        return -1;
    if (rows)
        acta_db_execution_list_free(rows, n);
    return n;
}

/* Run cmd_run with a constructed argv (action token excluded, as
 * main.c passes gopts.argv + 1), capturing the stdout verdict line.
 * Returns cmd_run's exit code; -1 if the stdout capture failed. */
static int cmd_run_capture(db_t *db, int argc, char **argv,
                           char *out, size_t outsz)
{
    cmd_args_t ga;
    cmd_args_init(&ga, argc, argv);
    if (cmd_args_validate(&ga) != EXIT_OK)
        return EXIT_INVALID;
    cap_t cap;
    if (cap_begin(&cap) != 0)
        return -1;
    int rc = cmd_run(&ga, runner_gopts, db);
    cap_end(&cap, out, outsz);
    return rc;
}

/* Set or unset an environment variable. setenv/unsetenv are POSIX and
 * MinGW's stdlib.h does not declare them; _putenv removes the variable
 * when given "name" without '=' and sets it to empty with "name=". */
static void env_set_or_unset(const char *name, const char *value)
{
#ifdef _WIN32
    char buf[256];
    if (value)
        snprintf(buf, sizeof buf, "%s=%s", name, value);
    else
        snprintf(buf, sizeof buf, "%s", name);
    _putenv(buf);
#else
    if (value)
        setenv(name, value, 1);
    else
        unsetenv(name);
#endif
}

/* ── scenarios ────────────────────────────────────────────────────── */

int main(void)
{
    /* Verbose level 1 so VLOG action summaries go to stderr. */
    static const global_opts_t gopts = { NULL, 1, 0, 0, 0, NULL };
    runner_gopts = &gopts;

    /* API key presence policy: $OPENAI_API_KEY must be SET
     * (docs/runner_contract.md, decision 4). */
    env_set_or_unset("OPENAI_API_KEY", "stub-key");

    int err = ACTA_DB_OK;
    db_t *db = acta_db_open(":memory:", &err, ACTA_DB_OPEN_CREATE);
    if (!db) {
        fprintf(stderr, "cannot open in-memory db: %s\n",
                acta_db_strerror(err));
        return 1;
    }
    if (load_schema(db) != ACTA_DB_OK) {
        fprintf(stderr, "cannot load schema: %s\n",
                acta_db_last_error(db) ? acta_db_last_error(db) : "unknown");
        acta_db_close(db);
        return 1;
    }

    /* 1. dead server (no stub), 2 pending rows -> exit 12, the `check`
     *    verdict line, no claim, no DB writes, rows stay pending */
    {
        printf("== auto preflight: --pending, dead server\n");
        int ids[2];
        int ok = 1;
        for (int i = 0; i < 2; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 2 pending");

        char out[512];
        char *av[] = { "--pending", "--timeout", "30" };
        int rc = cmd_run_capture(db, 3, av, out, sizeof out);
        check(rc == EXIT_HTTP, "exit 12 (the check verdict code)");
        check(strstr(out, "\"ok\":false") != NULL,
              "stdout carries ok:false (the check verdict line)");
        check(strstr(out, "\"verdict\":\"server unreachable\"") != NULL,
              "verdict is server unreachable");
        check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 2,
              "rows stay pending (nothing claimed)");
        check(no_log_rows(db, ids[0]) && no_log_rows(db, ids[1]),
              "no execution_log rows (no DB writes)");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
    }

    /* 2. /health 503 -> exit 12, "model still loading", no DB writes */
    {
        printf("== auto preflight: --pending, /health 503\n");
        int ids[2];
        int ok = 1;
        for (int i = 0; i < 2; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 2 pending");

        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 503;
        cfg.model_id = "stub-model";
        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char out[512];
            char *av[] = { "--pending", "--timeout", "30" };
            int rc = cmd_run_capture(db, 3, av, out, sizeof out);
            check(rc == EXIT_HTTP, "exit 12");
            check(strstr(out, "\"verdict\":\"model still loading\"") != NULL,
                  "verdict is model still loading");
            check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 2,
                  "rows stay pending (nothing claimed)");
            check(no_log_rows(db, ids[0]) && no_log_rows(db, ids[1]),
                  "no execution_log rows (no DB writes)");
            check(stub_server_chat_requests() == 0,
                  "zero /v1/chat/completions requests (token-free)");
            stub_server_stop();
        }
    }

    /* 3. model not served -> exit 12, "model not served", no DB writes */
    {
        printf("== auto preflight: --pending, model not served\n");
        int ids[2];
        int ok = 1;
        for (int i = 0; i < 2; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 2 pending");

        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "other-model";
        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char out[512];
            char *av[] = { "--pending", "--timeout", "30" };
            int rc = cmd_run_capture(db, 3, av, out, sizeof out);
            check(rc == EXIT_HTTP, "exit 12");
            check(strstr(out, "\"verdict\":\"model not served\"") != NULL,
                  "verdict is model not served");
            check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 2,
                  "rows stay pending (nothing claimed)");
            check(no_log_rows(db, ids[0]) && no_log_rows(db, ids[1]),
                  "no execution_log rows (no DB writes)");
            check(stub_server_chat_requests() == 0,
                  "zero /v1/chat/completions requests (token-free)");
            stub_server_stop();
        }
    }

    /* 4. slow server + --timeout 1 -> exit 13, "server unreachable" */
    {
        printf("== auto preflight: --pending, timeout\n");
        int ids[2];
        int ok = 1;
        for (int i = 0; i < 2; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 2 pending");

        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.delay_ms = 2500;
        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char out[512];
            char *av[] = { "--pending", "--timeout", "1" };
            int rc = cmd_run_capture(db, 3, av, out, sizeof out);
            check(rc == EXIT_TIMEOUT, "exit 13 (the check timeout code)");
            check(strstr(out, "\"verdict\":\"server unreachable\"") != NULL,
                  "verdict is server unreachable");
            check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 2,
                  "rows stay pending (nothing claimed)");
            check(no_log_rows(db, ids[0]) && no_log_rows(db, ids[1]),
                  "no execution_log rows (no DB writes)");
            stub_server_stop();
        }
    }

    /* 5. `run <id>` with a dead server -> exit 12, verdict line, the row
     *    stays pending, no DB writes */
    {
        printf("== auto preflight: run <id>, dead server\n");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");

        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", id);
        char *av[] = { idbuf, "--timeout", "30" };
        int rc = cmd_run_capture(db, 2, av, out, sizeof out);
        check(rc == EXIT_HTTP, "exit 12");
        check(strstr(out, "\"verdict\":\"server unreachable\"") != NULL,
              "verdict is server unreachable");
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
              "row stays pending (nothing claimed)");
        check(no_log_rows(db, id), "no execution_log rows (no DB writes)");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
    }

    /* 6. healthy server, 2 pending rows -> both complete; the
     *    /health + /v1/models counts pin "once per invocation":
     *    1 pre-claim auto preflight + 1 per-execution pipeline
     *    preflight each = 3 calls (NOT 4 = per-row auto preflight,
     *    and NOT 2 = no auto preflight). */
    {
        printf("== auto preflight: healthy, 2 pending rows\n");
        int ids[2];
        int ok = 1;
        for (int i = 0; i < 2; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 2 pending");

        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char out[512];
            char *av[] = { "--pending" };
            int rc = cmd_run_capture(db, 1, av, out, sizeof out);
            check(rc == EXIT_OK, "exit 0 (the run proceeds)");
            char st[32];
            check(execution_status(db, ids[0], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "first row completed");
            check(execution_status(db, ids[1], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "second row completed");
            check(stub_server_health_requests() == 3,
                  "/health called 3x: 1 auto preflight + 2 pipeline");
            check(stub_server_models_requests() == 3,
                  "/v1/models called 3x: 1 auto preflight + 2 pipeline");
            check(stub_server_chat_requests() == 2,
                  "one chat completion per execution");
            stub_server_stop();
        }
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
