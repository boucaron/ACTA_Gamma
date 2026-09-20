/*
 * test_sweep.c — R4: `sweep --stale-seconds` stale-`running` cleanup.
 *
 * Covers (docs/runner_contract.md, "R4 — Stale-`running` cleanup sweep"):
 *   1. no running rows → clean exit 0.
 *   2. fresh `running` row (started_at + latest log ≈ now) → left
 *      running, exit 0.
 *   3. stale `running` row (started_at + logs ≈ now − 2h) → failed
 *      with the "stale running" error + execution_failed log row,
 *      exit 0.
 *   4. mixed (one stale, one fresh) → only the stale row failed.
 *   5. orphan: stale started_at and NO log rows → swept (fallback).
 *   6. newest-wins: fresh started_at but an old log row → last
 *      activity is the max, so the row is kept.
 *   7. `--stale-seconds 0` → rejected (EXIT_INVALID): a zero-second
 *      sweep would fail every running row.
 *   8. missing `--stale-seconds` → EXIT_INVALID.
 *   9. `--stale-seconds` non-numeric → EXIT_INVALID.
 *  10. inline `--stale-seconds=<n>` form works.
 *  11. page cap (known issue 10): with more log rows than
 *      ACTA_DB_MAX_PAGE, the newest row BEYOND the first page must be
 *      what last_activity() sees; a fresh row is kept, not swept.
 *
 * Same harness style as test_run.c / test_pending.c: scratch `:memory:`
 * DB seeded from `acta_gui/db/schema.sql`. No stub server — sweep is
 * pure DB + time; timestamps are set directly by UPDATE so no sleeping
 * is needed. `cmd_sweep` is called directly with a constructed argv
 * (no process spawn).
 *
 * Run: `make test` in acta_runner/.
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "runner.h"
#include "runner_util.h"
#include "argparse.h"
#include "acta_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
 * sweep.c without main.c, so the definition lives here. */
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

/* UTC timestamp in the SQLite CURRENT_TIMESTAMP format
 * ("YYYY-MM-DD HH:MM:SS") — the same lexicographically-ordered string
 * the schema stores. */
static void ts(time_t t, char *out, size_t outsz)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(out, outsz, "%Y-%m-%d %H:%M:%S", &tm);
}

static time_t now_t(void) { return time(NULL); }

/* Seed one pending execution (context + skill + model revisions) and
 * claim it with start() so it sits in `running` with started_at = now
 * and one execution_started log row. Returns the execution id or -1. */
static int seed_running(db_t *db)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "sweep-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "sweep-model-%d", seed_calls);

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
    m.base_url = "http://127.0.0.1:1";   /* unused: no HTTP in sweep */
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

    if (acta_db_execution_start(db, id) != ACTA_DB_OK)
        return -1;

    execution_log_t log;
    memset(&log, 0, sizeof log);
    log.execution_id = id;
    log.level = (char *)ACTA_LOG_LEVEL_INFO;
    log.event = (char *)"execution_started";
    log.message = (char *)"execution claimed (pending -> running)";
    if (acta_db_execution_log_create(db, &log, NULL) != ACTA_DB_OK)
        return -1;

    return id;
}

/* Backdate the row's started_at (dead-runner simulation). */
static int set_started_at(db_t *db, int id, time_t t)
{
    char out[32], sql[160];
    ts(t, out, sizeof out);
    snprintf(sql, sizeof sql,
             "UPDATE executions SET started_at = '%s' WHERE id = %d;",
             out, id);
    return acta_db_exec(db, sql);
}

/* Backdate every log row of the execution (dead-runner simulation). */
static int set_log_ts(db_t *db, int id, time_t t)
{
    char out[32], sql[160];
    ts(t, out, sizeof out);
    snprintf(sql, sizeof sql,
             "UPDATE execution_logs SET created_at = '%s' "
             "WHERE execution_id = %d;",
             out, id);
    return acta_db_exec(db, sql);
}

/* Delete every log row of the execution (orphan simulation). */
static int delete_logs(db_t *db, int id)
{
    char sql[96];
    snprintf(sql, sizeof sql,
             "DELETE FROM execution_logs WHERE execution_id = %d;", id);
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

static int execution_error(db_t *db, int id, const char *needle)
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

/* Run cmd_sweep with a constructed argv (action token excluded, as
 * main.c passes gopts.argv + 1). */
static int cmd_sweep_argv(db_t *db, int argc, char **argv)
{
    cmd_args_t ga;
    cmd_args_init(&ga, argc, argv);
    if (cmd_args_validate(&ga) != EXIT_OK)
        return EXIT_INVALID;
    return cmd_sweep(&ga, runner_gopts, db);
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

    char st[32];
    time_t now = now_t();

    /* 1. no running rows → clean exit 0 */
    {
        printf("== sweep: no running rows\n");
        check(execution_status(db, 1, st, sizeof st) == 0, "db is empty");
        char *av[] = { "--stale-seconds", "600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
    }

    /* 2. fresh running row → left running */
    {
        printf("== sweep: fresh running row\n");
        int id = seed_running(db);
        check(id > 0, "seeded fresh running row");

        char *av[] = { "--stale-seconds", "600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_RUNNING) == 0,
              "fresh row still running");
        check(!log_has_event(db, id, "execution_failed"),
              "no execution_failed log added");
    }

    /* 3. stale running row → failed with the stale error */
    {
        printf("== sweep: stale running row (now - 2h)\n");
        int id = seed_running(db);
        check(id > 0, "seeded running row");
        check(set_started_at(db, id, now - 2 * 3600) == ACTA_DB_OK &&
                  set_log_ts(db, id, now - 2 * 3600) == ACTA_DB_OK,
              "backdated started_at + logs to now - 2h");

        char *av[] = { "--stale-seconds", "600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_FAILED) == 0,
              "stale row failed");
        check(execution_error(db, id, "stale running: no runner activity"),
              "error names the stale-running cause");
        check(log_has_event(db, id, "execution_failed"),
              "execution_failed log row present");
    }

    /* 4. mixed: only the stale row is swept */
    {
        printf("== sweep: mixed (stale + fresh)\n");
        int id_stale = seed_running(db);
        int id_fresh = seed_running(db);
        check(id_stale > 0 && id_fresh > 0, "seeded 2 running rows");
        check(set_started_at(db, id_stale, now - 3 * 3600) == ACTA_DB_OK &&
                  set_log_ts(db, id_stale, now - 3 * 3600) == ACTA_DB_OK,
              "backdated one row to now - 3h");

        char *av[] = { "--stale-seconds", "3600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id_stale, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_FAILED) == 0,
              "stale row failed");
        check(execution_status(db, id_fresh, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_RUNNING) == 0,
              "fresh row still running");
    }

    /* 5. orphan: stale started_at, no log rows → swept via fallback */
    {
        printf("== sweep: orphan (no logs, stale started_at)\n");
        int id = seed_running(db);
        check(id > 0, "seeded running row");
        check(set_started_at(db, id, now - 2 * 3600) == ACTA_DB_OK &&
                  delete_logs(db, id) == ACTA_DB_OK,
              "backdated started_at and deleted logs");

        char *av[] = { "--stale-seconds", "600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_FAILED) == 0,
              "orphan row failed (started_at fallback)");
        check(execution_error(db, id, "stale running: no runner activity"),
              "error names the stale-running cause");
    }

    /* 6. newest-wins: fresh started_at, old log → kept */
    {
        printf("== sweep: newest-wins (fresh started_at, old log)\n");
        int id = seed_running(db);
        check(id > 0, "seeded running row");
        check(set_log_ts(db, id, now - 2 * 3600) == ACTA_DB_OK,
              "backdated only the log row");

        char *av[] = { "--stale-seconds", "600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_RUNNING) == 0,
              "row kept (max(log, started_at) is fresh)");
    }

    /* 7. --stale-seconds 0 → rejected */
    {
        printf("== sweep: --stale-seconds 0 rejected\n");
        int id = seed_running(db);
        check(id > 0, "seeded running row");
        char *av[] = { "--stale-seconds", "0" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_INVALID,
              "exit code 4 (invalid)");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_RUNNING) == 0,
              "row untouched");
    }

    /* 8. missing --stale-seconds → rejected */
    {
        printf("== sweep: missing --stale-seconds rejected\n");
        char *av[] = { };
        check(cmd_sweep_argv(db, 0, av) == EXIT_INVALID,
              "exit code 4 (invalid)");
    }

    /* 9. non-numeric --stale-seconds → rejected */
    {
        printf("== sweep: non-numeric --stale-seconds rejected\n");
        char *av[] = { "--stale-seconds", "abc" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_INVALID,
              "exit code 4 (invalid)");
        char *av2[] = { "--stale-seconds", "-5" };
        check(cmd_sweep_argv(db, 2, av2) == EXIT_INVALID,
              "negative rejected (exit code 4)");
    }

    /* 10. inline --stale-seconds=<n> form */
    {
        printf("== sweep: inline --stale-seconds=900\n");
        int id = seed_running(db);
        check(id > 0, "seeded running row");
        check(set_started_at(db, id, now - 2 * 3600) == ACTA_DB_OK &&
                  set_log_ts(db, id, now - 2 * 3600) == ACTA_DB_OK,
              "backdated to now - 2h");

        char *av[] = { "--stale-seconds=900" };
        check(cmd_sweep_argv(db, 1, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_FAILED) == 0,
              "stale row failed via inline form");
    }

    /* 11. page cap: > ACTA_DB_MAX_PAGE log rows — the newest row sits
     * beyond the first page, so a first-page-only read would miss it
     * and sweep a fresh row (known issue 10). */
    {
        printf("== sweep: log rows beyond ACTA_DB_MAX_PAGE\n");
        int id = seed_running(db);
        check(id > 0, "seeded running row");

        /* Push the row count past the cap, all backdated to now - 2h. */
        int created = 0;
        for (int i = 0; i < ACTA_DB_MAX_PAGE; i++) {
            execution_log_t log;
            memset(&log, 0, sizeof log);
            log.execution_id = id;
            log.level = (char *)ACTA_LOG_LEVEL_DEBUG;
            log.event = (char *)"bulk";
            log.message = (char *)"bulk row";
            if (acta_db_execution_log_create(db, &log, NULL) == ACTA_DB_OK)
                created++;
        }
        check(created == ACTA_DB_MAX_PAGE,
              "created ACTA_DB_MAX_PAGE extra log rows");
        check(set_log_ts(db, id, now - 2 * 3600) == ACTA_DB_OK,
              "backdated every log row to now - 2h");

        /* Bump ONLY the overall newest row (max id) to now: with the
         * (created_at, id) ASC order it is the last row of the LAST
         * page, beyond the first page's cap. */
        char fresh_ts[32];
        ts(now, fresh_ts, sizeof fresh_ts);
        char bump_sql[200];
        snprintf(bump_sql, sizeof bump_sql,
                 "UPDATE execution_logs SET created_at = '%s' "
                 "WHERE execution_id = %d AND id = "
                 "(SELECT MAX(id) FROM execution_logs);",
                 fresh_ts, id);
        check(acta_db_exec(db, bump_sql) == ACTA_DB_OK,
              "newest log row bumped to now");

        char *av[] = { "--stale-seconds", "600" };
        check(cmd_sweep_argv(db, 2, av) == EXIT_OK, "exit code 0");
        check(execution_status(db, id, st, sizeof st) &&
                  strcmp(st, ACTA_EXEC_STATUS_RUNNING) == 0,
              "fresh row kept (newest log beyond the page cap seen)");
        /* Exact count (1 seeded + ACTA_DB_MAX_PAGE bulk): a swept row
         * would have appended an execution_failed row beyond the first
         * page, which the first-page-only log_has_event() cannot see. */
        check(acta_db_execution_log_count(db, id, NULL, &err) ==
                  ACTA_DB_MAX_PAGE + 1,
              "log count unchanged (no execution_failed log added)");
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
