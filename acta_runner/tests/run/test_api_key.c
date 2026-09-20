/*
 * test_api_key.c — API key presence policy
 * (docs/runner_contract.md, decision 4):
 *   1. $OPENAI_API_KEY UNSET -> cmd_run rejects with EXIT_INVALID (4)
 *      BEFORE any claim; the seeded row stays `pending`.
 *   2. $OPENAI_API_KEY set but EMPTY -> warning only (no error): the
 *      execution completes against the keyless stub server (no
 *      Authorization header is sent). (Skipped on Windows: _putenv
 *      cannot set an empty string; the KEY_EMPTY_WARN branch is still
 *      unit-tested and exercised on POSIX.)
 *   3. $OPENAI_API_KEY set to a non-empty value -> the execution
 *      completes (Bearer header sent).
 *   4. unit checks of runner_api_key_status / runner_api_key_message.
 *
 * Same harness as test_pending.c: scratch `:memory:` DB seeded from
 * `acta_gui/db/schema.sql` + in-process stub server. `cmd_run` is
 * called directly with a constructed argv (no process spawn).
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

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#define probe_close(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define probe_close(s) close(s)
#endif

#define STUB_PORT 8918
#define STUB_BASE_URL "http://127.0.0.1:8918"

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

/* ── DB setup (mirrors test_pending.c) ────────────────────────────── */

static int load_schema(db_t *db)
{
    static const char *paths[] = {
        "../../acta_gui/db/schema.sql",
        "../acta_gui/db/schema.sql",
        "acta_gui/db/schema.sql",
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
 * revision); returns the execution id or -1. `model_id` is the
 * model_identifier the model revision carries (the stub serves
 * "stub-model"). */
static int seed_pending(db_t *db, const char *model_id)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "key-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "key-model-%d", seed_calls);

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
 * POSIX setenv/unsetenv are exact. Observed MSVCRT/mingw _putenv
 * semantics: "name=value" sets; "name=" REMOVES the variable; a bare
 * "name" is a no-op. There is no way to set a variable to an EMPTY
 * string via _putenv, so ENV_EMPTY returns 0 on Windows and the
 * caller must skip that scenario (the KEY_EMPTY_WARN branch is still
 * unit-tested above, and exercised on POSIX).
 * Returns 1 when the requested state was produced. */
enum { ENV_VALUE, ENV_EMPTY, ENV_UNSET };
static int env_force(const char *name, int state, const char *value)
{
#ifdef _WIN32
    char buf[256];
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

/* Raw TCP connect to the stub port: proves the listener is actually
 * accepting before the run (a SO_REUSEADDR bind can succeed while the
 * port is unreachable, which would show up as a confusing
 * "transport failure" inside the pipeline). */
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

/* ── scenarios ────────────────────────────────────────────────────── */

int main(void)
{
    /* Verbose level 1 so VLOG action summaries go to stderr. */
    static const global_opts_t gopts = { NULL, 1, 0, 0, 0, NULL };
    runner_gopts = &gopts;

    /* 4. (unit) presence-policy helper */
    {
        printf("== unit: runner_api_key_status / runner_api_key_message\n");
        check(runner_api_key_status(NULL) == KEY_UNSET_ERR,
              "NULL (unset) -> KEY_UNSET_ERR");
        check(runner_api_key_status("") == KEY_EMPTY_WARN,
              "empty -> KEY_EMPTY_WARN");
        check(runner_api_key_status("k") == KEY_OK,
              "non-empty -> KEY_OK");
        check(runner_api_key_message(KEY_UNSET_ERR) != NULL &&
                  strstr(runner_api_key_message(KEY_UNSET_ERR),
                         "OPENAI_API_KEY is not set") != NULL,
              "unset message names the variable");
        check(runner_api_key_message(KEY_EMPTY_WARN) != NULL &&
                  strstr(runner_api_key_message(KEY_EMPTY_WARN),
                         "no Authorization header") != NULL,
              "empty message warns about the missing header");
        check(runner_api_key_message(KEY_OK) == NULL,
              "OK -> no message");
    }

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

    /* 1. UNSET -> EXIT_INVALID before any claim; row stays pending.
     *    No HTTP needed: the policy fires before the claim. (The
     *    variable is removed explicitly so an inherited shell value
     *    cannot leak in. */
    {
        printf("== cmd_run: OPENAI_API_KEY unset\n");
        check(env_force("OPENAI_API_KEY", ENV_UNSET, NULL),
              "variable removed");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");

        char idstr[16];
        snprintf(idstr, sizeof idstr, "%d", id);
        char *av[] = { idstr };
        int rc = cmd_run_argv(db, 1, av);
        check(rc == EXIT_INVALID,
              "exit code 4 (unset -> hard error)");
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
              "row NOT claimed (still pending)");
    }

    /* 2. EMPTY -> warning only; the run completes against the keyless
     *    stub server (no Authorization header sent). Not producible via
     *    _putenv on Windows: skipped there (the branch is unit-tested
     *    above and exercised on POSIX). */
    {
        printf("== cmd_run: OPENAI_API_KEY empty\n");
        if (!env_force("OPENAI_API_KEY", ENV_EMPTY, NULL)) {
            check(1, "skipped: empty value not producible on Windows");
        } else {
            int id = seed_pending(db, "stub-model");
            check(id > 0, "seeded 1 pending");

            if (stub_server_start(&cfg) != 0) {
                check(0, "stub server start");
            } else {
                check(port_reachable(STUB_PORT), "stub port reachable");
                char idstr[16];
                snprintf(idstr, sizeof idstr, "%d", id);
                char *av[] = { idstr };
                int rc = cmd_run_argv(db, 1, av);
                check(rc == EXIT_OK,
                      "exit code 0 (empty -> warning only, run proceeds)");
                char st[32];
                check(execution_status(db, id, st, sizeof st) &&
                          strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                      "row completed");
                stub_server_stop();
            }
        }
    }

    /* 3. NON-EMPTY -> completes (Bearer header sent). */
    {
        printf("== cmd_run: OPENAI_API_KEY non-empty\n");
        check(env_force("OPENAI_API_KEY", ENV_VALUE, "stub-key"),
              "variable set to non-empty value");
        int id = seed_pending(db, "stub-model");
        check(id > 0, "seeded 1 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            check(port_reachable(STUB_PORT), "stub port reachable");
            char idstr[16];
            snprintf(idstr, sizeof idstr, "%d", id);
            char *av[] = { idstr };
            int rc = cmd_run_argv(db, 1, av);
            check(rc == EXIT_OK, "exit code 0 (key present)");
            char st[32];
            check(execution_status(db, id, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "row completed");
            stub_server_stop();
        }
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
