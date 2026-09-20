/*
 * test_deleted.c — soft-delete claim tests (acta_runner):
 *
 *   1. `run <id>` on a soft-deleted `pending` execution →
 *      EXIT_NOT_FOUND (1): the standard not-found path; the row is
 *      NOT claimed (stays `pending`, no `execution_started` log).
 *   2. `run --pending` with one live + one deleted pending row →
 *      only the live one runs (completed); the deleted row stays
 *      `pending` (the batch claim is live-only by default).
 *
 * Same harness as test_run.c / test_pending.c: scratch `:memory:` DB
 * seeded from `acta_gui/db/schema.sql` + in-process stub server.
 * `cmd_run` is called directly with a constructed argv (no process
 * spawn).
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

#define STUB_PORT 8917
#define STUB_BASE_URL "http://127.0.0.1:8917"

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

/* ── DB setup ─────────────────────────────────────────────────────── */

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

/* Seed one pending execution (context + skill + model, each with a
 * revision, model pointing at the stub); returns the execution id or
 * -1. */
static int seed_pending(db_t *db)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "del-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "del-model-%d", seed_calls);

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
    m.model_identifier = "stub-model";
    int model_row_id = 0;
    if (acta_db_model_create(db, &m, &model_row_id) != ACTA_DB_OK)
        return -1;

    model_revision_t *mr =
        acta_db_model_revision_get_latest(db, model_row_id, &err);
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

/* Soft-delete a row in place (simulates a prior `exec delete <id>`;
 * the runner must now treat the row as not found). */
static int mark_deleted(db_t *db, int id)
{
    char sql[64];
    snprintf(sql, sizeof sql,
             "UPDATE executions SET deleted_at = datetime('now') "
             "WHERE id = %d", id);
    return acta_db_exec(db, sql);
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

static int execution_deleted(db_t *db, int id)
{
    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, id, &err);
    if (err != ACTA_DB_OK || !e)
        return -1;
    int del = (e->deleted_at != NULL);
    acta_db_execution_free(e);
    return del;
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
     * (docs/runner_contract.md, decision 4). Use a non-empty value:
     * MSVCRT _putenv("name=") is not reliable for setting an empty
     * string (it can remove the variable), and the stub server
     * ignores the Authorization header, so any value works here. */
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

    /* 1. run <id> on a deleted pending execution → not found,
     *    the row is NOT claimed (no HTTP needed: the guard fires
     *    before any backend call). */
    {
        printf("== single claim: deleted pending row\n");
        int id = seed_pending(db);
        check(id > 0, "seeded 1 pending");
        check(mark_deleted(db, id) == ACTA_DB_OK, "row soft-deleted");

        char id_str[16];
        snprintf(id_str, sizeof id_str, "%d", id);
        char *av[] = { id_str };
        int rc = cmd_run_argv(db, 1, av);
        check(rc == EXIT_NOT_FOUND, "exit code 1 (standard not-found path)");
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
              "row still pending (not claimed)");
        check(execution_deleted(db, id) == 1, "row still deleted");
        check(log_has_event(db, id, "execution_started") == 0,
              "no execution_started log (nothing ran)");
    }

    /* 2. run --pending with one live + one deleted pending row →
     *    only the live one runs; the deleted row stays pending. */
    {
        printf("== batch: one live + one deleted pending\n");
        int id_live = seed_pending(db);
        int id_del = seed_pending(db);
        check(id_live > 0 && id_del > 0, "seeded 2 pending");
        check(mark_deleted(db, id_del) == ACTA_DB_OK,
              "one row soft-deleted");

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
            char *av[] = { "--pending" };
            int rc = cmd_run_argv(db, 1, av);
            check(rc == EXIT_OK, "exit code 0");
            char st[32];
            check(execution_status(db, id_live, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "live row completed");
            check(execution_status(db, id_del, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
                  "deleted row still pending (skipped by the batch claim)");
            check(execution_deleted(db, id_del) == 1,
                  "deleted row still deleted");
            stub_server_stop();
        }
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
