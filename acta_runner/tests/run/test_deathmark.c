/*
 * test_deathmark.c — death marker for orphaned `running` rows
 * (docs/plans/runner-ops-hardening.md, item 3).
 *
 * In-process, pure DB (no stub server, no backend sources): the plain
 * marker (deathmark_mark / deathmark_exit_path) is exercised directly
 * against a scratch `:memory:` DB seeded from `acta_db/schema.sql`:
 *   0. guards: zero id, unknown id, NULL db -> no-op.
 *   1. claimed `running` row -> the exit path marks it `failed` with
 *      the "runner process exited during execution" diagnostic + one
 *      error-level `execution_failed` log row.
 *   2. idempotent: a second exit-path invocation adds nothing.
 *   3. normal completion untouched: a `completed` row is not marked
 *      and gains no log row.
 *   4. released claim: after deathmark_release() the exit path no
 *      longer marks a still-running row.
 *
 * The signal wiring itself (SIGINT/SIGTERM -> exit() -> atexit ->
 * deathmark_exit_path) is deliberately thin and not tested in-process;
 * the e2e dead-runner suite (tests/run/test_deadrunner.c) covers the
 * real process loop.
 *
 * Run from acta_runner/: `make test`.
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "runner.h"
#include "deathmark.h"
#include "acta_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIAG "runner process exited during execution"

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
 * deathmark.c without main.c, so the definition lives here. */
const global_opts_t *runner_gopts;

/* ── DB setup ─────────────────────────────────────────────────────── */

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

/* Seed one pending execution (context + skill + model revisions).
 * Returns the execution id or -1. */
static int seed_pending(db_t *db)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "dm-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "dm-model-%d", seed_calls);

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
    m.base_url = "http://127.0.0.1:1";   /* unused: no HTTP here */
    m.model_identifier = "stub-model";
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

static int execution_error_contains(db_t *db, int id, const char *needle)
{
    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, id, &err);
    if (err != ACTA_DB_OK || !e)
        return 0;
    int found = e->error && e->error[0] && strstr(e->error, needle) != NULL;
    acta_db_execution_free(e);
    return found;
}

int main(void)
{
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

    /* 0. guards */
    {
        printf("== guards\n");
        check(deathmark_mark(db, 0) == 0, "zero id is a no-op");
        check(deathmark_mark(db, 987654321) == 0, "unknown id is a no-op");
        check(deathmark_mark(NULL, 0) == 0, "NULL db is a no-op");
    }

    /* 1. claimed `running` row -> exit path marks it failed */
    {
        printf("== exit path marks a still-running claimed row\n");
        int id = seed_pending(db);
        check(id > 0, "seeded 1 pending");
        check(acta_db_execution_start(db, id) == ACTA_DB_OK,
              "row claimed (pending -> running)");
        deathmark_claim(db, id);
        check(deathmark_mark(db, id) == 1,
              "mark acts on a still-running row");
        deathmark_exit_path();   /* the atexit body: the real exit path */
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_FAILED) == 0,
              "row failed");
        check(execution_error_contains(db, id, DIAG),
              "error names the death marker");
        int n = 0, lerr = ACTA_DB_OK;
        execution_log_t **rows =
            acta_db_execution_log_list_by_execution(db, id,
                                                    ACTA_LOG_LEVEL_ERROR,
                                                    0, 0, &n, &lerr);
        check(lerr == ACTA_DB_OK && n == 1 && rows &&
                  rows[0]->event &&
                  strcmp(rows[0]->event, "execution_failed") == 0,
              "one error-level execution_failed log row");
        acta_db_execution_log_list_free(rows, n);
    }

    /* 2. idempotent: a second exit-path invocation adds nothing */
    {
        printf("== idempotent\n");
        int id = seed_pending(db);
        check(id > 0, "seeded 1 pending");
        check(acta_db_execution_start(db, id) == ACTA_DB_OK,
              "row claimed (pending -> running)");
        deathmark_claim(db, id);
        deathmark_exit_path();
        deathmark_exit_path();   /* second invocation */
        int cnt = acta_db_execution_log_count(db, id,
                                              ACTA_LOG_LEVEL_ERROR, NULL);
        char st[32];
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_FAILED) == 0 && cnt == 1,
              "second exit-path call adds nothing");
    }

    /* 3. normal completion untouched: a `completed` row is not marked */
    {
        printf("== completed row untouched\n");
        int id = seed_pending(db);
        check(id > 0, "seeded 1 pending");
        check(acta_db_execution_start(db, id) == ACTA_DB_OK,
              "row claimed (pending -> running)");
        check(acta_db_execution_complete(db, id, "RESULT") == ACTA_DB_OK,
              "row completed");
        deathmark_claim(db, id);
        check(deathmark_mark(db, id) == 0, "completed row not marked");
        char st[32];
        int cnt = acta_db_execution_log_count(db, id,
                                              ACTA_LOG_LEVEL_ERROR, NULL);
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0 && cnt == 0,
              "status and log unchanged");
    }

    /* 4. released claim: the exit path no longer marks */
    {
        printf("== released claim\n");
        int id = seed_pending(db);
        check(id > 0, "seeded 1 pending");
        check(acta_db_execution_start(db, id) == ACTA_DB_OK,
              "row claimed (pending -> running)");
        deathmark_claim(db, id);
        deathmark_release();
        deathmark_exit_path();   /* nothing claimed anymore */
        char st[32];
        int cnt = acta_db_execution_log_count(db, id,
                                              ACTA_LOG_LEVEL_ERROR, NULL);
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_RUNNING) == 0 && cnt == 0,
              "row still running, no mark");
    }

    acta_db_close(db);
    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
