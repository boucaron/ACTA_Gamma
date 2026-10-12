/*
 * test_dialect.c — unit tests for src/dialect.{h,c} (no HTTP, no DB).
 *
 * Covers:
 *   1. name -> dialect mapping, and the strict rejection of everything
 *      that is not a canonical name (no aliases, no case folding, no
 *      trimming: an ambiguous backend must fail loudly)
 *   2. dialect_get / dialect_name
 *   3. the capability flags of the dialect matrix (fetch_catalog,
 *      sends_top_k)
 *   4. the two response_format envelopes, byte for byte, including the
 *      deep copy of the skill schema (the caller's schema is never
 *      modified or consumed)
 *   5. the context-ceiling extraction of each dialect: the dialect's own
 *      field first, the other dialect's field accepted, /health as the
 *      last resort, "none" when the server reports nothing — and the
 *      source label that goes into the log
 *
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "dialect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

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

static void check_str(const char *got, const char *want, const char *what)
{
    checks++;
    if (got && strcmp(got, want) == 0) {
        printf("  PASS %s\n", what);
    } else {
        printf("  FAIL %s\n    got:  %s\n    want: %s\n", what,
               got ? got : "(null)", want);
        failures++;
    }
}

static void check_long(long got, long want, const char *what)
{
    checks++;
    if (got == want) {
        printf("  PASS %s\n", what);
    } else {
        printf("  FAIL %s (got %ld, want %ld)\n", what, got, want);
        failures++;
    }
}

/* The skill output_schema used by the envelope tests. */
#define SCHEMA_SRC \
    "{\"type\":\"object\",\"properties\":{\"label\":{\"type\":\"string\"}}," \
    "\"required\":[\"label\"]}"

static char *print_of(const cJSON *v)
{
    return cJSON_PrintUnformatted(v);
}

/* -- 1. name mapping ------------------------------------------------- */

static void test_from_string(void)
{
    printf("== dialect: name mapping\n");

    check(dialect_from_string("llamacpp") == DIALECT_LLAMACPP,
          "\"llamacpp\" -> DIALECT_LLAMACPP");
    check(dialect_from_string("openai") == DIALECT_OPENAI,
          "\"openai\" -> DIALECT_OPENAI");

    /* Everything else is unknown: no aliases, no case folding, no
     * trimming, no empty string, no NULL. */
    static const char *bad[] = {
        NULL, "", "llama", "llama.cpp", "llama-server", "local", "vllm",
        "LM Studio", "openai_strict", "OpenAI", "OPENAI", "llamacpp ",
        " llamacpp", "openai\n", "openai-completions"
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        check(dialect_from_string(bad[i]) == DIALECT_UNKNOWN,
              "non-canonical backend -> DIALECT_UNKNOWN");

    check(dialect_from_string("openai") != DIALECT_LLAMACPP,
          "the two dialects are distinct ids");
}

/* -- 2. dialect_get / dialect_name ----------------------------------- */

static void test_get_name(void)
{
    printf("== dialect: get + name\n");

    const dialect_t *lc = dialect_get(DIALECT_LLAMACPP);
    const dialect_t *oa = dialect_get(DIALECT_OPENAI);
    check(lc != NULL, "dialect_get(llamacpp) resolves");
    check(oa != NULL, "dialect_get(openai) resolves");
    check(dialect_get(DIALECT_UNKNOWN) == NULL,
          "dialect_get(unknown) -> NULL");
    check(dialect_get(99) == NULL, "dialect_get(bogus id) -> NULL");

    check_str(dialect_name(DIALECT_LLAMACPP), "llamacpp",
              "dialect_name(llamacpp)");
    check_str(dialect_name(DIALECT_OPENAI), "openai",
              "dialect_name(openai)");
    check_str(dialect_name(DIALECT_UNKNOWN), "(unknown)",
              "dialect_name(unknown)");

    /* The id stored in the descriptor matches the lookup key. */
    check(lc && lc->id == DIALECT_LLAMACPP,
          "descriptor id matches (llamacpp)");
    check(oa && oa->id == DIALECT_OPENAI,
          "descriptor id matches (openai)");

    /* The accepted vocabulary is stated once, for every error message. */
    check_str(dialect_expected_names(), "llamacpp or openai",
              "the accepted-vocabulary wording");
}

/* -- 3. capability flags (the dialect matrix) ------------------------ */

static void test_flags(void)
{
    printf("== dialect: capability flags\n");

    const dialect_t *lc = dialect_get(DIALECT_LLAMACPP);
    const dialect_t *oa = dialect_get(DIALECT_OPENAI);
    if (!lc || !oa) {
        check(0, "descriptors present");
        return;
    }
    check(lc->fetch_catalog == 1,
          "llamacpp audits the catalog with GET /");
    check(oa->fetch_catalog == 0,
          "openai has no catalog endpoint: the call is skipped");
    check(lc->sends_top_k == 1, "llamacpp sends top_k");
    check(oa->sends_top_k == 0, "openai omits top_k");
    check(lc->build_response_format != NULL,
          "llamacpp has a response_format builder");
    check(oa->build_response_format != NULL,
          "openai has a response_format builder");
    check(lc->max_context_of != NULL, "llamacpp has a ceiling extractor");
    check(oa->max_context_of != NULL, "openai has a ceiling extractor");
}

/* -- 4. response_format envelopes ------------------------------------ */

static void test_envelopes(void)
{
    printf("== dialect: response_format envelopes\n");

    const dialect_t *lc = dialect_get(DIALECT_LLAMACPP);
    const dialect_t *oa = dialect_get(DIALECT_OPENAI);
    if (!lc || !oa) { check(0, "descriptors present"); return; }

    cJSON *schema = cJSON_Parse(SCHEMA_SRC);
    if (!schema) { check(0, "schema parses"); return; }

    cJSON *rf_lc = lc->build_response_format(schema);
    cJSON *rf_oa = oa->build_response_format(schema);

    char *s_lc = rf_lc ? print_of(rf_lc) : NULL;
    char *s_oa = rf_oa ? print_of(rf_oa) : NULL;

    /* llama.cpp shape: the schema sits at the top level of the
     * envelope (docs/llamacpp_server_contract.md section 4). */
    check_str(s_lc,
              "{\"type\":\"json_schema\",\"schema\":" SCHEMA_SRC "}",
              "llamacpp envelope");

    /* OpenAI shape: name + strict + schema nested under json_schema —
     * the envelope Strata requires and llama.cpp rejects. */
    check_str(s_oa,
              "{\"type\":\"json_schema\",\"json_schema\":{\"name\":"
              "\"acta_output\",\"strict\":true,\"schema\":" SCHEMA_SRC "}}",
              "openai envelope");

    /* The two envelopes are different: that IS the dialect difference. */
    check(s_lc && s_oa && strcmp(s_lc, s_oa) != 0,
          "the two envelopes differ");

    /* The envelope carries a deep copy: mutating it must not touch the
     * caller's schema, which the caller still owns and frees. */
    cJSON *copy_lc = cJSON_GetObjectItem(rf_lc, "schema");
    cJSON_AddItemToObject(copy_lc, "injected", cJSON_CreateString("x"));
    char *schema_after = print_of(schema);
    check_str(schema_after, SCHEMA_SRC,
              "the caller's schema is untouched by the envelope");
    free(schema_after);

    /* Building twice yields the same bytes (no hidden state): the same
     * skill + model revision must produce the same request body. */
    cJSON *again = lc->build_response_format(schema);
    char *s_again = again ? print_of(again) : NULL;
    check_str(s_again, s_lc, "the envelope is deterministic");
    free(s_again);
    cJSON_Delete(again);

    /* NULL schema -> no envelope (the caller then sends no
     * response_format at all). */
    check(lc->build_response_format(NULL) == NULL,
          "llamacpp builder: NULL schema -> NULL");
    check(oa->build_response_format(NULL) == NULL,
          "openai builder: NULL schema -> NULL");

    free(s_lc);
    free(s_oa);
    cJSON_Delete(rf_lc);
    cJSON_Delete(rf_oa);
    cJSON_Delete(schema);
}

/* -- 5. context ceiling extraction ----------------------------------- */

struct ceiling_case {
    const char *what;
    const char *entry_src;   /* NULL -> entry is NULL */
    const char *health_src;  /* NULL -> no /health body */
    long        want_lc;
    const char *src_lc;
    long        want_oa;
    const char *src_oa;
};

static void test_ceiling(void)
{
    printf("== dialect: context ceiling\n");

    const dialect_t *lc = dialect_get(DIALECT_LLAMACPP);
    const dialect_t *oa = dialect_get(DIALECT_OPENAI);
    if (!lc || !oa) { check(0, "descriptors present"); return; }

    static const struct ceiling_case cases[] = {
        { "both fields present: each dialect prefers its own",
          "{\"id\":\"m\",\"max_context\":162000,\"meta\":{\"n_ctx\":204800}}",
          "{\"status\":\"ok\",\"max_context\":999999}",
          162000, "entry.max_context", 204800, "entry.meta.n_ctx" },
        { "llama.cpp shape only",
          "{\"id\":\"m\",\"max_context\":162000}",
          NULL, 162000, "entry.max_context", 162000, "entry.max_context" },
        { "Strata shape only (meta.n_ctx, no top-level max_context)",
          "{\"id\":\"m\",\"meta\":{\"n_ctx\":204800}}",
          "{\"status\":\"ok\",\"max_context\":204800}",
          204800, "entry.meta.n_ctx", 204800, "entry.meta.n_ctx" },
        { "no entry ceiling: /health is the last resort",
          "{\"id\":\"m\"}",
          "{\"status\":\"ok\",\"max_context\":204800}",
          204800, "health.max_context", 204800, "health.max_context" },
        { "nothing reported",
          "{\"id\":\"m\"}", NULL, 0, "none", 0, "none" },
        { "zero / negative values are not a ceiling",
          "{\"id\":\"m\",\"max_context\":0,\"meta\":{\"n_ctx\":-5}}",
          "{\"status\":\"ok\",\"max_context\":0}", 0, "none", 0, "none" },
        { "meta present but without n_ctx",
          "{\"id\":\"m\",\"meta\":{\"n_params\":1}}",
          NULL, 0, "none", 0, "none" },
        { "no entry at all (defensive)",
          NULL, "{\"status\":\"ok\",\"max_context\":4096}",
          4096, "health.max_context", 4096, "health.max_context" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        cJSON *entry = cases[i].entry_src ? cJSON_Parse(cases[i].entry_src)
                                          : NULL;
        cJSON *health = cases[i].health_src ? cJSON_Parse(cases[i].health_src)
                                            : NULL;
        if (cases[i].entry_src && !entry) {
            check(0, cases[i].what);
            cJSON_Delete(health);
            continue;
        }
        const char *src = NULL;
        long v = lc->max_context_of(entry, health, &src);
        check_long(v, cases[i].want_lc, cases[i].what);
        check_str(src, cases[i].src_lc, "llamacpp ceiling source");
        src = NULL;
        v = oa->max_context_of(entry, health, &src);
        check_long(v, cases[i].want_oa, cases[i].what);
        check_str(src, cases[i].src_oa, "openai ceiling source");

        /* A NULL source pointer is tolerated (callers that do not care
         * which field was used). */
        v = lc->max_context_of(entry, health, NULL);
        check_long(v, cases[i].want_lc, "source out-param is optional");

        cJSON_Delete(entry);
        cJSON_Delete(health);
    }
}

int main(void)
{
    test_from_string();
    test_get_name();
    test_flags();
    test_envelopes();
    test_ceiling();

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
