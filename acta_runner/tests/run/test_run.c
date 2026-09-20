/*
 * test_run.c — phase-2 pipeline tests against the in-process stub
 * OpenAI-compatible server (tests/stub_server.c).
 *
 * Covers (docs/runner_contract.md, "Code shape in acta_runner/"):
 *   1. success            -> completed + full phase log
 *   2. /health 503        -> failed ("model still loading") + EXIT_HTTP
 *   3. model mismatch     -> failed + EXIT_HTTP
 *   4. chat 500           -> failed + EXIT_HTTP
 *   5. timeout            -> failed + EXIT_TIMEOUT
 *   6. non-pending row    -> EXIT_INVALID, row untouched
 *   7. not found          -> EXIT_NOT_FOUND
 *   8. schema validation  -> failed + EXIT_INVALID (post-hoc path)
 *   9. schema validation  -> completed (valid JSON against schema)
 *   10. missing catalog   -> completed, preflight_passed records
 *                              "catalog":null (non-llama backend)
 *   11. unknown config key -> failed + EXIT_INVALID
 *   12. malformed config JSON -> failed + EXIT_INVALID
 *   13. wrong config key type -> failed + EXIT_INVALID
 *   14. empty context content -> failed + EXIT_INVALID
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

/* ── DB setup ─────────────────────────────────────────────────────── */

/* Load acta_gui/db/schema.sql from a couple of likely locations. */
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

/* Create context + skill (+ revision) + model (+ revision); returns 0 on
 * success, -1 on failure. */
static int seed(db_t *db, int *ctx_id, int *skill_rev_id,
                int *model_rev_id, const char *output_schema,
                const char *model_config, const char *ctx_content)
{
    int err = ACTA_DB_OK;

    /* Unique names per call: the schema has unique indexes on
     * skills.name / models.name (where folder_id IS NULL), so reusing
     * the same name across seeds would violate uq_skills_root /
     * uq_models_root. */
    static int seed_calls = 0;
    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "test-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "test-model-%d", seed_calls);

    context_t c;
    memset(&c, 0, sizeof c);
    c.type = "text";
    c.content = (char *)ctx_content;
    /* Required non-NULL by acta_db_context_create; the runner pipeline
     * never verifies the hash, so a fixed placeholder is fine here. */
    c.content_hash = "test-hash";
    if (acta_db_context_create(db, &c, ctx_id) != ACTA_DB_OK)
        return -1;

    skill_t s;
    memset(&s, 0, sizeof s);
    s.name = skill_name;
    s.prompt_template = "SYS-TEMPLATE";
    s.output_schema = (char *)output_schema;
    int skill_id = 0;
    if (acta_db_skill_create(db, &s, &skill_id) != ACTA_DB_OK)
        return -1;

    skill_revision_t *sr = acta_db_skill_revision_get_latest(db, skill_id, &err);
    if (!sr)
        return -1;
    *skill_rev_id = sr->id;
    acta_db_skill_revision_free(sr);

    model_t m;
    memset(&m, 0, sizeof m);
    m.name = model_name;
    m.backend = "llama";
    m.base_url = STUB_BASE_URL;
    m.model_identifier = "stub-model";
    m.configuration = (char *)model_config;
    int model_id = 0;
    if (acta_db_model_create(db, &m, &model_id) != ACTA_DB_OK)
        return -1;

    model_revision_t *mr = acta_db_model_revision_get_latest(db, model_id, &err);
    if (!mr)
        return -1;
    *model_rev_id = mr->id;
    acta_db_model_revision_free(mr);
    return 0;
}

static int make_execution(db_t *db, int ctx_id, int skill_rev_id,
                          int model_rev_id)
{
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

/* True when some log row with the given event has metadata containing
 * substr. */
static int log_metadata_contains(db_t *db, int exec_id, const char *event,
                                const char *substr)
{
    int err = ACTA_DB_OK, n = 0;
    execution_log_t **rows = acta_db_execution_log_list_by_execution(
        db, exec_id, NULL, 0, ACTA_DB_MAX_PAGE, &n, &err);
    if (err != ACTA_DB_OK || !rows)
        return 0;
    int found = 0;
    for (int i = 0; i < n; i++)
        if (rows[i]->event && strcmp(rows[i]->event, event) == 0 &&
            rows[i]->metadata &&
            strstr(rows[i]->metadata, substr) != NULL) {
            found = 1;
            break;
        }
    acta_db_execution_log_list_free(rows, n);
    return found;
}

/*
 * One scenario: start the stub, seed a fresh pending execution, run it,
 * verify exit code + row state + log rows, stop the stub. Returns the
 * execution id, or -1 if setup failed.
 */
static int scenario(const char *name, db_t *db, const stub_config_t *cfg,
                     const char *output_schema,
                     const char *model_config,
                     const char *ctx_content,
                     int timeout_sec, int expect_exit,
                     const char *expect_status,
                     const char *expect_raw,
                     const char *expect_err_substr,
                     const char *const *expect_events, size_t n_events)
{
    printf("== %s\n", name);

    int ctx = 0, skr = 0, mkr = 0;
    if (seed(db, &ctx, &skr, &mkr, output_schema, model_config,
             ctx_content) != 0) {
        check(0, "seed");
        return -1;
    }
    int id = make_execution(db, ctx, skr, mkr);
    if (id < 0) {
        check(0, "make_execution");
        return -1;
    }

    if (stub_server_start(cfg) != 0) {
        check(0, "stub server start");
        return -1;
    }

    int rc = run_execution(db, id, timeout_sec, NULL);
    check(rc == expect_exit, "exit code");

    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, id, &err);
    check(e != NULL, "execution row present");
    if (e) {
        check(e->status && strcmp(e->status, expect_status) == 0,
              "execution status");
        if (expect_raw)
            check(e->raw_response && strcmp(e->raw_response, expect_raw) == 0,
                  "raw_response stored");
        if (expect_err_substr)
            check(e->error && e->error[0] &&
                      strstr(e->error, expect_err_substr) != NULL,
                  "error message contains expected substring");
        for (size_t i = 0; i < n_events; i++)
            check(log_has_event(db, id, expect_events[i]),
                  expect_events[i]);
        acta_db_execution_free(e);
    }

    stub_server_stop();
    return id;
}

/* ── scenarios ────────────────────────────────────────────────────── */

static const char *EVT_FULL_SUCCESS[] = {
    "execution_started", "context_loaded", "prompt_resolved",
    "preflight_passed", "llm_request", "llm_response",
    "execution_completed",
};

/* main.c owns runner_gopts in the real binary; the test binary links
 * run.c without main.c, so the definition lives here. */
const global_opts_t *runner_gopts;

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

    /* 1. success */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        int sid = scenario("success", db, &cfg, NULL, NULL,
                           "CTX-CONTENT", 30, EXIT_OK,
                           ACTA_EXEC_STATUS_COMPLETED, "stub-response", NULL,
                           EVT_FULL_SUCCESS, sizeof(EVT_FULL_SUCCESS) /
                           sizeof(EVT_FULL_SUCCESS[0]));
        if (sid > 0) {
            check(log_metadata_contains(db, sid, "prompt_resolved",
                                       "\"system\":\"SYS-TEMPLATE\""),
                  "prompt_resolved metadata carries resolved system");
            check(log_metadata_contains(
                      db, sid, "prompt_resolved",
                      "\"user\":\"CTX-CONTENT\""),
                  "prompt_resolved metadata carries resolved user");
            check(log_metadata_contains(db, sid, "preflight_passed",
                                       "\"n_ctx\":162048"),
                  "preflight_passed metadata carries catalog n_ctx");
            check(log_metadata_contains(db, sid, "preflight_passed",
                                       "\"args\":[\"llama-server\""),
                  "preflight_passed metadata carries launch args");
        }
    }

    /* 2. health 503 -> model still loading */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 503;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        scenario("health 503", db, &cfg, NULL, NULL, "CTX-CONTENT",
                 30, EXIT_HTTP,
                 ACTA_EXEC_STATUS_FAILED, NULL, "still loading", NULL, 0);
    }

    /* 3. model mismatch */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "other-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        const char *events[] = { "execution_failed" };
        scenario("model mismatch", db, &cfg, NULL, NULL, "CTX-CONTENT",
                 30, EXIT_HTTP,
                 ACTA_EXEC_STATUS_FAILED, NULL, "not served by server",
                 events, 1);
    }

    /* 4. chat 500 */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 500;
        cfg.chat_error = "boom";
        cfg.chat_content = "stub-response";
        const char *events[] = { "llm_request", "execution_failed" };
        scenario("chat 500", db, &cfg, NULL, NULL, "CTX-CONTENT", 30,
                 EXIT_HTTP,
                 ACTA_EXEC_STATUS_FAILED, NULL, "500",
                 events, 2);
    }

    /* 5. timeout */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        cfg.delay_ms = 2500;
        scenario("timeout", db, &cfg, NULL, NULL, "CTX-CONTENT",
                 1, EXIT_TIMEOUT,
                 ACTA_EXEC_STATUS_FAILED, NULL, "timed out", NULL, 0);
    }

    /* 6. non-pending row: run once (success), then run again */
    {
        printf("== non-pending row\n");
        int ctx = 0, skr = 0, mkr = 0;
        if (seed(db, &ctx, &skr, &mkr, NULL, NULL, "CTX-CONTENT") != 0) {
            check(0, "seed");
        } else {
            int id = make_execution(db, ctx, skr, mkr);
            stub_config_t cfg;
            memset(&cfg, 0, sizeof cfg);
            cfg.port = STUB_PORT;
            cfg.health_status = 200;
            cfg.model_id = "stub-model";
            cfg.chat_status = 200;
            cfg.chat_content = "stub-response";
            if (id > 0 && stub_server_start(&cfg) == 0) {
                int rc1 = run_execution(db, id, 30, NULL);
                check(rc1 == EXIT_OK, "first run succeeds");
                int rc2 = run_execution(db, id, 30, NULL);
                check(rc2 == EXIT_INVALID, "second run rejected (not pending)");
            }
            stub_server_stop();
        }
    }

    /* 7. not found */
    {
        printf("== not found\n");
        int rc = run_execution(db, 999999, 30, NULL);
        check(rc == EXIT_NOT_FOUND, "unknown id -> EXIT_NOT_FOUND");
    }

    /* 8. post-hoc schema validation failure
     *    (supports_response_format=false, response not JSON) */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "not-json-at-all";
        const char *schema =
            "{\"type\":\"object\",\"required\":[\"answer\"],"
            "\"properties\":{\"answer\":{\"type\":\"string\"}}}";
        const char *model_config = "{\"supports_response_format\":false}";
        const char *events[] = {
            "llm_response", "validation_started", "validation_failed",
            "execution_failed",
        };
        scenario("schema validation failure", db, &cfg, schema,
                 model_config, "CTX-CONTENT", 30, EXIT_INVALID,
                 ACTA_EXEC_STATUS_FAILED, "not-json-at-all",
                 "validation failed", events, 4);
    }

    /* 9. post-hoc schema validation success */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "{\"answer\":\"ok\"}";
        const char *schema =
            "{\"type\":\"object\",\"required\":[\"answer\"],"
            "\"properties\":{\"answer\":{\"type\":\"string\"}}}";
        const char *model_config = "{\"supports_response_format\":false}";
        const char *events[] = { "validation_started", "execution_completed" };
        scenario("schema validation success", db, &cfg, schema,
                 model_config, "CTX-CONTENT", 30, EXIT_OK,
                 ACTA_EXEC_STATUS_COMPLETED, "{\"answer\":\"ok\"}", NULL,
                 events, 2);
    }

    /* 10. missing catalog (non-llama backend): the run still completes;
     *     preflight_passed records "catalog":null */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        cfg.catalog_status = 404;
        const char *events[] = { "preflight_passed" };
        int cid = scenario("missing catalog", db, &cfg, NULL, NULL,
                           "CTX-CONTENT", 30,
                           EXIT_OK,
                           ACTA_EXEC_STATUS_COMPLETED, "stub-response", NULL,
                           events, 1);
        if (cid > 0) {
            check(log_metadata_contains(db, cid, "preflight_passed",
                                       "\"catalog\":null"),
                  "preflight_passed metadata holds null catalog");
        }
    }

    /* 11. unknown configuration key (typo) -> hard failure */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        const char *events[] = { "execution_failed" };
        scenario("unknown config key", db, &cfg, NULL,
                 "{\"temperature\":0.7,\"temperatue\":1}",
                 "CTX-CONTENT", 30, EXIT_INVALID,
                 ACTA_EXEC_STATUS_FAILED, NULL,
                 "unknown model configuration keys", events, 1);
    }

    /* 12. malformed configuration JSON -> hard failure */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        const char *events[] = { "execution_failed" };
        scenario("malformed config JSON", db, &cfg, NULL,
                 "{\"temperature\":0.7", "CTX-CONTENT", 30, EXIT_INVALID,
                 ACTA_EXEC_STATUS_FAILED, NULL,
                 "not valid JSON", events, 1);
    }

    /* 13. wrong configuration key type -> hard failure */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        const char *events[] = { "execution_failed" };
        scenario("wrong config key type", db, &cfg, NULL,
                 "{\"temperature\":\"high\"}", "CTX-CONTENT", 30,
                 EXIT_INVALID,
                 ACTA_EXEC_STATUS_FAILED, NULL,
                 "must be a number", events, 1);
    }

    /* 14. configuration carrying api_key -> hard failure: the key is
     *     no longer read from the blob, so a stored secret is now an
     *     unknown key (docs/plans/drop-model-config-api-key.md). */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        const char *events[] = { "execution_failed" };
        scenario("config api_key is an unknown key", db, &cfg, NULL,
                 "{\"api_key\":\"secret\",\"temperature\":0.7}",
                 "CTX-CONTENT", 30, EXIT_INVALID,
                 ACTA_EXEC_STATUS_FAILED, NULL,
                 "unknown model configuration keys", events, 1);
    }

    /* 15. empty context content -> failed + EXIT_INVALID */
    {
        stub_config_t cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.port = STUB_PORT;
        cfg.health_status = 200;
        cfg.model_id = "stub-model";
        cfg.chat_status = 200;
        cfg.chat_content = "stub-response";
        const char *events[] = { "execution_failed" };
        scenario("empty context content", db, &cfg, NULL, NULL,
                 "", 30, EXIT_INVALID,
                 ACTA_EXEC_STATUS_FAILED, NULL, "empty context content",
                 events, 1);
    }

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
