/*
 * test_check.c — token-free backend health-check tests for the `check`
 * action (docs/runner_contract.md, check action).
 *
 * Covers:
 *   1. DB mode success              -> exit 0, ok:true, max_context
 *                                       reported
 *   2. /health 503                  -> "model still loading", EXIT_HTTP
 *   3. model not in catalog         -> "model not served", EXIT_HTTP
 *   4. catalog endpoint error       -> "catalog unreachable", EXIT_HTTP
 *   5. connection refused (no stub) -> "server unreachable", EXIT_HTTP
 *   6. slow /health + small timeout -> EXIT_TIMEOUT
 *   7. unknown <model-record-id>    -> EXIT_NOT_FOUND
 *   8. standalone mode              -> exit 0, ok:true, no DB lookup
 *                                       (db handle NULL)
 *   9. argument conflicts           -> EXIT_INVALID (one standalone flag
 *                                       only; flags + positional; no args)
 *
 * The plan's defining property is asserted in every scenario: the stub's
 * POST /v1/chat/completions request count stays 0 (zero tokens — the
 * check never performs a chat completion), and no execution or
 * execution_log rows exist afterwards (no DB writes of any kind).
 *
 * Run from tests/check/ (or anywhere): `make test` in acta_runner/.
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

#define STUB_PORT 8921
#define STUB_BASE_URL "http://127.0.0.1:8921"

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
 * check.c without main.c, so the definition lives here. */
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
    snprintf(c->path, sizeof c->path, ".test_check.XXXXXX");
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

/* ── helpers ──────────────────────────────────────────────────────── */

/* Load acta_db/schema.sql from a couple of likely locations. */
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

/* Seed one live model row pointing at the stub; returns the row id or
 * -1. Unique names (the schema has unique indexes on models.name in the
 * root folder). */
static int seed_model(db_t *db, const char *base_url, const char *model_id)
{
    static int seeds = 0;
    char name[64];
    snprintf(name, sizeof name, "check-model-%d", ++seeds);
    model_t m;
    memset(&m, 0, sizeof m);
    m.name = name;
    m.backend = "llama";
    m.base_url = (char *)base_url;
    m.model_identifier = (char *)model_id;
    int id = 0;
    if (acta_db_model_create(db, &m, &id) != ACTA_DB_OK)
        return -1;
    return id;
}

/* True when no execution rows exist yet (the "no DB writes" assertion). */
static int no_execution_rows(db_t *db)
{
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    int err = ACTA_DB_OK, n = 0;
    execution_t **rows = acta_db_execution_query(db, &q, 0, 1, &n, &err);
    if (rows)
        acta_db_execution_list_free(rows, n);
    return err == ACTA_DB_OK && n == 0;
}

/* Run cmd_check over the given action argv (without the "check" token)
 * and capture the stdout verdict line. Returns cmd_check's exit code;
 * -1 if the stdout capture failed. */
static int run_check_capture(const char *const *argv, int argc, db_t *db,
                             char *out, size_t outsz, const global_opts_t *g)
{
    cmd_args_t ga;
    cmd_args_init(&ga, argc, (char **)argv);
    cap_t cap;
    if (cap_begin(&cap) != 0)
        return -1;
    int rc = cmd_check(&ga, g, db);
    cap_end(&cap, out, outsz);
    return rc;
}

/* ── scenarios ────────────────────────────────────────────────────── */

int main(void)
{
    /* Verbose level 1 so VLOG action summaries go to stderr. */
    static const global_opts_t gopts = { NULL, 1, 0, 0, 0, NULL };
    runner_gopts = &gopts;

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

    /* 1. DB mode success: health 200 + model in catalog -> ok, exit 0,
     *    max_context reported */
    {
        printf("== check: DB mode success\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.max_context = 162000;
        if (mid < 0 || stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_OK, "exit 0");
        check(strstr(out, "\"ok\":true") != NULL, "stdout carries ok:true");
        check(strstr(out, "\"model\":\"stub-model\"") != NULL,
              "stdout carries the model id");
        check(strstr(out, "\"max_context\":162000") != NULL,
              "stdout reports max_context");
        check(strstr(out, "\"base_url\":\"" STUB_BASE_URL "\"") != NULL,
              "stdout carries the base_url");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 2. /health 503 -> "model still loading", EXIT_HTTP */
    {
        printf("== check: /health 503\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 503;
        cfg.model_id = "stub-model";
        if (mid < 0 || stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_HTTP, "EXIT_HTTP");
        check(strstr(out, "\"ok\":false") != NULL, "stdout carries ok:false");
        check(strstr(out, "\"verdict\":\"model still loading\"") != NULL,
              "verdict is model still loading");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 3. model not in catalog -> "model not served", EXIT_HTTP */
    {
        printf("== check: model not served\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "other-model";
        if (mid < 0 || stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_HTTP, "EXIT_HTTP");
        check(strstr(out, "\"verdict\":\"model not served\"") != NULL,
              "verdict is model not served");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 4. catalog endpoint error -> "catalog unreachable", EXIT_HTTP */
    {
        printf("== check: catalog unreachable\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.models_status = 500;
        if (mid < 0 || stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_HTTP, "EXIT_HTTP");
        check(strstr(out, "\"verdict\":\"catalog unreachable\"") != NULL,
              "verdict is catalog unreachable");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 5. connection refused (no stub running) -> "server unreachable",
     *    EXIT_HTTP */
    {
        printf("== check: server unreachable\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        if (mid < 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_HTTP, "EXIT_HTTP");
        check(strstr(out, "\"verdict\":\"server unreachable\"") != NULL,
              "verdict is server unreachable");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
    }

    /* 6. slow /health + small --timeout -> EXIT_TIMEOUT */
    {
        printf("== check: timeout\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.delay_ms = 2500;
        if (mid < 0 || stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "1" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_TIMEOUT, "EXIT_TIMEOUT");
        check(strstr(out, "\"ok\":false") != NULL, "stdout carries ok:false");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 7. unknown <model-record-id> -> EXIT_NOT_FOUND (no stub needed) */
    {
        printf("== check: unknown model id\n");
        char out[512];
        char *argv[] = { "999999", "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_NOT_FOUND, "EXIT_NOT_FOUND");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
    }

    /* 8. standalone mode: no DB lookup (db handle NULL), exit 0 */
    {
        printf("== check: standalone mode\n");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.max_context = 162000;
        if (stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char *argv[] = { "--base-url", STUB_BASE_URL,
                         "--model-identifier", "stub-model",
                         "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 6, NULL, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_OK, "exit 0");
        check(strstr(out, "\"ok\":true") != NULL, "stdout carries ok:true");
        check(strstr(out, "\"max_context\":162000") != NULL,
              "stdout reports max_context");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        stub_server_stop();
    }

    /* 9. argument conflicts -> EXIT_INVALID (no stub needed) */
    {
        printf("== check: argument conflicts\n");
        char out[512];

        char *argv1[] = { "--base-url", STUB_BASE_URL, "--timeout", "30" };
        int rc1 = run_check_capture((const char *const *)argv1, 4, db, out,
                                   sizeof out, &gopts);
        check(rc1 == EXIT_INVALID,
              "one standalone flag only -> EXIT_INVALID");

        char *argv2[] = { "--base-url", STUB_BASE_URL,
                          "--model-identifier", "stub-model",
                          "1", "--timeout", "30" };
        int rc2 = run_check_capture((const char *const *)argv2, 7, db, out,
                                   sizeof out, &gopts);
        check(rc2 == EXIT_INVALID,
              "standalone flags + positional -> EXIT_INVALID");

        char *argv3[] = { "--timeout", "30" };
        int rc3 = run_check_capture((const char *const *)argv3, 2, db, out,
                                   sizeof out, &gopts);
        check(rc3 == EXIT_INVALID,
              "no <model-record-id> and no standalone flags -> EXIT_INVALID");

        check(no_execution_rows(db), "no execution rows (no DB writes)");
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
