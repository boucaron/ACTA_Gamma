/*
 * test_pending.c — R3: `run --pending` batch looping and `--max` clamping.
 *
 * Covers (docs/runner_contract.md, "R3 — `--pending` batch and `--max`
 * clamping tests"):
 *   1. N pending rows → all run, all `completed`, one log sequence per
 *      row, exit 0.
 *   2. `--max M < N` → exactly M run (the first M by `id ASC`, the
 *      lister's order), the rest stay `pending`, exit 0; a follow-up
 *      unbounded batch then consumes the remainder.
 *   3. `--max 0` → no limit, all run (the lister clamps 0 to
 *      ACTA_DB_MAX_PAGE), exit 0.
 *   4. mixed outcomes → the failing row does not stop the batch:
 *      later rows are still processed and the process exits with the
 *      WORST exit code seen (12 HTTP/preflight, 13 timeout, 4
 *      claim/validation). The single stub server serves one
 *      configuration per scenario, so the failing row uses a model
 *      identifier mismatch (preflight `/v1/models` check) — same
 *      EXIT_HTTP class as the health-503 case.
 *   5. no pending rows → clean exit 0.
 *
 * Same harness as test_run.c: scratch `:memory:` DB seeded from
 * `acta_db/schema.sql` + in-process stub server. `cmd_run` is
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
 * "stub-model"; a different value makes preflight fail). */
static int seed_pending(db_t *db, const char *model_id)
{
    int err = ACTA_DB_OK;
    static int seed_calls = 0;

    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "pend-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "pend-model-%d", seed_calls);

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

    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.port = STUB_PORT;
    cfg.health_status = 200;
    cfg.model_id = "stub-model";
    cfg.chat_status = 200;
    cfg.chat_content = "stub-response";

    /* 1. N pending rows → all run, all completed, one log sequence each */
    {
        printf("== batch: 3 pending, no --max\n");
        int ids[3];
        int ok = 1;
        for (int i = 0; i < 3; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 3 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char *av[] = { "--pending" };
            int rc = cmd_run_argv(db, 1, av);
            check(rc == EXIT_OK, "exit code 0");
            for (int i = 0; i < 3; i++) {
                char st[32];
                check(execution_status(db, ids[i], st, sizeof st) &&
                          strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                      "row completed");
                check(log_has_event(db, ids[i], "execution_started") &&
                          log_has_event(db, ids[i], "llm_response") &&
                          log_has_event(db, ids[i], "execution_completed"),
                      "one log sequence per row");
            }
            check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 0,
                  "no pending rows left");
            stub_server_stop();
        }
    }

    /* 2. --max M < N → exactly M run (first M by id ASC), rest pending */
    {
        printf("== batch: 4 pending, --max 2\n");
        int ids[4];
        int ok = 1;
        for (int i = 0; i < 4; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 4 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char *av[] = { "--pending", "--max", "2" };
            int rc = cmd_run_argv(db, 3, av);
            check(rc == EXIT_OK, "exit code 0");

            char st[32];
            check(execution_status(db, ids[0], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "first row completed");
            check(execution_status(db, ids[1], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "second row completed");
            check(execution_status(db, ids[2], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
                  "third row still pending");
            check(execution_status(db, ids[3], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_PENDING) == 0,
                  "fourth row still pending");

            /* follow-up unbounded batch consumes the remainder */
            char *av2[] = { "--pending" };
            int rc2 = cmd_run_argv(db, 1, av2);
            check(rc2 == EXIT_OK, "follow-up batch exit code 0");
            check(execution_status(db, ids[2], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "third row completed by follow-up");
            check(execution_status(db, ids[3], st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "fourth row completed by follow-up");
            stub_server_stop();
        }
    }

    /* 3. --max 0 → no limit, all run */
    {
        printf("== batch: 3 pending, --max 0 (no limit)\n");
        int ids[3];
        int ok = 1;
        for (int i = 0; i < 3; i++) {
            ids[i] = seed_pending(db, "stub-model");
            ok = ok && ids[i] > 0;
        }
        check(ok, "seeded 3 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char *av[] = { "--pending", "--max", "0" };
            int rc = cmd_run_argv(db, 3, av);
            check(rc == EXIT_OK, "exit code 0");
            char st[32];
            int all_done = 1;
            for (int i = 0; i < 3; i++) {
                if (!execution_status(db, ids[i], st, sizeof st) ||
                    strcmp(st, ACTA_EXEC_STATUS_COMPLETED) != 0)
                    all_done = 0;
            }
            check(all_done, "all 3 completed (0 = no limit)");
            stub_server_stop();
        }
    }

    /* 4. mixed outcomes: failing row does not stop the batch; later
     *    rows are still processed; exit = worst exit code seen (12). */
    {
        printf("== batch: mixed outcomes (mismatch then success)\n");
        int id_bad = seed_pending(db, "wrong-model");  /* lower id first */
        int id_good = seed_pending(db, "stub-model");
        check(id_bad > 0 && id_good > 0, "seeded 2 pending");

        if (stub_server_start(&cfg) != 0) {
            check(0, "stub server start");
        } else {
            char *av[] = { "--pending" };
            int rc = cmd_run_argv(db, 1, av);
            check(rc == EXIT_HTTP,
                  "worst exit code wins (12 = HTTP/preflight)");
            check(execution_error(db, id_bad, "not served by server"),
                  "failing row failed with model-mismatch error");
            check(log_has_event(db, id_bad, "execution_failed"),
                  "failing row logged execution_failed");
            char st[32];
            check(execution_status(db, id_good, st, sizeof st) &&
                      strcmp(st, ACTA_EXEC_STATUS_COMPLETED) == 0,
                  "later row still processed (completed)");
            check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 0,
                  "no pending rows left");
            stub_server_stop();
        }
    }

    /* 5. no pending rows → clean exit 0 */
    {
        printf("== batch: no pending rows\n");
        check(count_status(db, ACTA_EXEC_STATUS_PENDING) == 0,
              "nothing pending");
        char *av[] = { "--pending" };
        int rc = cmd_run_argv(db, 1, av);
        check(rc == EXIT_OK, "exit code 0");
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
