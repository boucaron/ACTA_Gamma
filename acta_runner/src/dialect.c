/*
 * dialect.c — the two backend dialects (llamacpp, openai).
 *
 * Pure descriptor + the two request/parse shapes that genuinely differ.
 * No HTTP here: callers (run.c, backend.c, check.c) own the requests and
 * hand the parsed JSON to these helpers. See dialect.h for the contract
 * and docs/plans/backend_dialects.md §2 for the dialect matrix.
 */

#include "dialect.h"

#include <string.h>

/*
 * Name given to the schema in the openai envelope. OpenAI-compatible
 * servers require a JSON-name-like schema name; it is a fixed constant
 * (never derived from the skill name or id) so the same skill + model
 * revision always produces the same request body — the replay property
 * the revision design exists to guarantee.
 */
#define ACTA_OPENAI_SCHEMA_NAME "acta_output"

/* Positive integer value of obj[key], or 0 when absent / not a number. */
static long json_positive_int(const cJSON *obj, const char *key)
{
    const cJSON *v = obj
        ? cJSON_GetObjectItem((cJSON *)obj, key) : NULL;
    if (!cJSON_IsNumber(v) || v->valuedouble <= 0.0)
        return 0;
    return (long)v->valuedouble;
}

/* ---- context ceiling extraction ---- */

static long entry_max_context(const cJSON *entry)
{
    return json_positive_int(entry, "max_context");
}

static long entry_meta_n_ctx(const cJSON *entry)
{
    if (!entry)
        return 0;
    return json_positive_int(cJSON_GetObjectItem((cJSON *)entry, "meta"),
                             "n_ctx");
}

static long health_max_context(const cJSON *health_body)
{
    return json_positive_int(health_body, "max_context");
}

/* llamacpp: the router reports the served window as the entry's
 * top-level max_context; meta.n_ctx and /health are fallbacks. */
static long max_context_llamacpp(const cJSON *entry, const cJSON *health,
                                 const char **source)
{
    long v = entry_max_context(entry);
    if (v > 0) {
        if (source) *source = "entry.max_context";
        return v;
    }
    v = entry_meta_n_ctx(entry);
    if (v > 0) {
        if (source) *source = "entry.meta.n_ctx";
        return v;
    }
    v = health_max_context(health);
    if (v > 0) {
        if (source) *source = "health.max_context";
        return v;
    }
    if (source) *source = "none";
    return 0;
}

/* openai: servers carry the window in the entry's meta (Strata:
 * meta.n_ctx); the llama.cpp-style top-level max_context is still
 * accepted, then /health. Same three sources, dialect-first order. */
static long max_context_openai(const cJSON *entry, const cJSON *health,
                               const char **source)
{
    long v = entry_meta_n_ctx(entry);
    if (v > 0) {
        if (source) *source = "entry.meta.n_ctx";
        return v;
    }
    v = entry_max_context(entry);
    if (v > 0) {
        if (source) *source = "entry.max_context";
        return v;
    }
    v = health_max_context(health);
    if (v > 0) {
        if (source) *source = "health.max_context";
        return v;
    }
    if (source) *source = "none";
    return 0;
}

/* ---- response_format envelopes ---- */

/* llama.cpp (docs/llamacpp_server_contract.md §4):
 *   {"type":"json_schema","schema":{…}} */
static cJSON *response_format_llamacpp(const cJSON *schema)
{
    if (!schema)
        return NULL;
    cJSON *rf = cJSON_CreateObject();
    if (!rf)
        return NULL;
    cJSON_AddStringToObject(rf, "type", "json_schema");
    cJSON_AddItemToObject(rf, "schema", cJSON_Duplicate(schema, 1));
    return rf;
}

/* OpenAI strict structured output:
 *   {"type":"json_schema",
 *    "json_schema":{"name":"acta_output","strict":true,"schema":{…}}}
 * llama.cpp rejects this (it wants the schema at the top level) and
 * OpenAI-compatible servers reject the llama.cpp shape with
 * "response_format.json_schema needs a schema object" — the envelope IS
 * the dialect difference (docs/plans/backend_dialects.md §2). */
static cJSON *response_format_openai(const cJSON *schema)
{
    if (!schema)
        return NULL;
    cJSON *rf = cJSON_CreateObject();
    if (!rf)
        return NULL;
    cJSON *js = cJSON_CreateObject();
    if (!js) {
        cJSON_Delete(rf);
        return NULL;
    }
    cJSON_AddStringToObject(rf, "type", "json_schema");
    cJSON_AddStringToObject(js, "name", ACTA_OPENAI_SCHEMA_NAME);
    cJSON_AddBoolToObject(js, "strict", 1);
    cJSON_AddItemToObject(js, "schema", cJSON_Duplicate(schema, 1));
    cJSON_AddItemToObject(rf, "json_schema", js);
    return rf;
}

/* ---- the table ---- */

static const dialect_t DIALECT_TABLE[] = {
    { DIALECT_LLAMACPP, "llamacpp",
      /* fetch_catalog */ 1, /* sends_top_k */ 1,
      response_format_llamacpp, max_context_llamacpp },
    { DIALECT_OPENAI, "openai",
      /* fetch_catalog */ 0, /* sends_top_k */ 0,
      response_format_openai, max_context_openai },
};

#define DIALECT_COUNT (int)(sizeof(DIALECT_TABLE) / sizeof(DIALECT_TABLE[0]))

dialect_id_t dialect_from_string(const char *backend)
{
    if (!backend)
        return DIALECT_UNKNOWN;
    for (int i = 0; i < DIALECT_COUNT; i++)
        if (strcmp(backend, DIALECT_TABLE[i].name) == 0)
            return DIALECT_TABLE[i].id;
    return DIALECT_UNKNOWN;
}

const dialect_t *dialect_get(dialect_id_t id)
{
    for (int i = 0; i < DIALECT_COUNT; i++)
        if (DIALECT_TABLE[i].id == id)
            return &DIALECT_TABLE[i];
    return NULL;
}

const char *dialect_name(dialect_id_t id)
{
    const dialect_t *d = dialect_get(id);
    return d ? d->name : "(unknown)";
}

const char *dialect_expected_names(void)
{
    return "llamacpp or openai";
}
