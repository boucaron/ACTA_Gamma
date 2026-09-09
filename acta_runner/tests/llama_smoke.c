/*
 * llama_smoke — manual smoke test against a running OpenAI-compatible
 * (llama-server) backend.
 *
 * Usage:
 *   llama_smoke <base_url> <api_key> [model] [prompt] [timeout_sec]
 *
 *   <base_url>    e.g. http://127.0.0.1:8080   (trailing "/" is fine)
 *   <api_key>     key for "Authorization: Bearer <key>"; pass "-" for
 *                 no auth header (llama-server without --api-key)
 *   [model]       model identifier; default: the first id reported by
 *                 GET /v1/models
 *   [prompt]      single user message; default: "Hello!"
 *   [timeout_sec] hard wall-clock timeout per request; default: 60
 *
 * Flow (reuses src/backend.c, the same curl wrapper as the runner):
 *   1. GET  {base}/health               — liveness
 *   2. GET  {base}/v1/models            — list ids, check/pick model
 *   3. POST {base}/v1/chat/completions  — one user message, print
 *                                          content + usage
 *
 * Exits 0 only when the chat call returns HTTP 200 with a non-empty
 * completion; 1 on any step failure; 3 on usage error.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "backend.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s <base_url> <api_key> [model] [prompt] [timeout_sec]\n"
            "  <base_url>    e.g. http://127.0.0.1:8080\n"
            "  <api_key>     Bearer key, or \"-\" for no auth\n"
            "  [model]       model id (default: first from /v1/models)\n"
            "  [prompt]      user message (default: \"Hello!\")\n"
            "  [timeout_sec] per-request timeout (default: 60)\n",
            prog);
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

/*
 * Append `s` to a JSON string body: opening quote already written,
 * caller appends the closing quote. Escapes ", \, and control chars.
 */
static void json_escape_append(char *dst, size_t dstsz, size_t *pos,
                               const char *s)
{
    for (const char *p = s; *p && *pos + 8 < dstsz; p++) {
        char buf[8];
        size_t n;
        switch (*p) {
        case '"':  n = snprintf(buf, sizeof buf, "\\\""); break;
        case '\\': n = snprintf(buf, sizeof buf, "\\\\"); break;
        case '\n': n = snprintf(buf, sizeof buf, "\\n");  break;
        case '\t': n = snprintf(buf, sizeof buf, "\\t");  break;
        case '\r': n = snprintf(buf, sizeof buf, "\\r");  break;
        default:
            if ((unsigned char)*p < 0x20)
                n = snprintf(buf, sizeof buf, "\\u%04x", (unsigned char)*p);
            else
                n = snprintf(buf, sizeof buf, "%c", *p);
        }
        if (n < 0 || (size_t)n >= sizeof buf) return;
        memcpy(dst + *pos, buf, (size_t)n);
        *pos += (size_t)n;
    }
}

/* One backend step: print the line, return 0 or die with rc. */
static int do_request(const char *what, const char *method, const char *url,
                      const char *json_body, const char *api_key,
                      int timeout_sec)
{
    backend_response_t res;
    int rc = backend_request(method, url, json_body, api_key, timeout_sec, &res);
    printf("%s %s\n    -> %s", what, url, backend_strerror(rc));
    if (rc != BACKEND_OK) {
        printf("\n");
        return 1;
    }
    printf("    -> HTTP %d (%lld ms)%s", res.http_status,
           (long long)res.latency_ms, res.body[0] ? " " : "");
    if (res.body[0]) {
        printf("\n    body: %s", res.body);
    }
    printf("\n");
    free(res.body);
    return res.http_status == 200 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) { usage(argv[0]); return 3; }
    const char *base_in  = argv[1];
    const char *api_key  = argv[2];
    const char *model    = argc > 3 ? argv[3] : NULL;
    const char *prompt   = argc > 4 ? argv[4] : "Hello!";
    int timeout_sec      = argc > 5 ? atoi(argv[5]) : 60;
    if (timeout_sec <= 0) timeout_sec = 60;

    char base[1024];
    rtrim_slash(base, sizeof base, base_in);

    const char *auth = (api_key[0] && strcmp(api_key, "-") != 0)
                           ? api_key : NULL;

    char url[1200];

    /* ---- 1. /health ---- */
    snprintf(url, sizeof url, "%s/health", base);
    if (do_request("1. health  GET ", "GET", url, NULL, auth, timeout_sec) != 0)
        return 1;

    /* ---- 2. /v1/models ---- */
    snprintf(url, sizeof url, "%s/v1/models", base);
    backend_response_t res;
    int rc = backend_request("GET", url, NULL, auth, timeout_sec, &res);
    printf("2. models   GET %s\n    -> %s", url, backend_strerror(rc));
    if (rc != BACKEND_OK || res.http_status != 200) {
        if (rc == BACKEND_OK) printf(" (HTTP %d)", res.http_status);
        printf("\n");
        free(res.body);
        return 1;
    }
    printf(" (HTTP %d, %lld ms)\n", res.http_status,
           (long long)res.latency_ms);

    const char *model_id = NULL;
    int n_ids = 0, in_list = 0;
    char ids[8][256];
    cJSON *root = cJSON_Parse(res.body);
    if (root) {
        cJSON *arr = cJSON_GetObjectItem(root, "data");
        if (cJSON_IsArray(arr)) {
            n_ids = cJSON_GetArraySize(arr);
            for (int i = 0; i < n_ids && i < 8; i++) {
                cJSON *id = cJSON_GetObjectItem(cJSON_GetArrayItem(arr, i), "id");
                if (!cJSON_IsString(id) || !id->valuestring) continue;
                snprintf(ids[i], sizeof ids[i], "%s", id->valuestring);
                if (!model_id) model_id = id->valuestring;
                if (model && strcmp(model, id->valuestring) == 0) in_list = 1;
            }
        }
        printf("    ids:");
        for (int i = 0; i < n_ids && i < 8; i++)
            if (ids[i][0]) printf(" %s", ids[i]);
        printf("\n");
        cJSON_Delete(root);
    }
    free(res.body);

    if (!model_id) {
        printf("no model id reported by /v1/models\n");
        return 1;
    }
    if (model && !in_list) {
        printf("model \"%s\" is not in the server's /v1/models list\n", model);
        return 1;
    }
    if (!model) model = model_id;

    /* ---- 3. /v1/chat/completions ---- */
    char body[8192];
    size_t pos = 0;
    pos += (size_t)snprintf(body + pos, sizeof body - pos,
                           "{\"model\":\"");
    json_escape_append(body, sizeof body, &pos, model);
    pos += (size_t)snprintf(body + pos, sizeof body - pos,
                           "\",\"messages\":[{\"role\":\"user\",\"content\":\"");
    json_escape_append(body, sizeof body, &pos, prompt);
    pos += (size_t)snprintf(body + pos, sizeof body - pos,
                           "\"}],\"stream\":false}");
    body[pos] = '\0';

    snprintf(url, sizeof url, "%s/v1/chat/completions", base);
    rc = backend_request("POST", url, body, auth, timeout_sec, &res);
    printf("3. chat     POST %s\n    -> %s", url, backend_strerror(rc));
    if (rc != BACKEND_OK) {
        printf("\n");
        return 1;
    }
    printf(" (HTTP %d, %lld ms)\n", res.http_status,
           (long long)res.latency_ms);
    if (res.http_status != 200) {
        printf("    body: %s\n", res.body);
        free(res.body);
        return 1;
    }

    int ok = 0;
    cJSON *root2 = cJSON_Parse(res.body);
    if (root2) {
        cJSON *choices = cJSON_GetObjectItem(root2, "choices");
        if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
            cJSON *msg = cJSON_GetObjectItem(
                cJSON_GetArrayItem(choices, 0), "message");
            cJSON *content = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
            if (cJSON_IsString(content) && content->valuestring &&
                content->valuestring[0]) {
                printf("    content: %s\n", content->valuestring);
                ok = 1;
            }
        }
        cJSON *usage = cJSON_GetObjectItem(root2, "usage");
        if (cJSON_IsObject(usage)) {
            const char *names[] = { "prompt_tokens", "completion_tokens",
                                    "total_tokens" };
            printf("    usage:");
            for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
                cJSON *v = cJSON_GetObjectItem(usage, names[i]);
                if (cJSON_IsNumber(v)) printf(" %s=%d", names[i], (int)v->valuedouble);
            }
            printf("\n");
        }
        cJSON_Delete(root2);
    }
    free(res.body);

    if (!ok) {
        printf("    no non-empty completion in response\n");
        return 1;
    }
    printf("OK\n");
    return 0;
}
