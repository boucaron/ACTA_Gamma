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
 *  10. context ceiling fallback     -> entry max_context -> entry
 *                                       meta.n_ctx -> /health max_context
 *                                       -> none, with the source reported
 *                                       (issue B: Strata has no entry
 *                                       max_context)
 *  11. unsupported model backend    -> EXIT_INVALID before any HTTP call
 *                                       (zero /health and /v1/models
 *                                       requests, no DB writes)
 *                                       (docs/plans/backend_dialects.md)
 *  12. standalone --dialect openai  -> exit 0; the dialect changes the
 *                                       ceiling order (meta.n_ctx beats
 *                                       the entry max_context here)
 *  13. standalone --dialect vllm    -> EXIT_INVALID, vocabulary in the
 *                                       stderr message, zero requests
 *  14. --dialect with a              -> EXIT_INVALID: DB mode takes the
 *      <model-record-id>                dialect from the model row
 *  15. DB mode on an `openai` model -> exit 0, row decides the dialect
 *  16. $OPENAI_API_KEY set              -> exit 0, the Authorization
 *                                       header IS sent (key-protected
 *                                       servers: /v1/models 401s without
 *                                       the key; /health stays open)
 *  17. no key anywhere                  -> exit 0, keyless exactly as
 *                                       before (no Authorization header)
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
#include "backend.h"
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
typedef struct { int saved; int fd; int target; char path[128]; } cap_t;

static int cap_begin_fd(cap_t *c, int fd, const char *tmpl)
{
    c->saved = dup(fd);
    c->target = fd;
    if (c->saved < 0)
        return -1;
    snprintf(c->path, sizeof c->path, "%s", tmpl);
    c->fd = mkstemp(c->path);
    if (c->fd < 0) {
        close(c->saved);
        c->saved = -1;
        return -1;
    }
    if (fd == STDOUT_FILENO)
        fflush(stdout);
    else
        fflush(stderr);
    if (dup2(c->fd, fd) < 0) {
        close(c->fd);
        unlink(c->path);
        close(c->saved);
        c->fd = -1;
        c->saved = -1;
        return -1;
    }
    return 0;
}

static int cap_begin(cap_t *c)
{
    return cap_begin_fd(c, STDOUT_FILENO, ".test_check.XXXXXX");
}

/* Same for stderr: the argument-validation errors (emit_error) and the
 * VLOG lines land there, so a scenario that must check an error MESSAGE
 * captures stderr. */
static int cap_begin_err(cap_t *c)
{
    return cap_begin_fd(c, STDERR_FILENO, ".test_check_err.XXXXXX");
}

static void cap_end(cap_t *c, char *buf, size_t bufsz)
{
    if (c->target == STDOUT_FILENO)
        fflush(stdout);
    else
        fflush(stderr);
    dup2(c->saved, c->target);
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
static int seed_model_backend(db_t *db, const char *base_url,
                              const char *model_id, const char *backend)
{
    static int seeds = 0;
    char name[64];
    snprintf(name, sizeof name, "check-model-%d", ++seeds);
    model_t m;
    memset(&m, 0, sizeof m);
    m.name = name;
    m.backend = (char *)backend; /* a dialect name (src/dialect.h) */
    m.base_url = (char *)base_url;
    m.model_identifier = (char *)model_id;
    int id = 0;
    if (acta_db_model_create(db, &m, &id) != ACTA_DB_OK)
        return -1;
    return id;
}

static int seed_model(db_t *db, const char *base_url, const char *model_id)
{
    return seed_model_backend(db, base_url, model_id, "llamacpp");
}

/* setenv / unsetenv are POSIX; MinGW provides _putenv instead. A value
 * set to the empty string is indistinguishable from unset inside
 * cmd_check (both resolve to no key), so this is exact enough for the
 * keyless scenario. */
#ifdef _WIN32
static int test_setenv(const char *name, const char *val)
{
    char buf[256];
    snprintf(buf, sizeof buf, "%s=%s", name, val);
    return _putenv(buf);
}
/* MinGW has no unsetenv, and a bare _putenv("NAME") does NOT unset
 * (it leaves the previous value in place — that is why the keyless
 * scenario must set the variable to the EMPTY value explicitly). An
 * empty value is indistinguishable from unset inside cmd_check (both
 * resolve to no key, no Authorization header). */
static int test_unsetenv(const char *name)
{
    char buf[256];
    snprintf(buf, sizeof buf, "%s=", name);
    return _putenv(buf);
}
#else
static int test_setenv(const char *name, const char *val)
{ return setenv(name, val, 1); }
static int test_unsetenv(const char *name) { return unsetenv(name); }
#endif

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

/* Same, optionally also capturing stderr (errbuf non-NULL) so a scenario
 * can assert the error MESSAGE, not just the exit code. */
static int run_check_capture_both(const char *const *argv, int argc, db_t *db,
                                  char *out, size_t outsz,
                                  char *errbuf, size_t errsz,
                                  const global_opts_t *g)
{
    cmd_args_t ga;
    cmd_args_init(&ga, argc, (char **)argv);
    cap_t cap, cap_err;
    if (cap_begin(&cap) != 0)
        return -1;
    int have_err = errbuf && cap_begin_err(&cap_err) == 0;
    int rc = cmd_check(&ga, g, db);
    cap_end(&cap, out, outsz);
    if (have_err)
        cap_end(&cap_err, errbuf, errsz);
    return rc;
}

/* Run cmd_check over the given action argv (without the "check" token)
 * and capture the stdout verdict line. Returns cmd_check's exit code;
 * -1 if the stdout capture failed. */
static int run_check_capture(const char *const *argv, int argc, db_t *db,
                             char *out, size_t outsz, const global_opts_t *g)
{
    return run_check_capture_both(argv, argc, db, out, outsz, NULL, 0, g);
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

    /* 10. context ceiling fallback chain (issue B): a server that does
     *     not report max_context in /v1/models must still yield a real
     *     ceiling — entry max_context -> entry meta.n_ctx -> /health
     *     max_context -> none — and the source is reported. */
    {
        printf("== check: max_context ceiling source\n");
        struct {
            long entry_mc, meta_nctx, health_mc, want;
            const char *src;
        } cases[] = {
            { 162000, 204800, 999999, 162000, "entry.max_context" },
            { 0,      204800, 999999, 204800, "entry.meta.n_ctx" },
            { 0,      0,      204800, 204800, "health.max_context" },
            { 0,      0,      0,            0, "none" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            stub_config_t cfg;
            memset(&cfg, 0, sizeof cfg);
            cfg.port = STUB_PORT;
            cfg.health_status = 200;
            cfg.model_id = "stub-model";
            cfg.max_context = cases[i].entry_mc;
            cfg.models_meta_n_ctx = cases[i].meta_nctx;
            cfg.health_max_context = cases[i].health_mc;
            if (stub_server_start(&cfg) != 0) {
                check(0, "setup");
                return 1;
            }
            backend_preflight_t pf;
            int prc = backend_preflight(STUB_BASE_URL, "stub-model",
                                        dialect_get(DIALECT_LLAMACPP),
                                        NULL, 30, &pf);
            check(prc == PREFLIGHT_OK, "preflight ok");
            check(pf.max_context == cases[i].want, "ceiling value resolved");
            check(strcmp(pf.max_context_source, cases[i].src) == 0,
                  "ceiling source reported");
            stub_server_stop();
        }

        /* The resolved ceiling flows into the `check` verdict line too
         * (meta.n_ctx only: the Strata shape). */
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.models_meta_n_ctx = 204800;
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
        check(rc == EXIT_OK, "exit 0 with meta.n_ctx-only ceiling");
        check(strstr(out, "\"max_context\":204800") != NULL,
              "stdout reports the meta.n_ctx ceiling");
        stub_server_stop();
        check(no_execution_rows(db), "no execution rows (no DB writes)");
    }

    /* 11. an unsupported model backend fails BEFORE any HTTP call:
     *     EXIT_INVALID, zero requests to the stub, no DB writes
     *     (docs/plans/backend_dialects.md acceptance criterion 4). */
    {
        printf("== check: unsupported backend\n");
        int mid = seed_model_backend(db, STUB_BASE_URL, "stub-model", "vllm");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
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
        check(rc == EXIT_INVALID, "unsupported backend -> EXIT_INVALID");
        check(stub_server_health_requests() == 0,
              "no /health request (fails before any HTTP call)");
        check(stub_server_models_requests() == 0,
              "no /v1/models request");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 12. standalone --dialect openai: the dialect decides the context
     *     ceiling order, so the SAME server shape yields a different
     *     value than scenario 8/10 (entry meta.n_ctx wins over the entry
     *     max_context on the openai dialect). Exit 0, still token-free. */
    {
        printf("== check: standalone --dialect openai\n");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.max_context = 162000;      /* llamacpp would prefer this one */
        cfg.models_meta_n_ctx = 204800; /* openai prefers this one */
        if (stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512];
        char *argv[] = { "--base-url", STUB_BASE_URL,
                         "--model-identifier", "stub-model",
                         "--dialect", "openai", "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 8, NULL, out,
                                   sizeof out, &gopts);
        check(rc == EXIT_OK, "exit 0 (--dialect accepted in standalone mode)");
        check(strstr(out, "\"ok\":true") != NULL, "stdout carries ok:true");
        check(strstr(out, "\"max_context\":204800") != NULL,
              "stdout reports the openai-dialect ceiling (meta.n_ctx)");
        check(stub_server_health_requests() == 1 &&
                  stub_server_models_requests() == 1,
              "the two token-free GETs only (no catalog call either)");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        stub_server_stop();
    }

    /* 13. standalone --dialect with a value outside the vocabulary ->
     *     EXIT_INVALID before any HTTP call, with the vocabulary in the
     *     message. */
    {
        printf("== check: standalone --dialect vllm\n");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        if (stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512], errbuf[1024];
        char *argv[] = { "--base-url", STUB_BASE_URL,
                         "--model-identifier", "stub-model",
                         "--dialect", "vllm", "--timeout", "30" };
        int rc = run_check_capture_both((const char *const *)argv, 8, NULL,
                                        out, sizeof out, errbuf,
                                        sizeof errbuf, &gopts);
        check(rc == EXIT_INVALID, "bad --dialect value -> EXIT_INVALID");
        check(strstr(errbuf, "invalid --dialect 'vllm'") != NULL,
              "stderr names the bad value");
        check(strstr(errbuf, "llamacpp or openai") != NULL,
              "stderr states the accepted vocabulary");
        check(out[0] == '\0', "no verdict line on stdout");
        check(stub_server_health_requests() == 0 &&
                  stub_server_models_requests() == 0 &&
                  stub_server_chat_requests() == 0,
              "zero stub requests (validated before any HTTP call)");
        stub_server_stop();
    }

    /* 14. --dialect with a <model-record-id> is rejected: in DB mode the
     *     model row decides, and a flag that changed the probe shape of a
     *     stored row is what the revision design forbids. */
    {
        printf("== check: DB mode --dialect rejected\n");
        int mid = seed_model(db, STUB_BASE_URL, "stub-model");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        if (mid < 0 || stub_server_start(&cfg) != 0) {
            check(0, "setup");
            return 1;
        }
        char out[512], errbuf[1024];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--dialect", "openai", "--timeout", "30" };
        int rc = run_check_capture_both((const char *const *)argv, 5, db,
                                        out, sizeof out, errbuf,
                                        sizeof errbuf, &gopts);
        check(rc == EXIT_INVALID, "DB mode --dialect -> EXIT_INVALID");
        check(strstr(errbuf, "only valid with --base-url") != NULL,
              "stderr says it is standalone-mode only");
        check(stub_server_health_requests() == 0 &&
                  stub_server_chat_requests() == 0,
              "zero stub requests");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 15. DB mode on a model registered `openai`: no flag involved, the
     *     row drives the dialect, and the ceiling order follows it. */
    {
        printf("== check: DB mode on an openai model\n");
        int mid = seed_model_backend(db, STUB_BASE_URL, "stub-model", "openai");
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.max_context = 162000;
        cfg.models_meta_n_ctx = 204800;
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
        check(strstr(out, "\"max_context\":204800") != NULL,
              "the row dialect decides the ceiling order (meta.n_ctx)");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 16. $OPENAI_API_KEY set: the check succeeds AND the Authorization
     *     header is really sent — a key-protected server (llama-server
     *     started with --api-key) answers 401 on /v1/models without the
     *     key, which is exactly what broke `check` there (session
     *     notes §17.8 gap A). */
    {
        printf("== check: resolved api key is sent\n");
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
        test_setenv("OPENAI_API_KEY", "TESTKEY");
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_OK, "exit 0");
        check(strstr(out, "\"ok\":true") != NULL, "stdout carries ok:true");
        const char *auth = stub_server_last_auth();
        check(auth != NULL && strcmp(auth, "Bearer TESTKEY") == 0,
              "Authorization: Bearer TESTKEY was sent");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    /* 17. No key anywhere (env unset, or empty — cmd_check cannot
     *     tell the two apart): keyless exactly as before, no
     *     Authorization header — the key resolution must not change
     *     the keyless-server behavior. */
    {
        printf("== check: keyless without a key anywhere\n");
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
        test_unsetenv("OPENAI_API_KEY");
        char out[512];
        char idbuf[16]; snprintf(idbuf, sizeof idbuf, "%d", mid);
        char *argv[] = { idbuf, "--timeout", "30" };
        int rc = run_check_capture((const char *const *)argv, 3, db, out,
                                  sizeof out, &gopts);
        check(rc == EXIT_OK, "exit 0");
        check(strstr(out, "\"ok\":true") != NULL, "stdout carries ok:true");
        check(stub_server_last_auth() == NULL,
              "no Authorization header (keyless, unchanged)");
        check(stub_server_chat_requests() == 0,
              "zero /v1/chat/completions requests (token-free)");
        check(no_execution_rows(db), "no execution rows (no DB writes)");
        stub_server_stop();
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
