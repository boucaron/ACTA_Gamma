/*
 * test_openai_dialect.c — the `openai` dialect end-to-end against a stub
 * that SPEAKS the openai dialect (tests/stub_server.c cfg.dialect).
 *
 * What the llamacpp-only suites cannot prove: that the request body the
 * runner builds for a model revision registered backend:"openai" is the
 * one an OpenAI-compatible server accepts, and that the openai-only
 * omissions really happen. The stub is the second opinion: it rejects a
 * request whose response_format envelope is the other dialect's shape
 * (the failure docs/session_strata_backend.md issue A recorded live).
 *
 * Covers (docs/plans/backend_dialects.md §2 matrix, §6 acceptance
 * criteria 1-3, §9 decisions 3 and 4):
 *   1. backend:"openai" + an output_schema skill against an
 *      openai-dialect stub -> completed; the body carries the OpenAI
 *      envelope (json_schema name/strict/schema); preflight_passed
 *      records dialect=openai + "catalog":null + the ceiling read from
 *      entry meta.n_ctx; llm_request records
 *      dialect/dialect_source=model_revision/model_backend=openai;
 *      ZERO GET / catalog calls (decision 4)
 *   2. the same OpenAI envelope against a llama.cpp stub -> HTTP 400,
 *      EXIT_HTTP, row failed, the llama.cpp-shaped refusal recorded
 *   3. the llama.cpp envelope against an openai-dialect stub -> HTTP 400
 *      with Strata's real message
 *      "response_format.json_schema needs a schema object"
 *      (issue A, reproduced deterministically)
 *   4. model.configuration top_k is NOT sent on the openai dialect
 *      (decision 3) while temperature, configured in the same object,
 *      IS sent
 *   5. model.configuration top_k IS sent on the llamacpp dialect, which
 *      also makes exactly one GET / catalog call
 *   6. supports_response_format:false sends no envelope at all, which an
 *      openai-dialect server accepts (post-hoc validation path)
 *
 * Run from acta_runner/ (the suite loads ../acta_db/schema.sql relative
 * to the cwd). Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "runner.h"
#include "runner_util.h"
#include "argparse.h"
#include "acta_db.h"
#include "stub_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUB_PORT 8923
#define STUB_BASE_URL "http://127.0.0.1:8923"

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

/* Context + skill (+ revision) + model (+ revision) with an explicit
 * model backend — the dialect source (docs/plans/backend_dialects.md
 * §3.2). Names are unique per call (uq_models_root / uq_skills_root). */
static int seed_backend(db_t *db, int *ctx_id, int *skill_rev_id,
                        int *model_rev_id, const char *output_schema,
                        const char *model_config, const char *ctx_content,
                        const char *backend)
{
    int err = ACTA_DB_OK;

    static int seed_calls = 0;
    char skill_name[64], model_name[64];
    snprintf(skill_name, sizeof skill_name, "od-skill-%d", ++seed_calls);
    snprintf(model_name, sizeof model_name, "od-model-%d", seed_calls);

    context_t c;
    memset(&c, 0, sizeof c);
    c.type = "text";
    c.content = (char *)ctx_content;
    c.content_hash = "od-hash";
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
    m.backend = (char *)backend;
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

/* Same plain implementation as tests/run/test_run.c: list the execution's
 * log rows, strcmp the event, free the list. */
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

/* True when NO log row of this execution carries the given event. */
static int log_lacks_event(db_t *db, int exec_id, const char *event)
{
    return !log_has_event(db, exec_id, event);
}

/* Does the request body the stub last served contain `substr`? */
static int last_body_contains(const char *substr)
{
    const char *b = stub_server_last_chat_body();
    return b && strstr(b, substr) != NULL;
}

/*
 * One scenario: seed a model revision registered `backend`, start the
 * stub configured for stub_dialect, run the execution, verify the exit
 * code, the row, the log metadata and the stub's request counters.
 * Returns the execution id, or -1 when setup failed.
 */
static int scenario(const char *name, db_t *db, stub_config_t *cfg,
                    const char *model_backend, const char *stub_dialect,
                    const char *output_schema, const char *model_config,
                    const char *ctx_content, int expect_exit,
                    const char *expect_status, const char *expect_raw,
                    const char *expect_err_substr)
{
    printf("== %s\n", name);

    int ctx = 0, skr = 0, mkr = 0;
    if (seed_backend(db, &ctx, &skr, &mkr, output_schema, model_config,
                     ctx_content, model_backend) != 0) {
        check(0, "seed");
        return -1;
    }
    int id = make_execution(db, ctx, skr, mkr);
    if (id < 0) {
        check(0, "make_execution");
        return -1;
    }

    cfg->port = STUB_PORT;
    cfg->model_id = "stub-model";
    cfg->dialect = stub_dialect;
    if (stub_server_start(cfg) != 0) {
        check(0, "stub server start");
        return -1;
    }

    int rc = run_execution(db, id, 30, ACTA_CONF_DEFAULT_MAX_CHARS, NULL);
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
        acta_db_execution_free(e);
    }

    stub_server_stop();
    return id;
}

/* The output_schema every scenario here uses (the shape of the live
 * Strata sentiment probe in docs/session_strata_backend.md §2.1). */
#define OD_SCHEMA \
    "{\"type\":\"object\",\"required\":[\"label\",\"confidence\"]," \
    "\"properties\":{\"label\":{\"type\":\"string\"}," \
    "\"confidence\":{\"type\":\"number\"}}}"
#define OD_CONTENT "{\"label\":\"positive\",\"confidence\":0.95}"

/* ── scenarios ────────────────────────────────────────────────────── */

/* 1. openai model revision + output_schema skill against an
 *    openai-dialect server: completes, and every openai-dialect property
 *    of the run is recorded. */
static void s1_openai_end_to_end(db_t *db)
{
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.health_status = 200;
    cfg.chat_status = 200;
    cfg.chat_content = OD_CONTENT;
    /* The Strata shape: no top-level max_context on the /v1/models
     * entry, the ceiling lives in meta.n_ctx. */
    cfg.models_meta_n_ctx = 204800;

    int id = scenario("1 openai model + output_schema -> openai server",
                      db, &cfg, "openai", "openai", OD_SCHEMA, NULL,
                      "CTX-CONTENT", EXIT_OK, ACTA_EXEC_STATUS_COMPLETED,
                      OD_CONTENT, NULL);
    if (id < 0)
        return;

    check(stub_server_chat_requests() == 1, "exactly one chat request");
    check(stub_server_health_requests() == 1, "exactly one /health");
    check(stub_server_models_requests() == 1, "exactly one /v1/models");
    check(stub_server_catalog_requests() == 0,
          "no GET / catalog call (decision 4)");

    check(last_body_contains("\"response_format\""),
          "request carries response_format");
    check(last_body_contains("\"type\":\"json_schema\""),
          "envelope type is json_schema");
    check(last_body_contains("\"json_schema\":{\"name\":\"acta_output\","
                             "\"strict\":true,\"schema\":{"),
          "OpenAI envelope: json_schema{name,strict,schema}");

    check(log_metadata_contains(db, id, "preflight_passed",
                                "\"dialect\":\"openai\""),
          "preflight_passed dialect=openai");
    check(log_metadata_contains(db, id, "preflight_passed",
                                "\"catalog\":null"),
          "preflight_passed catalog null");
    check(log_metadata_contains(db, id, "preflight_passed",
                                "\"max_context\":204800"),
          "preflight_passed max_context 204800");
    check(log_metadata_contains(db, id, "preflight_passed",
                                "\"max_context_source\":\"entry.meta.n_ctx\""),
          "ceiling from entry.meta.n_ctx");

    check(log_metadata_contains(db, id, "llm_request",
                                "\"dialect\":\"openai\""),
          "llm_request dialect=openai");
    check(log_metadata_contains(db, id, "llm_request",
                                "\"dialect_source\":\"model_revision\""),
          "llm_request dialect_source=model_revision");
    check(log_metadata_contains(db, id, "llm_request",
                                "\"model_backend\":\"openai\""),
          "llm_request model_backend=openai");

    check(log_has_event(db, id, "execution_completed"),
          "execution_completed logged");
}

/* 2. The OpenAI envelope against a llama.cpp server: the stub refuses it,
 *    so the run fails with EXIT_HTTP and the refusal is in the row. */
static void s2_openai_envelope_rejected_by_llamacpp(db_t *db)
{
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.health_status = 200;
    cfg.chat_status = 200;
    cfg.chat_content = OD_CONTENT;
    cfg.max_context = 162000;   /* the llama.cpp router shape */

    int id = scenario("2 OpenAI envelope -> llama.cpp server refuses",
                      db, &cfg, "openai", "llamacpp", OD_SCHEMA, NULL,
                      "CTX-CONTENT", EXIT_HTTP, ACTA_EXEC_STATUS_FAILED,
                      NULL,
                      "response_format.schema is required for type json_schema");
    if (id < 0)
        return;

    check(stub_server_chat_requests() == 1, "the request was attempted");
    check(log_lacks_event(db, id, "execution_completed"),
          "no execution_completed");
    check(log_lacks_event(db, id, "llm_response"), "no llm_response");
}

/* 3. The llama.cpp envelope against an openai-dialect server: this is
 *    docs/session_strata_backend.md issue A reproduced against a stub
 *    that answers exactly like Strata. */
static void s3_llamacpp_envelope_rejected_by_openai(db_t *db)
{
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.health_status = 200;
    cfg.chat_status = 200;
    cfg.chat_content = OD_CONTENT;
    cfg.models_meta_n_ctx = 204800;

    int id = scenario("3 llama.cpp envelope -> openai server refuses",
                      db, &cfg, "llamacpp", "openai", OD_SCHEMA, NULL,
                      "CTX-CONTENT", EXIT_HTTP, ACTA_EXEC_STATUS_FAILED,
                      NULL,
                      "response_format.json_schema needs a schema object");
    if (id < 0)
        return;

    check(stub_server_chat_requests() == 1, "the request was attempted");
    check(last_body_contains("\"type\":\"json_schema\",\"schema\":{"),
          "request carried the llama.cpp envelope");
    check(!last_body_contains("\"json_schema\":{"),
          "no OpenAI json_schema wrapper in that body");
    check(log_lacks_event(db, id, "execution_completed"),
          "no execution_completed");
}

/* 4. top_k is a llama.cpp request parameter: model.configuration may set
 *    it on the openai dialect, but it must not reach the request body
 *    (decision 3). */
static void s4_top_k_omitted_on_openai(db_t *db)
{
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.health_status = 200;
    cfg.chat_status = 200;
    cfg.chat_content = "plain answer";
    cfg.models_meta_n_ctx = 204800;

    /* temperature is in the configuration too: it proves the omission is
     * dialect-specific and not a general "parameters are dropped" bug.
     * (The runner sends temperature only when model.configuration sets
     * it, so it has to be configured here.) */
    int id = scenario("4 top_k in configuration, openai dialect -> not sent",
                      db, &cfg, "openai", "openai", NULL,
                      "{\"top_k\":40,\"temperature\":0.7}", "CTX-CONTENT",
                      EXIT_OK, ACTA_EXEC_STATUS_COMPLETED, "plain answer",
                      NULL);
    if (id < 0)
        return;

    check(!last_body_contains("\"top_k\""), "top_k absent from the body");
    check(last_body_contains("\"temperature\""),
          "the shared parameters are still sent");
    check(!last_body_contains("\"response_format\""),
          "no envelope without an output_schema");
}

/* 5. The llamacpp baseline stays exactly as it was: top_k IS sent and the
 *    catalog audit runs once. */
static void s5_top_k_sent_on_llamacpp(db_t *db)
{
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.health_status = 200;
    cfg.chat_status = 200;
    cfg.chat_content = "plain answer";
    cfg.max_context = 162000;

    int id = scenario("5 top_k in configuration, llamacpp dialect -> sent",
                      db, &cfg, "llamacpp", "llamacpp", NULL,
                      "{\"top_k\":40,\"temperature\":0.7}", "CTX-CONTENT",
                      EXIT_OK, ACTA_EXEC_STATUS_COMPLETED, "plain answer",
                      NULL);
    if (id < 0)
        return;

    check(last_body_contains("\"top_k\":40"), "top_k sent on llamacpp");
    check(last_body_contains("\"temperature\""),
          "temperature sent on llamacpp too");
    check(stub_server_catalog_requests() == 1,
          "one GET / catalog call on llamacpp");
    check(log_metadata_contains(db, id, "preflight_passed",
                                "\"dialect\":\"llamacpp\""),
          "preflight_passed dialect=llamacpp");
    check(log_metadata_contains(db, id, "preflight_passed",
                                "\"max_context_source\":\"entry.max_context\""),
          "ceiling from entry.max_context");
}

/* 6. supports_response_format:false sends no envelope at all — an
 *    openai-dialect server has nothing to refuse, and the post-hoc
 *    validation path validates the stored content instead. */
static void s6_no_envelope_accepted(db_t *db)
{
    stub_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.health_status = 200;
    cfg.chat_status = 200;
    cfg.chat_content = OD_CONTENT;
    cfg.models_meta_n_ctx = 204800;

    int id = scenario("6 supports_response_format:false -> no envelope",
                      db, &cfg, "openai", "openai", OD_SCHEMA,
                      "{\"supports_response_format\":false}",
                      "CTX-CONTENT", EXIT_OK, ACTA_EXEC_STATUS_COMPLETED,
                      OD_CONTENT, NULL);
    if (id < 0)
        return;

    check(!last_body_contains("\"response_format\""),
          "no response_format in the body");
    check(stub_server_chat_requests() == 1, "chat accepted (no 400)");
    check(log_has_event(db, id, "validation_started"),
          "post-hoc validation ran");
    check(log_has_event(db, id, "execution_completed"),
          "execution_completed logged");
}

/* ── main ─────────────────────────────────────────────────────────── */

/* main.c owns runner_gopts in the real binary; this suite links run.c
 * without main.c, so the definition lives here (same as test_run.c). */
const global_opts_t *runner_gopts;

int main(void)
{
    /* Verbose level 1 so the VLOG action summaries go to stderr. */
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

    s1_openai_end_to_end(db);
    s2_openai_envelope_rejected_by_llamacpp(db);
    s3_llamacpp_envelope_rejected_by_openai(db);
    s4_top_k_omitted_on_openai(db);
    s5_top_k_sent_on_llamacpp(db);
    s6_no_envelope_accepted(db);

    acta_db_close(db);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
