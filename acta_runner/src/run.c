/*
 * acta_runner — "run" action (phase 2: full execution pipeline)
 *
 * Pipeline per docs/runner_analysis.md:
 *
 *   1. Claim     — fetch execution; must be pending; start() -> running;
 *                   log execution_started.
 *   2. Resolve   — fetch context, skill revision, model revision;
 *                   log context_loaded / prompt_resolved.
 *   3. Preflight — GET /health (503 -> "model still loading"),
 *                   GET /v1/models (server model id must match).
 *   4. Call      — POST /v1/chat/completions with
 *                     system = skill.prompt_template,
 *                     user   = context.content + "\n\n" + execution.prompt,
 *                     model  = model_identifier,
 *                     params from the model configuration JSON
 *                     (temperature, max_tokens, top_k, api_key,
 *                      supports_response_format),
 *                     response_format = json_schema(output_schema) when the
 *                     skill has an output_schema and the backend supports it;
 *                   log llm_request.
 *   5. Record    — set_raw_response(choices[0].message.content);
 *                   log llm_response (HTTP status, latency, usage).
 *   6. Validate  — when an output_schema exists but response_format was not
 *                   sent (non-llama backend), validate the raw response
 *                   post-hoc against a schema subset
 *                   (type, properties, required, items);
 *                   log validation_started / validation_failed.
 *   7. Close     — complete(result) / fail(error);
 *                   log execution_completed / execution_failed.
 *
 * Concurrency: start() is the atomic claim; after the claim succeeds every
 * error path funnels through fail_execution() so the row never stays stuck
 * in "running".
 */

#include "runner.h"
#include "runner_util.h"
#include "acta_db.h"
#include "backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cjson/cJSON.h>

static void run_usage(FILE *out)
{
    fputs(
        "Usage: acta_runner run <execution-id> [flags]\n"
        "       acta_runner run --pending [flags]\n"
        "\n"
        "Flags:\n"
        "  --pending       Run pending executions instead of one id\n"
        "  --max <n>       Max executions to run with --pending (0 = no limit)\n"
        "  --timeout <sec> Backend timeout in seconds (default 300)\n"
        "  --api_key <key> API key override (default: $OPENAI_API_KEY)\n",
        out);
}

int cmd_run(cmd_args_t *ga, const global_opts_t *gopts, db_t *db)
{
    int pending = cmd_args_has_flag(ga, "pending");
    int max = 0;
    int timeout = 300;
    const char *api_key = NULL;
    char msg[192];

    const char *v = cmd_args_flag(ga, "max", 1);
    if (v) {
        if (!parse_nonneg_int(v, &max)) {
            VLOG(1, "cmd_run: ERROR --max must be a non-negative integer, got '%s'", v);
            emit_error("--max must be a non-negative integer");
            run_usage(stderr);
            return EXIT_INVALID;
        }
    }

    v = cmd_args_flag(ga, "timeout", 1);
    if (v) {
        if (!parse_nonneg_int(v, &timeout) || timeout == 0) {
            VLOG(1, "cmd_run: ERROR --timeout must be a positive integer, got '%s'", v);
            emit_error("--timeout must be a positive integer");
            run_usage(stderr);
            return EXIT_INVALID;
        }
    }

    api_key = cmd_args_flag(ga, "api_key", 1);
    if (!api_key)
        api_key = getenv("OPENAI_API_KEY");

    const char *id_str = cmd_args_next_positional(ga);

    if (pending && id_str) {
        VLOG(1, "cmd_run: ERROR --pending conflicts with an execution-id positional");
        emit_error("--pending conflicts with an execution-id positional");
        run_usage(stderr);
        return EXIT_INVALID;
    }
    if (!pending && !id_str) {
        VLOG(1, "cmd_run: ERROR missing <execution-id> (or use --pending)");
        emit_error("missing <execution-id>: pass an id or use --pending");
        run_usage(stderr);
        return EXIT_INVALID;
    }

    if (pending) {
        /* Run a batch of pending executions (up to --max; 0 = no limit).
         * limit 0 is clamped to ACTA_DB_MAX_PAGE by the lister. */
        execution_query_t q = ACTA_EXEC_QUERY_ANY;
        q.status = ACTA_EXEC_STATUS_PENDING;

        int n = 0;
        int err = ACTA_DB_OK;
        execution_t **rows =
            acta_db_execution_query(db, &q, 0, max, &n, &err);
        if (!rows)
            return finish_op_error(db, err, "execution_query");

        if (n == 0) {
            VLOG(1, "cmd_run: no pending executions");
            return EXIT_OK;
        }

        VLOG(1, "cmd_run: running %d pending execution(s)", n);
        int worst = EXIT_OK;
        for (int i = 0; i < n; i++) {
            int rc = run_execution(db, rows[i]->id, timeout, api_key);
            if (rc != EXIT_OK && rc > worst)
                worst = rc;
        }
        acta_db_execution_list_free(rows, n);
        return worst;
    }

    int id = 0;
    if (!parse_positive_id(id_str, &id)) {
        snprintf(msg, sizeof msg,
                 "invalid <execution-id>: '%s' must be a positive integer",
                 id_str);
        VLOG(1, "cmd_run: %s", msg);
        emit_error(msg);
        run_usage(stderr);
        return EXIT_INVALID;
    }

    return run_execution(db, id, timeout, api_key);
}

/* ── pipeline helpers ───────────────────────────────────────────────── */

/* Log one phase row for the execution. A log failure is NOT fatal:
 * log rows are an audit aid, not state, so we warn (VLOG) and continue. */
static int log_phase(db_t *db, int exec_id, const char *level,
                     const char *event, const char *message,
                     const char *metadata_json)
{
    execution_log_t log;
    memset(&log, 0, sizeof(log));
    log.execution_id = exec_id;
    log.level    = (char *)level;
    log.event    = (char *)event;
    log.message  = (char *)message;
    log.metadata = (char *)metadata_json;
    int rc = acta_db_execution_log_create(db, &log, NULL);
    if (rc != ACTA_DB_OK)
        VLOG(1, "log_phase: execution_log create failed (%s); continuing",
             acta_db_strerror(rc));
    return rc;
}

/* Heap-allocated JSON string for a cJSON value; the value tree is freed
 * by this call. NULL in, NULL out. */
static char *json_print(cJSON *v)
{
    if (!v)
        return NULL;
    char *s = cJSON_PrintUnformatted(v);
    cJSON_Delete(v);
    return s;
}

/* Build "<base_url><path>" with the base's trailing slash normalized. */
static void build_url(char *out, size_t outsz, const char *base, const char *path)
{
    size_t n = base ? strlen(base) : 0;
    while (n > 0 && base[n - 1] == '/')
        n--;
    snprintf(out, outsz, "%.*s%s", (int)n, base ? base : "", path);
}

/* Terminal failure: log execution_failed (level error), transition the
 * row running -> failed, emit the runner error line, return exit_code.
 * After the atomic start() claim, EVERY error path in run_execution
 * must funnel through here so the row never stays stuck in "running". */
static int fail_execution(db_t *db, int exec_id, int exit_code,
                          const char *msg)
{
    log_phase(db, exec_id, ACTA_LOG_LEVEL_ERROR, "execution_failed",
              msg, NULL);
    int rc = acta_db_execution_fail(db, exec_id, msg);
    if (rc != ACTA_DB_OK)
        VLOG(1, "fail_execution: execution_fail returned %s; row may stay running",
             acta_db_strerror(rc));
    return emit_runner_error(exit_code, msg);
}

/* Set errmsg/exit_code_ and jump to the common failure exit in
 * run_execution. Both are declared at the top of run_execution so the
 * macro can reference them at any point before the `done:` label. */
#define FAIL(exit_code, fmt, ...)                                   \
    do {                                                            \
        snprintf(errmsg, sizeof errmsg, fmt, ##__VA_ARGS__);        \
        exit_code_ = (exit_code);                                   \
        goto done;                                                  \
    } while (0)

/* ── post-hoc output_schema validation (subset) ────────────────────── */

/* type_ok: does `value` satisfy the JSON-schema `type` keyword?
 * Unknown/absent type keywords are not enforced. */
static int type_ok(const char *type, const cJSON *v)
{
    if (!type)
        return 1;
    if (strcmp(type, "object")  == 0) return cJSON_IsObject(v);
    if (strcmp(type, "array")   == 0) return cJSON_IsArray(v);
    if (strcmp(type, "string")  == 0) return cJSON_IsString(v);
    if (strcmp(type, "number")  == 0) return cJSON_IsNumber(v);
    if (strcmp(type, "integer") == 0)
        return cJSON_IsNumber(v) && v->valuedouble == floor(v->valuedouble);
    if (strcmp(type, "boolean") == 0) return cJSON_IsBool(v);
    if (strcmp(type, "null")    == 0) return cJSON_IsNull(v);
    return 1;
}

/*
 * Validate `value` against a SUBSET of a JSON schema:
 *   - type       (object|array|string|number|integer|boolean|null)
 *   - required   (on objects)
 *   - properties (on objects, recursively)
 *   - items      (on arrays, recursively)
 * Not supported: pattern, minLength/maxLength, enum, format, ...
 * Returns 1 if conforming, 0 otherwise (first violation in errbuf).
 */
static int schema_check(const cJSON *schema, const cJSON *value,
                        char *errbuf, size_t errsz, int depth,
                        const char *path)
{
    if (depth > 32)
        return 1;

    cJSON *type = cJSON_GetObjectItem(schema, "type");
    if (cJSON_IsString(type) && type->valuestring) {
        if (!type_ok(type->valuestring, value)) {
            snprintf(errbuf, errsz, "%s: expected type '%s'",
                     path, type->valuestring);
            return 0;
        }
    }

    if (cJSON_IsObject(value)) {
        cJSON *req = cJSON_GetObjectItem(schema, "required");
        if (cJSON_IsArray(req)) {
            int n = cJSON_GetArraySize(req);
            for (int i = 0; i < n; i++) {
                cJSON *k = cJSON_GetArrayItem(req, i);
                if (!cJSON_IsString(k) || !k->valuestring)
                    continue;
                if (!cJSON_HasObjectItem(value, k->valuestring)) {
                    snprintf(errbuf, errsz,
                             "%s: missing required property '%s'",
                             path, k->valuestring);
                    return 0;
                }
            }
        }
        cJSON *props = cJSON_GetObjectItem(schema, "properties");
        if (cJSON_IsObject(props)) {
            for (cJSON *p = props->child; p; p = p->next) {
                if (!p->string)
                    continue;
                cJSON *v = cJSON_GetObjectItem(value, p->string);
                if (!v)
                    continue; /* optional property */
                char sub[160];
                snprintf(sub, sizeof sub, "%s.%s", path, p->string);
                if (!schema_check(p, v, errbuf, errsz, depth + 1, sub))
                    return 0;
            }
        }
    } else if (cJSON_IsArray(value)) {
        cJSON *items = cJSON_GetObjectItem(schema, "items");
        if (cJSON_IsObject(items) || cJSON_IsArray(items)) {
            int n = cJSON_GetArraySize(value);
            for (int i = 0; i < n; i++) {
                char sub[160];
                snprintf(sub, sizeof sub, "%s[%d]", path, i);
                if (!schema_check(items, cJSON_GetArrayItem(value, i),
                                  errbuf, errsz, depth + 1, sub))
                    return 0;
            }
        }
    }
    return 1;
}

/* ── single-execution pipeline ──────────────────────────────────────── */

int run_execution(db_t *db, int exec_id, int timeout_sec,
                  const char *api_key)
{
    int exit_code_ = EXIT_OK;  /* set by FAIL, consumed at `done:` */
    char errmsg[512];           /* set by FAIL, consumed at `done:` */
    cJSON *jr = NULL;           /* parsed response; freed at `done:` */

    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, exec_id, &err);
    if (err != ACTA_DB_OK) {
        acta_db_execution_free(e);
        return finish_op_error(db, err, "execution_get");
    }
    if (!e)
        return emit_not_found("execution");

    if (e->status && strcmp(e->status, ACTA_EXEC_STATUS_PENDING) != 0) {
        VLOG(1, "run_execution: execution %d is not pending (status: %s)",
             exec_id, e->status ? e->status : "(null)");
        acta_db_execution_free(e);
        emit_error("execution is not pending; only pending executions "
                   "can be run");
        return EXIT_INVALID;
    }

    /* ---- 1. claim: pending -> running (atomic) ---- */
    int rc = acta_db_execution_start(db, exec_id);
    if (rc != ACTA_DB_OK) {
        acta_db_execution_free(e);
        return emit_runner_error(EXIT_INVALID,
                                 "could not claim execution: start() failed");
    }
    log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "execution_started",
              "execution claimed (pending -> running)", NULL);

    /* ---- 2. resolve refs ---- */
    context_t *ctx = acta_db_context_get(db, e->context_id, &err);
    if (err != ACTA_DB_OK || !ctx) {
        char msg[192];
        snprintf(msg, sizeof msg, "context fetch failed (id %d): %s",
                 e->context_id,
                 acta_db_last_error(db) ? acta_db_last_error(db) : "not found");
        int ex = fail_execution(db, exec_id,
                                err != ACTA_DB_OK ? EXIT_SQL : EXIT_INVALID, msg);
        acta_db_execution_free(e);
        return ex;
    }

    skill_revision_t *skill =
        acta_db_skill_revision_get(db, e->skill_revision_id, &err);
    if (err != ACTA_DB_OK || !skill) {
        char msg[192];
        snprintf(msg, sizeof msg, "skill revision fetch failed (id %d): %s",
                 e->skill_revision_id,
                 acta_db_last_error(db) ? acta_db_last_error(db) : "not found");
        int ex = fail_execution(db, exec_id,
                                err != ACTA_DB_OK ? EXIT_SQL : EXIT_INVALID, msg);
        acta_db_context_free(ctx);
        acta_db_execution_free(e);
        return ex;
    }

    model_revision_t *model =
        acta_db_model_revision_get(db, e->model_revision_id, &err);
    if (err != ACTA_DB_OK || !model) {
        char msg[192];
        snprintf(msg, sizeof msg, "model revision fetch failed (id %d): %s",
                 e->model_revision_id,
                 acta_db_last_error(db) ? acta_db_last_error(db) : "not found");
        int ex = fail_execution(db, exec_id,
                                err != ACTA_DB_OK ? EXIT_SQL : EXIT_INVALID, msg);
        acta_db_context_free(ctx);
        acta_db_skill_revision_free(skill);
        acta_db_execution_free(e);
        return ex;
    }
    if (!model->base_url || !model->base_url[0] ||
        !model->model_identifier || !model->model_identifier[0]) {
        int ex = fail_execution(db, exec_id, EXIT_INVALID,
                                "model revision missing base_url or "
                                "model_identifier");
        acta_db_context_free(ctx);
        acta_db_skill_revision_free(skill);
        acta_db_model_revision_free(model);
        acta_db_execution_free(e);
        return ex;
    }

    /* user message = context.content + "\n\n" + execution.prompt
     * (either half may be empty; both empty -> fail). */
    const char *prompt = e->prompt;
    const char *cc = ctx->content;
    char *user = NULL;
    if (cc && cc[0] && prompt && prompt[0]) {
        size_t n = strlen(cc) + 2 + strlen(prompt);
        user = (char *)malloc(n + 1);
        if (user) {
            memcpy(user, cc, strlen(cc));
            memcpy(user + strlen(cc), "\n\n", 2);
            memcpy(user + strlen(cc) + 2, prompt, strlen(prompt) + 1);
        }
    } else {
        const char *src = (cc && cc[0]) ? cc : (prompt ? prompt : "");
        user = src ? strdup(src) : NULL;
    }
    if (!user || !user[0]) {
        free(user);
        int ex = fail_execution(db, exec_id, EXIT_INVALID,
                                "empty prompt (no context content and no "
                                "execution prompt)");
        acta_db_context_free(ctx);
        acta_db_skill_revision_free(skill);
        acta_db_model_revision_free(model);
        acta_db_execution_free(e);
        return ex;
    }
    const char *system = skill->prompt_template;

    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "context_id", (double)e->context_id);
    cJSON_AddNumberToObject(m, "content_bytes",
                            (double)strlen(cc ? cc : ""));
    char *meta = json_print(m);
    log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "context_loaded",
              "context loaded", meta);
    free(meta);

    m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "system_bytes",
                            (double)strlen(system ? system : ""));
    cJSON_AddNumberToObject(m, "user_bytes", (double)strlen(user));
    meta = json_print(m);
    log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "prompt_resolved",
              "prompt resolved (system + user)", meta);
    free(meta);

    /* ---- configuration JSON (backend knobs) ----
     * Known keys:
     *   api_key                 string; used when --api_key / env not set
     *   temperature             number
     *   max_tokens              number
     *   top_k                   number
     *   supports_response_format bool (default true; false => the backend
     *                             has no json_schema response_format, so
     *                             post-hoc validation applies)
     */
    cJSON *cfg = (model->configuration && model->configuration[0])
        ? cJSON_Parse(model->configuration) : NULL;
    double temperature = -1.0;
    long max_tokens = 0, top_k = 0;
    int supports_rf = 1;
    if (cfg) {
        cJSON *kv;
        kv = cJSON_GetObjectItem(cfg, "api_key");
        if (cJSON_IsString(kv) && kv->valuestring)
            api_key = kv->valuestring;
        kv = cJSON_GetObjectItem(cfg, "temperature");
        if (cJSON_IsNumber(kv))
            temperature = kv->valuedouble;
        kv = cJSON_GetObjectItem(cfg, "max_tokens");
        if (cJSON_IsNumber(kv) && kv->valueint > 0)
            max_tokens = kv->valueint;
        kv = cJSON_GetObjectItem(cfg, "top_k");
        if (cJSON_IsNumber(kv) && kv->valueint > 0)
            top_k = kv->valueint;
        kv = cJSON_GetObjectItem(cfg, "supports_response_format");
        if (cJSON_IsBool(kv))
            supports_rf = cJSON_IsTrue(kv);
    }

    /* ---- output_schema: parse once, used for response_format and/or
     *      post-hoc validation ---- */
    const char *schema = skill->output_schema;
    int have_schema = schema && schema[0];
    cJSON *schema_j = NULL;
    if (have_schema) {
        schema_j = cJSON_Parse(schema);
        if (!schema_j) {
            int ex = fail_execution(db, exec_id, EXIT_INVALID,
                                    "skill output_schema is not valid JSON");
            cJSON_Delete(cfg);
            free(user);
            acta_db_context_free(ctx);
            acta_db_skill_revision_free(skill);
            acta_db_model_revision_free(model);
            acta_db_execution_free(e);
            return ex;
        }
    }
    int use_response_format = have_schema && supports_rf;

    /* ---- 3. preflight: /health, /v1/models ---- */
    {
        char url[1024];
        build_url(url, sizeof url, model->base_url, "/health");
        backend_response_t r;
        int brc = backend_request("GET", url, NULL, api_key, timeout_sec, &r);
        if (brc != BACKEND_OK) {
            free(r.body);
            if (brc == BACKEND_ERR_TIMEOUT)
                FAIL(EXIT_TIMEOUT, "health check timed out after %d s",
                     timeout_sec);
            FAIL(EXIT_HTTP, "health check transport failure: %s",
                 backend_strerror(brc));
        }
        int st = r.http_status;
        free(r.body);
        if (st == 503)
            FAIL(EXIT_HTTP,
                 "backend /health returned 503: model still loading");
        if (st != 200)
            FAIL(EXIT_HTTP, "backend /health returned %d", st);
    }
    {
        char url[1024];
        build_url(url, sizeof url, model->base_url, "/v1/models");
        backend_response_t r;
        int brc = backend_request("GET", url, NULL, api_key, timeout_sec, &r);
        if (brc != BACKEND_OK) {
            free(r.body);
            if (brc == BACKEND_ERR_TIMEOUT)
                FAIL(EXIT_TIMEOUT, "models check timed out after %d s",
                     timeout_sec);
            FAIL(EXIT_HTTP, "models check transport failure: %s",
                 backend_strerror(brc));
        }
        if (r.http_status != 200) {
            int st = r.http_status;
            free(r.body);
            FAIL(EXIT_HTTP, "backend /v1/models returned %d", st);
        }
        cJSON *jm = cJSON_Parse(r.body);
        free(r.body);
        if (!jm)
            FAIL(EXIT_HTTP, "unparseable /v1/models response body");
        cJSON *data = cJSON_GetObjectItem(jm, "data");
        cJSON *d0 = (data && cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0)
            ? cJSON_GetArrayItem(data, 0) : NULL;
        cJSON *sid = d0 ? cJSON_GetObjectItem(d0, "id") : NULL;
        if (!cJSON_IsString(sid) || !sid->valuestring ||
            strcmp(sid->valuestring, model->model_identifier) != 0) {
            const char *server_id =
                (cJSON_IsString(sid) && sid->valuestring)
                    ? sid->valuestring : "(none)";
            cJSON_Delete(jm);
            FAIL(EXIT_HTTP,
                 "server model '%s' does not match execution model '%s'",
                 server_id, model->model_identifier);
        }
        cJSON_Delete(jm);
    }

    /* ---- 4. build and send the chat/completions request ---- */
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", model->model_identifier);
    cJSON *msgs = cJSON_CreateArray();
    if (system && system[0]) {
        cJSON *s = cJSON_CreateObject();
        cJSON_AddStringToObject(s, "role", "system");
        cJSON_AddStringToObject(s, "content", system);
        cJSON_AddItemToArray(msgs, s);
    }
    {
        cJSON *u = cJSON_CreateObject();
        cJSON_AddStringToObject(u, "role", "user");
        cJSON_AddStringToObject(u, "content", user);
        cJSON_AddItemToArray(msgs, u);
    }
    cJSON_AddItemToObject(req, "messages", msgs);
    if (temperature >= 0.0)
        cJSON_AddNumberToObject(req, "temperature", temperature);
    if (max_tokens > 0)
        cJSON_AddNumberToObject(req, "max_tokens", max_tokens);
    if (top_k > 0)
        cJSON_AddNumberToObject(req, "top_k", top_k);
    if (use_response_format) {
        cJSON *rf = cJSON_CreateObject();
        cJSON_AddStringToObject(rf, "type", "json_schema");
        cJSON_AddItemToObject(rf, "schema",
                              cJSON_Duplicate(schema_j, 1));
        cJSON_AddItemToObject(req, "response_format", rf);
    }
    char *reqbody = json_print(req);

    char url[1024];
    build_url(url, sizeof url, model->base_url, "/v1/chat/completions");
    cJSON *m2 = cJSON_CreateObject();
    cJSON_AddStringToObject(m2, "url", url);
    cJSON_AddStringToObject(m2, "model", model->model_identifier);
    cJSON_AddBoolToObject(m2, "response_format", use_response_format);
    char *meta2 = json_print(m2);
    {
        char rmsg[1100];
        snprintf(rmsg, sizeof rmsg, "POST %s model=%s",
                 url, model->model_identifier);
        log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "llm_request", rmsg, meta2);
    }
    free(meta2);

    backend_response_t resp = { 0, NULL, 0 };
    int brc = backend_request("POST", url, reqbody, api_key, timeout_sec,
                              &resp);
    free(reqbody);
    if (brc != BACKEND_OK) {
        free(resp.body);
        if (brc == BACKEND_ERR_TIMEOUT)
            FAIL(EXIT_TIMEOUT, "backend call timed out after %d s",
                 timeout_sec);
        FAIL(EXIT_HTTP, "backend transport failure: %s",
             backend_strerror(brc));
    }
    if (resp.http_status < 200 || resp.http_status >= 300) {
        int st = resp.http_status;
        const char *b = resp.body ? resp.body : "";
        snprintf(errmsg, sizeof errmsg, "backend HTTP %d: %.300s", st, b);
        free(resp.body);
        exit_code_ = EXIT_HTTP;
        goto done;
    }

    /* ---- 5. record: parse the response, store the raw content ---- */
    jr = cJSON_Parse(resp.body);
    if (!jr) {
        free(resp.body);
        FAIL(EXIT_HTTP, "malformed chat/completions response (not JSON)");
    }
    cJSON *choices = cJSON_GetObjectItem(jr, "choices");
    cJSON *c0 = (choices && cJSON_IsArray(choices) &&
                  cJSON_GetArraySize(choices) > 0)
        ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *cm = c0 ? cJSON_GetObjectItem(c0, "message") : NULL;
    cJSON *ct = cm ? cJSON_GetObjectItem(cm, "content") : NULL;
    if (!cJSON_IsString(ct) || !ct->valuestring || !ct->valuestring[0]) {
        free(resp.body);
        FAIL(EXIT_HTTP,
             "response has no non-empty choices[0].message.content");
    }
    const char *content = ct->valuestring;

    long prompt_tokens = 0, completion_tokens = 0, total_tokens = 0;
    cJSON *usage = cJSON_GetObjectItem(jr, "usage");
    if (usage) {
        cJSON *kv;
        kv = cJSON_GetObjectItem(usage, "prompt_tokens");
        if (cJSON_IsNumber(kv)) prompt_tokens = kv->valueint;
        kv = cJSON_GetObjectItem(usage, "completion_tokens");
        if (cJSON_IsNumber(kv)) completion_tokens = kv->valueint;
        kv = cJSON_GetObjectItem(usage, "total_tokens");
        if (cJSON_IsNumber(kv)) total_tokens = kv->valueint;
    }

    rc = acta_db_execution_set_raw_response(db, exec_id, content);
    if (rc != ACTA_DB_OK) {
        free(resp.body);
        FAIL(EXIT_SQL, "set_raw_response failed: %s",
             acta_db_last_error(db) ? acta_db_last_error(db) : "unknown");
    }

    cJSON *m3 = cJSON_CreateObject();
    cJSON_AddNumberToObject(m3, "http_status", (double)resp.http_status);
    cJSON_AddNumberToObject(m3, "latency_ms", (double)resp.latency_ms);
    cJSON_AddNumberToObject(m3, "prompt_tokens", (double)prompt_tokens);
    cJSON_AddNumberToObject(m3, "completion_tokens",
                            (double)completion_tokens);
    cJSON_AddNumberToObject(m3, "total_tokens", (double)total_tokens);
    char *meta3 = json_print(m3);
    {
        char rmsg[128];
        snprintf(rmsg, sizeof rmsg, "HTTP %d in %ld ms",
                 resp.http_status, resp.latency_ms);
        log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "llm_response", rmsg,
                  meta3);
    }
    free(meta3);
    free(resp.body);

    /* ---- 6. validate: post-hoc only when response_format was not sent
     *      (non-llama backend without json_schema support) ---- */
    if (have_schema && !use_response_format) {
        log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "validation_started",
                  "validating raw response against skill output_schema",
                  NULL);
        char verr[256];
        /* The content is a JSON *string* holding the document; parse it
         * first, then validate the parsed value against the schema. */
        cJSON *jv = cJSON_Parse(content);
        if (!jv) {
            snprintf(verr, sizeof verr, "raw response is not valid JSON");
            log_phase(db, exec_id, ACTA_LOG_LEVEL_ERROR, "validation_failed",
                      verr, NULL);
            FAIL(EXIT_INVALID, "output_schema validation failed: %s", verr);
        }
        if (!schema_check(schema_j, jv, verr, sizeof verr, 0, "$")) {
            cJSON_Delete(jv);
            log_phase(db, exec_id, ACTA_LOG_LEVEL_ERROR, "validation_failed",
                      verr, NULL);
            FAIL(EXIT_INVALID, "output_schema validation failed: %s", verr);
        }
        cJSON_Delete(jv);
    }

    /* ---- 7. close: complete / fail ---- */
done:
    if (exit_code_ != EXIT_OK) {
        int ex = fail_execution(db, exec_id, exit_code_, errmsg);
        cJSON_Delete(cfg);
        cJSON_Delete(schema_j);
        cJSON_Delete(jr);
        free(user);
        acta_db_context_free(ctx);
        acta_db_skill_revision_free(skill);
        acta_db_model_revision_free(model);
        acta_db_execution_free(e);
        return ex;
    }

    rc = acta_db_execution_complete(db, exec_id, content);
    if (rc != ACTA_DB_OK) {
        int ex = fail_execution(db, exec_id, EXIT_SQL,
                                "complete() transition failed");
        cJSON_Delete(cfg);
        cJSON_Delete(schema_j);
        cJSON_Delete(jr);
        free(user);
        acta_db_context_free(ctx);
        acta_db_skill_revision_free(skill);
        acta_db_model_revision_free(model);
        acta_db_execution_free(e);
        return ex;
    }
    log_phase(db, exec_id, ACTA_LOG_LEVEL_INFO, "execution_completed",
              "execution completed", NULL);

    cJSON_Delete(cfg);
    cJSON_Delete(schema_j);
    cJSON_Delete(jr);
    free(user);
    acta_db_context_free(ctx);
    acta_db_skill_revision_free(skill);
    acta_db_model_revision_free(model);
    acta_db_execution_free(e);
    return EXIT_OK;
}
