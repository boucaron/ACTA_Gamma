/*
 * test_conf.c — config-file integration suite
 * (docs/runner_contract.md, decision 4):
 *   1. missing file + $OPENAI_API_KEY unset -> the existing hard error
 *      (EXIT_INVALID) before any claim; the seeded row stays `pending`.
 *   2. file fallback: env unset, file "api_key" present -> the execution
 *      completes and the Bearer header carries the FILE key.
 *   3. env-set-wins: env set AND file "api_key" present -> the Bearer
 *      header carries the ENV key, not the file key.
 *   4. (POSIX) env set to EMPTY still wins: warning only, run completes
 *      WITHOUT an Authorization header (the file key is not consulted as
 *      a channel).
 *   5. (POSIX) bad-permissions refusal: a readable file whose mode gives
 *      group/other read access (not 0600) is refused fail-closed ->
 *      EXIT_INVALID before any claim; row stays pending (on Windows the
 *      gate warns and reads the file anyway, so the scenario is
 *      POSIX-only).
 *   6. readable-but-malformed file -> EXIT_INVALID fail-closed before any
 *      claim; row stays pending.
 *   7. (unit) max_chars / timeout file-over-builtin resolution order
 *      (acta_conf_resolve_max_chars / acta_conf_resolve_timeout).
 *
 * The DB-path precedence with/without the file is pinned by the
 * acta_cli/tests/dbpath suite (work item 3); the acta_conf parser /
 * status-helper unit rules are pinned by the acta_cli/tests/conf suite
 * (work item 1 / this file's section 7 covers only the resolution order).
 *
 * Same harness as test_api_key.c: scratch `:memory:` DB seeded from
 * `acta_db/schema.sql` + in-process stub server.  `cmd_run` is
 * called directly with a constructed argv (no process spawn).  The
 * platform app-data env var (APPDATA / XDG_DATA_HOME) is pointed at a
 * scratch dir so `acta_conf_default_path()` is fully controlled.
 *
 * Run from tests/run/ (or anywhere): `make test` in acta_runner/.
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

static void check_str(const char *got, const char *want, const char *what)
{
    checks++;
    if (want) {
        if (got && strcmp(got, want) == 0)
            printf("  PASS %s\n", what);
        else {
            printf("  FAIL %s (got %s)\n", what, got ? got : "<none>");
            failures++;
        }
    } else {
        if (got == NULL)
            printf("  PASS %s\n", what);
        else {
            printf("  FAIL %s (got \"%s\")\n", what, got);
            failures++;
        }
    }
}

/* main.c owns runner_gopts in the real binary; the test binary links
 * run.c without main.c, so the definition lives here. */
const global_opts_t *runner_gopts;

/* ── DB setup (mirrors test_api_key.c) ─────────────────────────────── */

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
    snprintf(skill_name, sizeof skill_name, "conf-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "conf-model-%d", seed_calls);

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
    /* name and value are each char* (up to 511 bytes per snprintf's
     * worst case); size the buffer for name + '=' + value + NUL so
     * -Wformat-truncation cannot fire. */
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

/* Create <tmpdir>/acta_conf_conf/ and its "ACTA_Gamma" subdir so the
 * platform app-data env var points at a fully controlled scratch base.
 * Stores the full config-file path in confpath.  Returns 1 ok, 0
 * failure. */
static int make_conf_locations(const char *tmpdir, char *base, size_t bs,
                              char *confpath, size_t cs)
{
    char gamma[600];
#ifdef _WIN32
    snprintf(base, bs, "%s\\acta_conf_conf", tmpdir);
    snprintf(gamma, sizeof gamma, "%s\\ACTA_Gamma", base);
    errno = 0;
    int r1 = _mkdir(base);
    errno = 0;
    int r2 = _mkdir(gamma);
    if ((r1 != 0 && errno != EEXIST) || (r2 != 0 && errno != EEXIST))
        return 0;
    snprintf(confpath, cs, "%s\\ACTA_Gamma.conf", gamma);
#else
    snprintf(base, bs, "%s/acta_conf_conf", tmpdir);
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

/* Write a config file with the contract mode 0600 (work item 5).
 * The permission gate refuses group/other-readable files, so a
 * default-umask (0644) file would be refused in every test that needs
 * the file to be readable. */
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

/* Write a config file with an explicit (non-0600) mode, to exercise
 * the bad-permissions refusal.  POSIX-only: its only callers are in the
 * POSIX-only section 5, so it is not compiled on Windows (where the
 * mode bits are meaningless and the gate only warns). */
#ifndef _WIN32
static int write_conf_mode(const char *path, const char *content,
                           int mode)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
    if (chmod(path, mode) != 0)
        return 0;
    return 1;
}
#endif

static int run_stub(db_t *db, int id)
{
    char idstr[16];
    snprintf(idstr, sizeof idstr, "%d", id);
    char *av[] = { idstr };
    return cmd_run_argv(db, 1, av);
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
    char base[512], confpath[700];
    if (!make_conf_locations(tmpdir, base, sizeof base,
                             confpath, sizeof confpath)) {
        fprintf(stderr, "SKIP: cannot create config locations\n");
        return 0;
    }
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

    /* 1. Missing file + env unset -> the EXISTING hard error
     *    (EXIT_INVALID) before any claim; row stays pending.
     *    No HTTP needed: the policy fires before the claim. */
    {
        printf("== cmd_run: missing file + OPENAI_API_KEY unset\n");
        check(env_force("OPENAI_API_KEY", ENV_UNSET, NULL),
              "variable removed");
        remove(confpath); /* ensure no file in the scratch dir */
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");
        int rc = run_stub(db, id);
        check(rc == EXIT_INVALID,
              "exit code 4 (no key anywhere -> hard error)");
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
              "row NOT claimed (still pending)");
    }

    /* 2. File fallback: env unset, file "api_key" -> completes and the
     *    Bearer header carries the FILE key. */
    {
        printf("== cmd_run: file fallback (env unset)\n");
        check(env_force("OPENAI_API_KEY", ENV_UNSET, NULL),
              "variable removed");
        check(write_conf(confpath,
                         "{\"api_key\": \"file-key-42\"}"),
              "wrote conf file with api_key (0600)");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            check(port_reachable(STUB_PORT), "stub port reachable");
            int rc = run_stub(db, id);
            check(rc == EXIT_OK, "exit code 0 (file key accepted)");
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check_str(stub_server_last_auth(), "Bearer file-key-42",
                      "Authorization header carries the FILE key");
            stub_server_stop();
        }
    }

    /* 3. Env-set-wins: env set AND file "api_key" present -> the Bearer
     *    header carries the ENV key, not the file key. */
    {
        printf("== cmd_run: env-set-wins (env set, file key present)\n");
        check(env_force("OPENAI_API_KEY", ENV_VALUE, "env-key-1"),
              "variable set to non-empty value");
        check(write_conf(confpath,
                         "{\"api_key\": \"file-key-42\"}"),
              "wrote conf file with api_key (0600)");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            check(port_reachable(STUB_PORT), "stub port reachable");
            int rc = run_stub(db, id);
            check(rc == EXIT_OK, "exit code 0 (key present)");
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check_str(stub_server_last_auth(), "Bearer env-key-1",
                      "Authorization header carries the ENV key (file "
                      "key ignored)");
            stub_server_stop();
        }
        env_force("OPENAI_API_KEY", ENV_UNSET, NULL);
    }

    /* 4. (POSIX) env set to EMPTY still wins: warning only, run
     *    completes WITHOUT an Authorization header — the file key is
     *    never consulted as a channel.  Not producible via _putenv on
     *    Windows: skipped there (the EMPTY_WARN branch of
     *    acta_conf_api_key_status is unit-tested in the conf suite). */
#ifndef _WIN32
    {
        printf("== cmd_run: empty env var wins over file key\n");
        check(env_force("OPENAI_API_KEY", ENV_EMPTY, NULL),
              "variable set to empty string");
        check(write_conf(confpath,
                         "{\"api_key\": \"file-key-42\"}"),
              "wrote conf file with api_key (0600)");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            check(port_reachable(STUB_PORT), "stub port reachable");
            int rc = run_stub(db, id);
            check(rc == EXIT_OK,
                  "exit code 0 (empty -> warning only, run proceeds)");
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            check(stub_server_last_auth() == NULL,
                  "NO Authorization header sent (empty env var wins, "
                  "file key not used)");
            stub_server_stop();
        }
        env_force("OPENAI_API_KEY", ENV_UNSET, NULL);
    }
#endif

    /* 5. Bad-permissions refusal: a readable file whose mode gives
     *    group/other read access is refused fail-closed BEFORE its
     *    contents are read -> EXIT_INVALID, row stays pending.
     *    POSIX-only: on Windows the mode bits are meaningless (always
     *    0666) and the gate warns and reads the file anyway
     *    (best-effort), so the refusal cannot be exercised there. */
#ifndef _WIN32
    {
        printf("== cmd_run: bad-permissions file (0644) refused\n");
        check(env_force("OPENAI_API_KEY", ENV_UNSET, NULL),
              "variable removed");
        check(write_conf_mode(confpath,
                              "{\"api_key\": \"file-key-42\"}", 0644),
              "wrote conf file with api_key (0644)");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");
        int rc = run_stub(db, id);
        check(rc == EXIT_INVALID,
              "exit code 4 (group-readable conf -> fail-closed hard error)");
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
              "row NOT claimed (still pending)");
        /* The gate is evaluated before the contents are read: a
         * group-readable file that would parse fine must still be
         * refused. */
        check(write_conf_mode(confpath, "not json at all", 0644),
              "rewrote wrong-mode file with malformed contents");
        rc = run_stub(db, id);
        check(rc == EXIT_INVALID,
              "malformed-but-wrong-mode file still refused (gate before "
              "parse)");
        remove(confpath);
    }
#endif

    /* 6. Readable-but-malformed file -> fail-closed hard error before
     *    any claim; row stays pending. */
    {
        printf("== cmd_run: malformed conf file\n");
        check(env_force("OPENAI_API_KEY", ENV_UNSET, NULL),
              "variable removed");
        check(write_conf(confpath, "{nope"),
              "wrote malformed conf file (0600)");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");
        int rc = run_stub(db, id);
        check(rc == EXIT_INVALID,
              "exit code 4 (malformed conf -> fail-closed hard error)");
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
              "row NOT claimed (still pending)");
        /* Unknown key and wrong type are the same fail-closed class. */
        check(write_conf(confpath, "{\"api_keys\": \"x\"}"),
              "wrote unknown-key conf file (0600)");
        rc = run_stub(db, id);
        check(rc == EXIT_INVALID, "unknown-key conf -> hard error");
        check(write_conf(confpath, "{\"api_key\": 42}"),
              "wrote wrong-type conf file (0600)");
        rc = run_stub(db, id);
        check(rc == EXIT_INVALID, "wrong-type conf -> hard error");
        remove(confpath);
    }

    /* 7. (unit) max_chars / timeout file-over-builtin resolution order
     *    (work item 4 helpers; the file supplies the default, the
     *    --timeout flag is the per-run override). */
    {
        printf("== unit: acta_conf_resolve_max_chars / _timeout\n");
        acta_conf_t conf;
        memset(&conf, 0, sizeof conf);

        /* Absent (zeroed / NULL conf) -> built-in defaults. */
        check(acta_conf_resolve_max_chars(&conf) ==
                  ACTA_CONF_DEFAULT_MAX_CHARS,
              "absent max_chars -> built-in default (100000)");
        check(acta_conf_resolve_max_chars(NULL) ==
                  ACTA_CONF_DEFAULT_MAX_CHARS,
              "NULL conf -> built-in max_chars default");
        check(acta_conf_resolve_timeout(&conf, 0) ==
                  ACTA_CONF_DEFAULT_TIMEOUT,
              "absent timeout, no flag -> built-in default (600)");

        /* File value wins over the built-in default. */
        conf.max_chars = 42;
        conf.timeout = 60;
        check(acta_conf_resolve_max_chars(&conf) == 42,
              "file max_chars -> 42 (file over builtin)");
        check(acta_conf_resolve_timeout(&conf, 0) == 60,
              "file timeout, no flag -> 60 (file over builtin)");

        /* The --timeout flag wins over the file value. */
        check(acta_conf_resolve_timeout(&conf, 120) == 120,
              "flag timeout -> 120 (flag over file)");
        check(acta_conf_resolve_timeout(&conf, 0) == 60,
              "no flag -> file timeout again");

        acta_conf_free(&conf);
    }

    /* clean up */
    remove(confpath);
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
