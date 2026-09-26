/*
 * test_shadow.c — OPENAI_API_KEY shadow-warning suite
 * (docs/plans/runner-ops-hardening.md, item 1):
 *   1. (POSIX) env set to EMPTY + non-empty file "api_key" -> the run
 *      completes WITHOUT an Authorization header and stderr carries the
 *      shadow warning (plus the existing empty-key warning).
 *   2. env set non-empty + non-empty file "api_key" -> the run
 *      completes with the ENV key in the Bearer header and stderr
 *      carries the shadow warning.
 *   3. env UNSET + non-empty file "api_key" -> the run completes with
 *      the FILE key; NO shadow warning (nothing is being shadowed).
 *   4. env set non-empty + file key ABSENT -> completes with the ENV
 *      key; NO shadow warning.
 *   5. (POSIX) env set to EMPTY + file key ABSENT -> completes with no
 *      Authorization header; the empty-key warning only, NO shadow
 *      warning.
 *   6. (unit) acta_conf_api_key_shadow_warning truth table.
 *
 * Same harness as test_conf.c: scratch `:memory:` DB seeded from
 * `acta_db/schema.sql` + in-process stub server; `cmd_run` is called
 * directly with a constructed argv.  stderr is captured per run by
 * dup(2)-ing the real stderr aside, freopen'ing a scratch file onto
 * stderr for the duration of the call, then restoring.
 *
 * Run from acta_runner/: `make test`.
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "runner.h"
#include "runner_util.h"
#include "argparse.h"
#include "acta_db.h"
#include "conf.h"
#include "stub_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#include <io.h>
#define probe_close(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define probe_close(s) close(s)
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#define STUB_PORT 8921
#define STUB_BASE_URL "http://127.0.0.1:8921"

/* The canonical one-line warning (must stay in lockstep with
 * acta_conf_api_key_shadow_warning in acta_db/src/conf.c). */
static const char *SHADOW_WARN =
    "warning: OPENAI_API_KEY is set (empty or not) and shadows "
    "the config file \"api_key\"; the effective key is the "
    "environment value";

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

/* ── DB setup (mirrors test_conf.c) ───────────────────────────────── */

static int load_schema(db_t *db)
{
    static const char *paths[] = {
        "../../acta_db/schema.sql",
        "../acta_db/schema.sql",
        "acta_db/schema.sql",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths)[0]; i++) {
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
 * revision); returns the execution id or -1. */
static int seed_pending(db_t *db, const char *model_id)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "shadow-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "shadow-model-%d", seed_calls);

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

/* Run cmd_run with a constructed argv (action token excluded, as
 * main.c passes gopts.argv + 1). */
static int cmd_run_argv(db_t *db, int argc, char **argv)
{
    cmd_args_t ga;
    cmd_args_init(&ga, argc, argv);
    if (cmd_args_validate(&ga) != EXIT_OK)
        return EXIT_INVALID;
    return cmd_run(&ga, runner_gopts, db);
}

static int run_stub(db_t *db, int id)
{
    char idstr[16];
    snprintf(idstr, sizeof idstr, "%d", id);
    char *av[] = { idstr };
    return cmd_run_argv(db, 1, av);
}

/* Force an environment variable to a known state, portably.
 * POSIX setenv/unsetenv are exact.  Observed MSVCRT/mingw _putenv
 * semantics: "name=value" sets; "name=" REMOVES the variable.  There
 * is no way to set a variable to an EMPTY string via _putenv, so
 * ENV_EMPTY returns 0 on Windows and the caller must skip that
 * scenario.  Returns 1 when the requested state was produced. */
enum { ENV_VALUE, ENV_EMPTY, ENV_UNSET };
static int env_force(const char *name, int state, const char *value)
{
#ifdef _WIN32
    char buf[2048];
    if (state == ENV_VALUE) {
        snprintf(buf, sizeof buf, "%s=%s", name, value);
        _putenv(buf);
        return 1;
    }
    if (state == ENV_EMPTY)
        return 0;   /* not producible via _putenv */
    snprintf(buf, sizeof buf, "%s=", name);   /* removes (observed) */
    _putenv(buf);
    return 1;
#else
    if (state == ENV_VALUE)
        setenv(name, value, 1);
    else if (state == ENV_EMPTY)
        setenv(name, "", 1);
    else
        unsetenv(name);
    return 1;
#endif
}

static int port_reachable(int port)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return 0;
    int ok = (connect(s, (struct sockaddr *)&a, sizeof a) == 0);
    probe_close(s);
    return ok;
}

/* ── scratch config-file location (controls acta_conf_default_path) ── */

static int make_conf_locations(const char *tmpdir, char *base, size_t bs,
                              char *confpath, size_t cs)
{
    char gamma[600];
#ifdef _WIN32
    snprintf(base, bs, "%s\\acta_shadow_conf", tmpdir);
    snprintf(gamma, sizeof gamma, "%s\\ACTA_Gamma", base);
    errno = 0;
    int r1 = _mkdir(base);
    errno = 0;
    int r2 = _mkdir(gamma);
    if ((r1 != 0 && errno != EEXIST) || (r2 != 0 && errno != EEXIST))
        return 0;
    snprintf(confpath, cs, "%s\\ACTA_Gamma.conf", gamma);
#else
    snprintf(base, bs, "%s/acta_shadow_conf", tmpdir);
    snprintf(gamma, sizeof gamma, "%s/ACTA_Gamma", base);
    errno = 0;
    int r1 = mkdir(base, 0755);
    errno = 0;
    int r2 = mkdir(gamma, 0755);
    if ((r1 != 0 && errno != EEXIST) || (r2 != 0 && errno != EEXIST))
        return 0;
    snprintf(confpath, cs, "%s/ACTA_Gamma.conf", gamma);
#endif
    return 1;
}

/* Write a config file with the contract mode 0600 (the permission
 * gate refuses group/other-readable files). */
static int write_conf(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
    if (chmod(path, 0600) != 0)
        return 0;
    return 1;
}

/* ── stderr capture ───────────────────────────────────────────────── */

/* Redirect stderr to `capfile` while fn(arg) runs, restore the real
 * stderr, and return the captured bytes (malloc'd, NUL-terminated).
 * NULL on failure — the caller reports SKIP and must not assert.
 * The real stderr is saved with dup() before freopen; the capture
 * file must exist only for the duration of the call. */
static char *capture_stderr(const char *capfile, int (*fn)(void *), void *arg)
{
    remove(capfile);
    int saved = dup(STDERR_FILENO);
    if (saved < 0)
        return NULL;
    FILE *cap = freopen(capfile, "w", stderr);
    if (!cap) {
        close(saved);
        return NULL;
    }
    fn(arg);
    fflush(stderr);
    fclose(cap);
    dup2(saved, STDERR_FILENO);
    close(saved);

    FILE *f = fopen(capfile, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (len > 0 && fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

struct run_args {
    db_t *db;
    int id;
};
static int run_stub_cb(void *arg)
{
    struct run_args *a = (struct run_args *)arg;
    return run_stub(a->db, a->id);
}

int main(void)
{
    /* Verbose level 1 so VLOG action summaries go to stderr. */
    static const global_opts_t gopts = { NULL, 1, 0, 0, 0, NULL };
    runner_gopts = &gopts;

    char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0])
        tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0])
        tmpdir = ".";
    if (strlen(tmpdir) + 80 > 512) {
        fprintf(stderr, "SKIP: temp dir too long\n");
        return 0;
    }
    char base[512], confpath[700], capfile[700];
    if (!make_conf_locations(tmpdir, base, sizeof base,
                             confpath, sizeof confpath)) {
        fprintf(stderr, "SKIP: cannot create config locations\n");
        return 0;
    }
#ifdef _WIN32
    snprintf(capfile, sizeof capfile, "%s\\shadow_stderr.txt", tmpdir);
#else
    snprintf(capfile, sizeof capfile, "%s/shadow_stderr.txt", tmpdir);
#endif
    /* Point the platform app-data base at the scratch dir so
     * acta_conf_default_path() is deterministic. */
#ifdef _WIN32
    check(env_force("APPDATA", ENV_VALUE, base),
          "APPDATA -> scratch dir");
#else
    check(env_force("XDG_DATA_HOME", ENV_VALUE, base),
          "XDG_DATA_HOME -> scratch dir");
#endif
    check(strcmp(confpath, acta_conf_default_path()) == 0,
          "acta_conf_default_path() == scratch conf path");

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

    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.port = STUB_PORT;
    cfg.health_status = 200;
    cfg.model_id = "stub-model";
    cfg.chat_status = 200;
    cfg.chat_content = "stub-response";

    /* 6. (unit) acta_conf_api_key_shadow_warning truth table. */
    {
        printf("== unit: acta_conf_api_key_shadow_warning\n");
        const char *msg = "canary";

        check(acta_conf_api_key_shadow_warning("env-key", "file-key", &msg)
                  == 1,
              "env set (non-empty) + non-empty file key -> shadow");
        check(msg != NULL && strstr(msg, "shadows") != NULL,
              "shadow message names the shadowing");
        check(strstr(msg, "effective key is the environment value")
                  != NULL,
              "shadow message names the effective key");

        check(acta_conf_api_key_shadow_warning("", "file-key", &msg) == 1,
              "env set to EMPTY + non-empty file key -> shadow (the "
              "footgun case)");
        check(msg != NULL, "shadow message set (empty env)");

        check(acta_conf_api_key_shadow_warning(NULL, "file-key", &msg)
                  == 0,
              "env UNSET + file key -> no shadow");
        check(msg == NULL, "no message when no shadow (unset)");

        check(acta_conf_api_key_shadow_warning("env-key", NULL, &msg) == 0,
              "env set + file key ABSENT -> no shadow");
        check(msg == NULL, "no message when no shadow (absent)");

        check(acta_conf_api_key_shadow_warning("env-key", "", &msg) == 0,
              "env set + file key EMPTY -> nothing to shadow");
        check(msg == NULL, "no message when no shadow (empty)");

        check(acta_conf_api_key_shadow_warning("env-key", "file-key", NULL)
                  == 1,
              "NULL msg pointer tolerated");
    }

    if (stub_server_start(&cfg) != 0) {
        check(0, "stub server start");
    } else {
        check(port_reachable(STUB_PORT), "stub port reachable");

        struct run_args ra;
        ra.db = db;

        /* 1. (POSIX) env EMPTY + non-empty file key: shadow warning on
         *    stderr, run completes WITHOUT an Authorization header.
         *    Not producible via _putenv on Windows: skipped there. */
#ifndef _WIN32
        {
            printf("== cmd_run: empty env var shadows non-empty file key\n");
            check(env_force("OPENAI_API_KEY", ENV_EMPTY, NULL),
                  "variable set to empty string");
            check(write_conf(confpath,
                             "{\"api_key\": \"file-key-42\"}"),
                  "wrote conf file with api_key (0600)");
            int id = seed_pending(db, "stub-model");
            check(id > 0, "seeded 1 pending");
            ra.id = id;
            char *cap = capture_stderr(capfile, run_stub_cb, &ra);
            check(cap != NULL, "captured stderr");
            if (cap) {
                check(strstr(cap, SHADOW_WARN) != NULL,
                      "stderr carries the shadow warning");
                free(cap);
            }
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check(stub_server_last_auth() == NULL,
                  "NO Authorization header (empty env var wins, file "
                  "key not used)");
        }
        env_force("OPENAI_API_KEY", ENV_UNSET, NULL);
#endif

        /* 2. env non-empty + non-empty file key: shadow warning, ENV
         *    key in the Bearer header. */
        {
            printf("== cmd_run: env set shadows non-empty file key\n");
            check(env_force("OPENAI_API_KEY", ENV_VALUE, "env-key-1"),
                  "variable set to non-empty value");
            check(write_conf(confpath,
                             "{\"api_key\": \"file-key-42\"}"),
                  "wrote conf file with api_key (0600)");
            int id = seed_pending(db, "stub-model");
            check(id > 0, "seeded 1 pending");
            ra.id = id;
            char *cap = capture_stderr(capfile, run_stub_cb, &ra);
            check(cap != NULL, "captured stderr");
            if (cap) {
                check(strstr(cap, SHADOW_WARN) != NULL,
                      "stderr carries the shadow warning");
                free(cap);
            }
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check(stub_server_last_auth() &&
                      strcmp(stub_server_last_auth(), "Bearer env-key-1")
                          == 0,
                  "Authorization header carries the ENV key (file key "
                  "ignored)");
        }
        env_force("OPENAI_API_KEY", ENV_UNSET, NULL);

        /* 3. env UNSET + non-empty file key: NO shadow warning; the
         *    file key is used. */
        {
            printf("== cmd_run: env unset, file key alone (no shadow)\n");
            check(env_force("OPENAI_API_KEY", ENV_UNSET, NULL),
                  "variable removed");
            check(write_conf(confpath,
                             "{\"api_key\": \"file-key-42\"}"),
                  "wrote conf file with api_key (0600)");
            int id = seed_pending(db, "stub-model");
            check(id > 0, "seeded 1 pending");
            ra.id = id;
            char *cap = capture_stderr(capfile, run_stub_cb, &ra);
            check(cap != NULL && cap[0] != '\0',
                  "captured stderr (capture is functional)");
            if (cap) {
                check(strstr(cap, SHADOW_WARN) == NULL,
                      "NO shadow warning in stderr");
                free(cap);
            }
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check(stub_server_last_auth() &&
                      strcmp(stub_server_last_auth(), "Bearer file-key-42")
                          == 0,
                  "Authorization header carries the FILE key");
        }

        /* 4. env non-empty + file key ABSENT: NO shadow warning. */
        {
            printf("== cmd_run: env set, no file key (no shadow)\n");
            check(env_force("OPENAI_API_KEY", ENV_VALUE, "env-key-1"),
                  "variable set to non-empty value");
            remove(confpath); /* ensure no file in the scratch dir */
            int id = seed_pending(db, "stub-model");
            check(id > 0, "seeded 1 pending");
            ra.id = id;
            char *cap = capture_stderr(capfile, run_stub_cb, &ra);
            check(cap != NULL && cap[0] != '\0',
                  "captured stderr (capture is functional)");
            if (cap) {
                check(strstr(cap, SHADOW_WARN) == NULL,
                      "NO shadow warning in stderr");
                free(cap);
            }
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check(stub_server_last_auth() &&
                      strcmp(stub_server_last_auth(), "Bearer env-key-1")
                          == 0,
                  "Authorization header carries the ENV key");
        }
        env_force("OPENAI_API_KEY", ENV_UNSET, NULL);

        /* 5. (POSIX) env EMPTY + file key ABSENT: empty-key warning
         *    only, NO shadow warning (nothing to shadow). */
#ifndef _WIN32
        {
            printf("== cmd_run: empty env var, no file key (no shadow)\n");
            check(env_force("OPENAI_API_KEY", ENV_EMPTY, NULL),
                  "variable set to empty string");
            remove(confpath); /* ensure no file in the scratch dir */
            int id = seed_pending(db, "stub-model");
            check(id > 0, "seeded 1 pending");
            ra.id = id;
            char *cap = capture_stderr(capfile, run_stub_cb, &ra);
            check(cap != NULL, "captured stderr");
            if (cap) {
                check(strstr(cap, "OPENAI_API_KEY is empty") != NULL,
                      "empty-key warning present");
                check(strstr(cap, SHADOW_WARN) == NULL,
                      "NO shadow warning in stderr");
                free(cap);
            }
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check(stub_server_last_auth() == NULL,
                  "NO Authorization header (empty env var)");
        }
        env_force("OPENAI_API_KEY", ENV_UNSET, NULL);
#endif

        stub_server_stop();
    }

    /* clean up */
    remove(confpath);
    remove(capfile);
    acta_db_close(db);
    char gamma[600];
#ifdef _WIN32
    snprintf(gamma, sizeof gamma, "%s\\ACTA_Gamma", base);
    _rmdir(gamma);
    _rmdir(base);
    env_force("APPDATA", ENV_UNSET, NULL);
#else
    snprintf(gamma, sizeof gamma, "%s/ACTA_Gamma", base);
    rmdir(gamma);
    rmdir(base);
    env_force("XDG_DATA_HOME", ENV_UNSET, NULL);
#endif

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
