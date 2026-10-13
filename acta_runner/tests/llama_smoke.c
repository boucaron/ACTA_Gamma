/*
 * llama_smoke — manual LIVE check of an OpenAI-compatible backend, in
 * either ACTA dialect (docs/building.md "Live backend check",
 * acta_runner/tests/README.md).
 *
 * It is the C counterpart of scripts/live_backend_check.sh's probe phase
 * and it speaks the SAME dialect as acta_runner: the request body is built
 * through src/dialect.c, so what this tool sends is what the runner sends.
 * (Before the dialect work it hard-coded a bare chat body with no
 * response_format at all, so it could not see the structured-output path.)
 *
 * Usage (flag form, preferred):
 *   llama_smoke --base-url URL [--key KEY] [--dialect llamacpp|openai]
 *               [--model ID] [--prompt TEXT] [--timeout N]
 *
 * Legacy positional form (unchanged):
 *   llama_smoke <base_url> <api_key> [model] [prompt] [timeout_sec]
 *
 *   <base_url> / --base-url   e.g. http://127.0.0.1:8080 (trailing "/" ok)
 *   <api_key>  / --key        Bearer key; "-" or empty = no auth header
 *   --dialect                 llamacpp (default) | openai — the closed
 *                             models.backend vocabulary; an unknown value
 *                             exits 3, exactly like the runner
 *   [model]    / --model      model id; default: the first id from
 *                             GET /v1/models (else the /health "model")
 *   [prompt]   / --prompt     user message; default: "Say hello."
 *   [timeout]  / --timeout    per-request timeout; default: 60
 *
 * Flow (reuses src/backend.c — the runner's own curl wrapper — and
 * src/dialect.c — the runner's own dialect module):
 *   1. GET  {base}/health                liveness; body kept for the
 *                                        ceiling and the model id
 *   2. GET  {base}/v1/models             ids; the matched entry kept
 *   3. POST {base}/v1/chat/completions   plain completion
 *   4. context ceiling                   dialect chain entry.max_context ->
 *                                        entry.meta.n_ctx -> health.max_context,
 *                                        with the source label the runner logs
 *                                        (INFORMATIONAL: 0 = the server
 *                                        reports no ceiling)
 *   5. GET  {base}/                      the llama.cpp catalog audit,
 *                                        expected only when the dialect's
 *                                        fetch_catalog flag is set
 *                                        (INFORMATIONAL: the runner treats a
 *                                        missing catalog as "catalog":null,
 *                                        never as a failure)
 *   6. POST ... response_format          the envelope THIS dialect builds
 *                                        (HARD: a refusal is exactly the
 *                                        structured-output failure a skill
 *                                        with output_schema would hit)
 *
 * top_k is sent only when the dialect's sends_top_k flag is set, so the
 * request body is the runner's body, not an approximation of it.
 *
 * Exits 0 when steps 1, 2, 3 and 6 all succeed; 1 on any hard failure;
 * 3 on a usage error or a dialect outside the vocabulary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "backend.h"
#include "dialect.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s --base-url URL [--key KEY] [--dialect llamacpp|openai]\n"
            "              [--model ID] [--prompt TEXT] [--timeout N]\n"
            "   or: %s <base_url> <api_key> [model] [prompt] [timeout_sec]\n"
            "  --base_url    e.g. http://127.0.0.1:8080\n"
            "  --key         Bearer key, or \"-\" for no auth\n"
            "  --dialect     llamacpp (default) | openai\n"
            "  --model       model id (default: first from /v1/models)\n"
            "  --prompt      user message (default: \"Say hello.\")\n"
            "  --timeout     per-request timeout, seconds (default: 60)\n",
            prog, prog);
}

/* Strip a single trailing '/' from `in` into `out` (bounded by outsz). */
static void rtrim_slash(char *out, size_t outsz, const char *in)
{
    size_t n = strlen(in);
    if (n > 1 && in[n - 1] == '/') n--;
    if (n >= outsz) n = outsz - 1;
    memcpy(out, in, n);
    out[n] = '\0';
}

/* ---- argument parsing: flags anywhere, legacy positionals preserved ---- */

typedef struct {
    const char *base;
    const char *key;
    const char *dialect;
    const char *model;
    const char *prompt;
    int         timeout;
} smoke_opts_t;

/* value of "--name" or "--name=value"; consumes the next token when needed */
static int flag_value(char **argv, int i, int argc, const char *name,
                      const char **out)
{
    const char *tok = argv[i];
    size_t nlen = strlen(name);
    if (tok[nlen] == '=') {
        *out = tok + nlen + 1;
        return 0;                      /* inline value: one token consumed */
    }
    if (i + 1 >= argc) {
        fprintf(stderr, "%s needs a value\n", name);
        return -1;
    }
    *out = argv[i + 1];
    return 1;                          /* flag + value: two tokens consumed */
}

static int parse_args(int argc, char **argv, smoke_opts_t *o)
{
    const char *pos[5] = { NULL, NULL, NULL, NULL, NULL };
    int npos = 0;

    o->base = NULL; o->key = NULL; o->dialect = NULL;
    o->model = NULL; o->prompt = NULL; o->timeout = 60;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v;
        int used = 0;

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            usage(argv[0]);
            return 1;
        }
        if (strncmp(a, "--base-url", 10) == 0 ||
            strncmp(a, "--base_url", 10) == 0) {
            used = flag_value(argv, i, argc, a, &v);
            if (used < 0) return -1;
            o->base = v;
        } else if (strncmp(a, "--key", 5) == 0) {
            used = flag_value(argv, i, argc, a, &v);
            if (used < 0) return -1;
            o->key = v;
        } else if (strncmp(a, "--dialect", 9) == 0) {
            used = flag_value(argv, i, argc, a, &v);
            if (used < 0) return -1;
            o->dialect = v;
        } else if (strncmp(a, "--model", 7) == 0) {
            used = flag_value(argv, i, argc, a, &v);
            if (used < 0) return -1;
            o->model = v;
        } else if (strncmp(a, "--prompt", 8) == 0) {
            used = flag_value(argv, i, argc, a, &v);
            if (used < 0) return -1;
            o->prompt = v;
        } else if (strncmp(a, "--timeout", 9) == 0) {
            used = flag_value(argv, i, argc, a, &v);
            if (used < 0) return -1;
            o->timeout = atoi(v);
        } else if (strncmp(a, "--", 2) == 0) {
            fprintf(stderr, "unknown option '%s'\n", a);
            return -1;
        } else if (npos < 5) {
            pos[npos++] = a;
        } else {
            fprintf(stderr, "extra argument '%s'\n", a);
            return -1;
        }
        i += used;
    }

    /* legacy positional form fills whatever the flags did not set */
    if (!o->base && npos > 0) o->base    = pos[0];
    if (!o->key  && npos > 1) o->key     = pos[1];
    if (!o->model   && npos > 2) o->model   = pos[2];
    if (!o->prompt  && npos > 3) o->prompt  = pos[3];
    if (npos > 4 && o->timeout == 60) o->timeout = atoi(pos[4]);

    if (!o->base) {
        fprintf(stderr, "no base URL\n");
        return -1;
    }
    if (o->timeout <= 0) o->timeout = 60;
    return 0;
}

/*
 * Build one chat request body the way run.c builds it: model, messages
 * (system = the skill prompt, user = the context content), temperature,
 * max_tokens, stream:false, top_k only when the dialect sends it, and
 * response_format only when `rf` is non-NULL.
 * Returns a malloc'd JSON string (cJSON_free by the caller).
 */
static char *chat_body(const char *model, const char *system,
                       const char *user, const cJSON *rf,
                       const dialect_t *d, int max_tokens)
{
    cJSON *body = cJSON_CreateObject();
    if (!body) return NULL;

    cJSON_AddStringToObject(body, "model", model);

    cJSON *msgs = cJSON_AddArrayToObject(body, "messages");
    if (system && system[0]) {
        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "role", "system");
        cJSON_AddStringToObject(m, "content", system);
        cJSON_AddItemToArray(msgs, m);
    }
    {
        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "role", "user");
        cJSON_AddStringToObject(m, "content", user);
        cJSON_AddItemToArray(msgs, m);
    }

    cJSON_AddNumberToObject(body, "temperature", 0);
    cJSON_AddNumberToObject(body, "max_tokens", max_tokens);
    if (d->sends_top_k)
        cJSON_AddNumberToObject(body, "top_k", 8);
    cJSON_AddBoolToObject(body, "stream", 0);
    if (rf)
        cJSON_AddItemToObject(body, "response_format",
                              cJSON_Duplicate(rf, 1));

    char *s = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    return s;
}

/* Print one step's verdict line. */
static void step(const char *what, int http, long latency_ms,
                 const char *extra)
{
    printf("%-28s HTTP %d (%ld ms)%s%s\n", what, http, latency_ms,
           extra ? " " : "", extra ? extra : "");
}

/*
 * One chat POST. `rf` NULL = plain call. Returns 0 on HTTP 200 with a
 * non-empty choices[0].message.content, 1 otherwise (the server's own
 * error message is printed when present).
 */
static int chat_step(const char *what, const char *url, const char *body,
                     const char *auth, int timeout_sec)
{
    backend_response_t res;
    int rc = backend_request("POST", url, body, auth, timeout_sec, &res);
    if (rc != BACKEND_OK) {
        printf("%-28s %s\n", what, backend_strerror(rc));
        free(res.body);
        return 1;
    }

    int ok = 0;
    cJSON *root = cJSON_Parse(res.body);
    if (root) {
        cJSON *choices = cJSON_GetObjectItem(root, "choices");
        if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
            cJSON *msg = cJSON_GetObjectItem(
                cJSON_GetArrayItem(choices, 0), "message");
            cJSON *content = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
            cJSON *finish  = msg ? cJSON_GetObjectItem(msg, "finish_reason")
                                 : NULL;
            finish = finish ? finish : cJSON_GetObjectItem(
                cJSON_GetArrayItem(choices, 0), "finish_reason");
            if (cJSON_IsString(content) && content->valuestring &&
                content->valuestring[0]) {
                char extra[512];
                snprintf(extra, sizeof extra, "content: %.120s%s",
                         content->valuestring,
                         (cJSON_IsString(finish) && finish->valuestring)
                             ? "" : "");
                step(what, res.http_status, res.latency_ms, extra);
                if (cJSON_IsString(finish) && finish->valuestring)
                    printf("%-28s finish_reason: %s\n", "", finish->valuestring);
                ok = 1;
            }
        }
        if (!ok) {
            cJSON *err = cJSON_GetObjectItem(root, "error");
            cJSON *em  = err ? cJSON_GetObjectItem(err, "message") : NULL;
            char extra[512];
            snprintf(extra, sizeof extra, "%s",
                     (cJSON_IsString(em) && em->valuestring)
                         ? em->valuestring : res.body);
            step(what, res.http_status, res.latency_ms, extra);
        }
        cJSON_Delete(root);
    } else {
        step(what, res.http_status, res.latency_ms, "unparsable body");
    }
    free(res.body);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    smoke_opts_t o;
    int pr = parse_args(argc, argv, &o);
    if (pr != 0) {
        if (pr == 1) return 3;         /* --help already printed */
        usage(argv[0]);
        return 3;
    }

    /* The closed vocabulary: an unknown dialect is refused before any
     * request, exactly like the runner's EXIT_INVALID path. */
    const char *dname = o.dialect ? o.dialect : "llamacpp";
    dialect_id_t did = dialect_from_string(dname);
    if (did == DIALECT_UNKNOWN) {
        fprintf(stderr, "invalid --dialect '%s' (expected %s)\n",
                dname, dialect_expected_names());
        return 3;
    }
    const dialect_t *D = dialect_get(did);

    char base[1024];
    rtrim_slash(base, sizeof base, o.base);

    const char *auth = (o.key && o.key[0] && strcmp(o.key, "-") != 0)
                           ? o.key : NULL;

    printf("== live backend check: %s  (dialect %s)\n", base,
           dialect_name(did));
    printf("   auth: %s   top_k: %s   catalog audit: %s\n",
           auth ? "Bearer header sent" : "none (no Authorization header)",
           D->sends_top_k ? "sent (this dialect sends top_k)"
                          : "omitted (not part of this dialect)",
           D->fetch_catalog ? "GET / expected"
                            : "no GET / (this dialect has no catalog endpoint)");

    char url[1200];
    int hard_fail = 0;

    /* ---- 1. /health ---- */
    cJSON *health_root = NULL;
    {
        backend_response_t res;
        snprintf(url, sizeof url, "%s/health", base);
        int rc = backend_request("GET", url, NULL, auth, o.timeout, &res);
        if (rc != BACKEND_OK) {
            printf("%-28s %s\n", "1. health GET", backend_strerror(rc));
            free(res.body);
            return 1;
        }
        health_root = cJSON_Parse(res.body);
        const char *status = "";
        char status_buf[128] = "";
        if (health_root) {
            cJSON *s = cJSON_GetObjectItem(health_root, "status");
            if (cJSON_IsString(s) && s->valuestring)
                snprintf(status_buf, sizeof status_buf, "status=%s",
                         s->valuestring);
            status = status_buf;
        }
        step("1. health GET", res.http_status, res.latency_ms, status);
        free(res.body);
        if (res.http_status != 200) {
            printf("   health is not ok (503 = model still loading)\n");
            if (health_root) cJSON_Delete(health_root);
            return 1;
        }
    }

    /* ---- 2. /v1/models ---- */
    cJSON *models_root = NULL;
    cJSON *entry = NULL;              /* the entry the runner would match */
    char health_model[256] = "";
    char model_id[256] = "";
    {
        backend_response_t res;
        snprintf(url, sizeof url, "%s/v1/models", base);
        int rc = backend_request("GET", url, NULL, auth, o.timeout, &res);
        if (rc != BACKEND_OK || res.http_status != 200) {
            printf("%-28s %s\n", "2. models GET", backend_strerror(rc));
            if (rc == BACKEND_OK)
                printf("   HTTP %d\n", res.http_status);
            free(res.body);
            if (health_root) cJSON_Delete(health_root);
            return 1;
        }
        models_root = cJSON_Parse(res.body);
        free(res.body);

        if (health_root) {
            cJSON *m = cJSON_GetObjectItem(health_root, "model");
            if (cJSON_IsString(m) && m->valuestring)
                snprintf(health_model, sizeof health_model, "%s",
                         m->valuestring);
        }

        printf("%-28s HTTP %d (%ld ms)\n", "2. models GET", res.http_status,
               (long)res.latency_ms);

        cJSON *arr = models_root
                         ? cJSON_GetObjectItem(models_root, "data") : NULL;
        if (cJSON_IsArray(arr)) {
            int n = cJSON_GetArraySize(arr);
            printf("%-28s ids:", "");
            for (int i = 0; i < n && i < 8; i++) {
                cJSON *e = cJSON_GetArrayItem(arr, i);
                cJSON *id = e ? cJSON_GetObjectItem(e, "id") : NULL;
                if (!cJSON_IsString(id) || !id->valuestring) continue;
                printf(" %s", id->valuestring);
                if (!model_id[0])
                    snprintf(model_id, sizeof model_id, "%s", id->valuestring);
                if (!entry && o.model && strcmp(o.model, id->valuestring) == 0)
                    entry = e;
            }
            printf("\n");
        }
        if (!entry && o.model) {
            printf("%-28s model \"%s\" is NOT served by this server\n", "",
                   o.model);
            hard_fail = 1;
        }
        if (!entry) {
            /* no explicit --model: use the first id, as the runner's
             * preflight matches the configured identifier */
            entry = (cJSON_IsArray(arr) && cJSON_GetArraySize(arr) > 0)
                        ? cJSON_GetArrayItem(arr, 0) : NULL;
        }
        if (!model_id[0] && health_model[0])
            snprintf(model_id, sizeof model_id, "%s", health_model);
        if (!model_id[0]) {
            printf("%-28s no model id reported by /v1/models or /health\n", "");
            hard_fail = 1;
        }
    }
    if (hard_fail) {
        if (models_root) cJSON_Delete(models_root);
        if (health_root) cJSON_Delete(health_root);
        return 1;
    }

    /* ---- 3. plain chat ---- */
    snprintf(url, sizeof url, "%s/v1/chat/completions", base);
    {
        char *body = chat_body(model_id,
                               "Answer with one short sentence.",
                               o.prompt ? o.prompt : "Say hello.",
                               NULL, D, 64);
        if (!body) {
            if (models_root) cJSON_Delete(models_root);
            if (health_root) cJSON_Delete(health_root);
            return 1;
        }
        if (chat_step("3. chat POST", url, body, auth, o.timeout) != 0)
            hard_fail = 1;
        cJSON_free(body);
    }

    /* ---- 4. the context ceiling this dialect would record ---- */
    {
        const char *src = "none";
        long ceiling = D->max_context_of ? D->max_context_of(entry,
                                                             health_root,
                                                             &src) : 0;
        printf("%-28s max_context=%ld source=%s\n", "4. ceiling", ceiling,
               src ? src : "none");
        if (ceiling <= 0)
            printf("%-28s WARN: the server reports no context ceiling; "
                   "the max_chars guard has no server value to use\n", "");
    }

    /* ---- 5. the catalog audit (dialect-gated, best effort) ---- */
    {
        backend_response_t res;
        snprintf(url, sizeof url, "%s/", base);
        int rc = backend_request("GET", url, NULL, auth, o.timeout, &res);
        int catalog_like = 0;
        if (rc == BACKEND_OK && res.body && res.body[0]) {
            cJSON *c = cJSON_Parse(res.body);
            if (c) {
                cJSON *models = cJSON_GetObjectItem(c, "models");
                cJSON *status = cJSON_GetObjectItem(c, "status");
                catalog_like = cJSON_IsArray(models) ||
                               (cJSON_IsObject(status) &&
                                cJSON_GetObjectItem(status, "args") != NULL);
                cJSON_Delete(c);
            }
        }
        printf("%-28s %s HTTP %d%s\n", "5. catalog GET",
               catalog_like ? "catalog-shaped body" : "no catalog body",
               rc == BACKEND_OK ? res.http_status : 0,
               rc == BACKEND_OK ? "" : " (transport failure)");
        if (D->fetch_catalog && !catalog_like)
            printf("%-28s NOTE: this dialect audits GET /; the runner records "
                   "\"catalog\":null and continues (best effort)\n", "");
        if (!D->fetch_catalog && catalog_like)
            printf("%-28s NOTE: this dialect makes NO GET /; the runner "
                   "records \"catalog\":null either way\n", "");
        free(res.body);
    }

    /* ---- 6. the envelope THIS dialect builds (the hard check) ---- */
    snprintf(url, sizeof url, "%s/v1/chat/completions", base);
    {
        static const char SCHEMA[] =
            "{\"type\":\"object\",\"required\":[\"label\",\"confidence\"],"
            "\"properties\":{\"label\":{\"type\":\"string\"},"
            "\"confidence\":{\"type\":\"number\"}}}";
        cJSON *schema = cJSON_Parse(SCHEMA);
        cJSON *rf = schema ? D->build_response_format(schema) : NULL;
        if (!rf) {
            printf("%-28s could not build the %s envelope\n",
                   "6. json_schema POST", dialect_name(did));
            hard_fail = 1;
        } else {
            char *rf_str = cJSON_PrintUnformatted(rf);
            printf("%-28s envelope: %s\n", "6. json_schema POST",
                   rf_str ? rf_str : "(unprintable)");
            char *body = chat_body(model_id,
                                   "Classify the sentiment of the input. "
                                   "Reply with JSON only.",
                                   "The food was excellent.",
                                   rf, D, 400);
            if (body) {
                if (chat_step("6. json_schema POST", url, body, auth,
                              o.timeout) != 0) {
                    printf("%-28s the %s envelope was REFUSED: a skill with "
                           "output_schema would fail here\n", "",
                           dialect_name(did));
                    hard_fail = 1;
                }
                cJSON_free(body);
            }
            if (rf_str) cJSON_free(rf_str);
            cJSON_Delete(rf);
        }
        if (schema) cJSON_Delete(schema);
    }

    if (models_root) cJSON_Delete(models_root);
    if (health_root) cJSON_Delete(health_root);

    printf("== verdict: %s (dialect %s, model %s, base %s)\n",
           hard_fail ? "FAIL" : "usable as an acta_runner backend",
           dialect_name(did), model_id, base);
    printf("   register it as: backend=%s base_url=%s model_identifier=%s\n",
           dialect_name(did), base, model_id);
    return hard_fail ? 1 : 0;
}
