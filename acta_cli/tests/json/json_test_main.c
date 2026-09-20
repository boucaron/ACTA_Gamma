/* JSON layer unit tests (J1) — no DB, no argv seam.
 *
 * Staged from simple to complex, mirroring docs/cli_spec.md:
 *   Tier 1  json_validate: any well-formed JSON document
 *   Tier 2  root rejection: all seven parsers require an object root
 *   Tier 3  one field at a time (model): absent / null / wrong type /
 *           whitespace / case sensitivity / unknown keys
 *   Tier 4  strict integer contract: 0 sentinel, negatives, fractions,
 *           INT bounds, wrong types, exponent forms
 *   Tier 5  full spec payloads: all seven entities, wire keys verbatim
 *           from cli_spec.md input columns
 *   Tier 6  error-path hygiene: on -1 the struct is left fully zeroed
 *           (partially-copied strings freed) — every parser
 *   Tier 7  edge cases: escapes, unicode, empty/long strings, nesting
 *           limit, validate-vs-parser asymmetry
 */
#include "json.h"
#include <acta_db.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── minimal assertion harness ────────────────────────────────────── */

static int g_fail  = 0;
static int g_total = 0;

static void check(int cond, const char *what)
{
    g_total++;
    if (!cond) {
        g_fail++;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

#define T(cond)         check((cond) != 0, #cond)
#define TEQ(got, want)  check((got) == (want), #got " == " #want)
#define TNTNULL(p)      check((p) != NULL, #p " != NULL")
#define TNULL(p)        check((p) == NULL, #p " == NULL")
#define TSTREQ(a, b)    check(strcmp((a), (b)) == 0, #a " == " #b)

/* All seven parsers share the signature (blob, out) → 0/-1. */
typedef int (*parse_fn)(const char *blob, void *out);

/* ── local freeers (mirror json.c field ownership; success paths) ──── */

static void fmodel(model_t *m)
{
    free(m->name); free(m->description); free(m->backend);
    free(m->base_url); free(m->model_identifier); free(m->configuration);
}

static void fcontext(context_t *c)
{
    free(c->type); free(c->content); free(c->content_hash);
    free(c->metadata); free(c->created_at);
}

static void fexec(execution_t *e)
{
    free(e->status); free(e->prompt); free(e->created_at);
}

static void felog(execution_log_t *l)
{
    free(l->level); free(l->event); free(l->message);
    free(l->metadata); free(l->created_at);
}

static void fmf(model_folder_t *f)
{
    free(f->name); free(f->created_at); free(f->updated_at); free(f->deleted_at);
}

static void fsf(skill_folder_t *f)
{
    free(f->name); free(f->created_at); free(f->updated_at); free(f->deleted_at);
}

static void fskill(skill_t *s)
{
    free(s->name); free(s->description); free(s->prompt_template);
    free(s->output_schema); free(s->created_at); free(s->updated_at);
    free(s->deleted_at);
}

/* ── Tier 1: json_validate ────────────────────────────────────────── */

static void tier1_validate(void)
{
    /* scalars */
    TEQ(json_validate("123"), 0);
    TEQ(json_validate("-1.5e3"), 0);
    TEQ(json_validate("true"), 0);
    TEQ(json_validate("false"), 0);
    TEQ(json_validate("null"), 0);
    TEQ(json_validate("\"str\""), 0);
    /* containers */
    TEQ(json_validate("[]"), 0);
    TEQ(json_validate("[1,2,3]"), 0);
    TEQ(json_validate("{}"), 0);
    TEQ(json_validate("{\"a\":{\"b\":[1,{\"c\":null}]}}"), 0);
    /* malformed */
    TEQ(json_validate("{"), -1);
    TEQ(json_validate("}"), -1);
    TEQ(json_validate("[1,]"), -1);
    TEQ(json_validate("nul"), -1);
    /* cJSON 1.7.x parse_number is a strtod-based scan: leading zeros
     * ("01") are accepted — no strict flag exists for that. */
    TEQ(json_validate("01"), 0);
    TEQ(json_validate("{\"a\""), -1);
    TEQ(json_validate("{\"a\" 1}"), -1);
    /* trailing garbage rejected via require_null_terminated */
    TEQ(json_validate("{} x"), -1);
    /* trailing whitespace (e.g. stdin newline) still fine */
    TEQ(json_validate("{}\n"), 0);
    TEQ(json_validate(""), -1);
    TEQ(json_validate(NULL), -1);
}

/* ── Tier 2: root must be an object (all seven parsers) ───────────── */

static void expect_reject(parse_fn fn, const char *blob, void *dst)
{
    TEQ(fn(blob, dst), -1);
}

static void tier2_root_rejection(void)
{
    model_t m; memset(&m, 0, sizeof m);
    context_t c; memset(&c, 0, sizeof c);
    execution_t e; memset(&e, 0, sizeof e);
    execution_log_t l; memset(&l, 0, sizeof l);
    model_folder_t mf; memset(&mf, 0, sizeof mf);
    skill_folder_t sf; memset(&sf, 0, sizeof sf);
    skill_t s; memset(&s, 0, sizeof s);

    struct { parse_fn fn; void *dst; } ps[7] = {
        { json_parse_model,          &m  },
        { json_parse_context,        &c  },
        { json_parse_execution,      &e  },
        { json_parse_execution_log,  &l  },
        { json_parse_model_folder,   &mf },
        { json_parse_skill_folder,   &sf },
        { json_parse_skill,          &s  },
    };

    const char *bad_roots[] = {
        "[1,2,3]",       /* array */
        "5",             /* number */
        "null",          /* literal */
        "\"str\"",       /* string */
        "true",          /* bool */
        "{bad",          /* malformed */
        NULL,            /* NULL blob */
    };

    for (size_t i = 0; i < 7; i++)
        for (size_t j = 0; j < sizeof bad_roots / sizeof bad_roots[0]; j++)
            expect_reject(ps[i].fn, bad_roots[j], ps[i].dst);

    /* NULL out pointer, even with a valid blob */
    for (size_t i = 0; i < 7; i++)
        TEQ(ps[i].fn("{}", NULL), -1);

    /* the empty object IS a valid parse for every entity:
     * every field optional, ids default to the 0 sentinel */
    for (size_t i = 0; i < 7; i++)
        TEQ(ps[i].fn("{}", ps[i].dst), 0);
}

/* ── Tier 3: model, one field at a time ───────────────────────────── */

static void tier3_single_fields(void)
{
    model_t m;

    /* empty object: all defaults (0 / NULL) */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{}", &m), 0);
    TEQ(m.folder_id, 0);
    TNULL(m.name); TNULL(m.description); TNULL(m.backend);
    TNULL(m.base_url); TNULL(m.model_identifier); TNULL(m.configuration);

    /* one field */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":\"x\"}", &m), 0);
    TSTREQ(m.name, "x");
    TNULL(m.description);
    fmodel(&m);

    /* JSON null is a valid string field value: NULL, no error */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":null}", &m), 0);
    TNULL(m.name);

    /* wrong type (number): treated as absent → NULL, no error */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":123}", &m), 0);
    TNULL(m.name);

    /* wrong type (object): absent → NULL */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":{\"a\":1}}", &m), 0);
    TNULL(m.name);

    /* whitespace and pretty form are accepted */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("  { \"name\" : \"x\" }  ", &m), 0);
    TSTREQ(m.name, "x");
    fmodel(&m);

    memset(&m, 0, sizeof m);
    TEQ(json_parse_model(
            "  {\n    \"name\" : \"x\",\n    \"folder_id\" : 7\n  }\n", &m), 0);
    TSTREQ(m.name, "x");
    TEQ(m.folder_id, 7);
    fmodel(&m);

    /* KI-2: keys are case-sensitive AND unknown top-level keys are
     * rejected (the table is the exact cli_spec.md wire key set).
     * Struct is left fully zeroed on -1 (json.h contract). */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"Name\":\"x\"}", &m), -1);
    TNULL(m.name);

    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":\"x\",\"unknown\":1,\"nam\":\"typo\"}", &m), -1);
    TNULL(m.name);
    TEQ(m.folder_id, 0);
}

/* ── Tier 4: strict integer contract ─────────────────────────────── */

static void tier4_int_contract(void)
{
    model_t m;

    /* 0 is the valid sentinel (root folder / omitted) */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":0}", &m), 0);
    TEQ(m.folder_id, 0);

    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":\"x\"}", &m), 0);   /* absent */
    TEQ(m.folder_id, 0);
    fmodel(&m);

    /* plain positive */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":7}", &m), 0);
    TEQ(m.folder_id, 7);

    /* negative → error, not clamped to 0 */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":-1}", &m), -1);
    TEQ(m.folder_id, 0);
    TNULL(m.name);

    /* fractional → error, not truncated (3.7 → 3 was the old bug) */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":3.7}", &m), -1);

    /* exact integer in exponent form → ok */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":1e3}", &m), 0);
    TEQ(m.folder_id, 1000);

    /* 1.0 is an exact integer */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":1.0}", &m), 0);
    TEQ(m.folder_id, 1);

    /* INT_MAX ok; INT_MAX+1 overflows int → error, not a wrap */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":2147483647}", &m), 0);
    TEQ(m.folder_id, INT_MAX);

    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":2147483648}", &m), -1);

    /* astronomically large double → error */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":1e300}", &m), -1);

    /* INT_MIN: in range but negative → error for an id */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":-2147483648}", &m), -1);

    /* wrong type (string) → treated as absent → 0, no error */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":\"7\"}", &m), 0);
    TEQ(m.folder_id, 0);

    /* wrong type (bool) → absent → 0 */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"folder_id\":true}", &m), 0);
    TEQ(m.folder_id, 0);

    /* same contract applies to every id field; spot-check context.id
     * and execution.parent_execution_id (0 = no parent sentinel) */
    context_t c; memset(&c, 0, sizeof c);
    TEQ(json_parse_context("{\"id\":-1}", &c), -1);
    TEQ(json_parse_context("{\"id\":5}", &c), 0);
    TEQ(c.id, 5);
    fcontext(&c);

    execution_t e; memset(&e, 0, sizeof e);
    TEQ(json_parse_execution(
            "{\"context_id\":1,\"skill_revision_id\":2,"
            "\"model_revision_id\":3,\"parent_execution_id\":0}", &e), 0);
    TEQ(e.context_id, 1);
    TEQ(e.skill_revision_id, 2);
    TEQ(e.model_revision_id, 3);
    TEQ(e.parent_execution_id, 0);
    TEQ(json_parse_execution("{\"parent_execution_id\":-1}", &e), -1);
}

/* ── Tier 5: full spec payloads (wire keys verbatim from cli_spec.md) ─ */

static void tier5_full_payloads(void)
{
    /* model: flags or JSON {name*, backend*, model_identifier*,
     * folder_id, description, base_url, configuration} */
    model_t m; memset(&m, 0, sizeof m);
    TEQ(json_parse_model(
            "{\"folder_id\":7,\"name\":\"m1\",\"description\":\"d\","
            "\"backend\":\"openai\",\"base_url\":\"https://api/\","
            "\"model_identifier\":\"gpt-x\","
            "\"configuration\":\"{\\\"k\\\":1}\"}",
            &m), 0);
    TEQ(m.folder_id, 7);
    TSTREQ(m.name, "m1");
    TSTREQ(m.description, "d");
    TSTREQ(m.backend, "openai");
    TSTREQ(m.base_url, "https://api/");
    TSTREQ(m.model_identifier, "gpt-x");
    TSTREQ(m.configuration, "{\"k\":1}");
    fmodel(&m);

    /* context: {type*, content*, hash*, metadata} (+ id, created_at) */
    context_t c; memset(&c, 0, sizeof c);
    TEQ(json_parse_context(
            "{\"id\":3,\"type\":\"file\",\"content\":\"hello \\\"world\\\"\","
            "\"hash\":\"h1\",\"metadata\":\"{\\\"src\\\":\\\"x\\\"}\","
            "\"created_at\":\"2024-01-01T00:00:00Z\"}", &c), 0);
    TEQ(c.id, 3);
    TSTREQ(c.type, "file");
    TSTREQ(c.content, "hello \"world\"");
    TSTREQ(c.content_hash, "h1");            /* wire key is "hash" */
    TSTREQ(c.metadata, "{\"src\":\"x\"}");
    TSTREQ(c.created_at, "2024-01-01T00:00:00Z");
    fcontext(&c);

    /* exec: {context_id*, skill_revision_id*, model_revision_id*,
     * parent_execution_id} (+ id, status, created_at).  'prompt' is
     * no longer a create wire key (legacy column, never written) —
     * a body carrying it is rejected as an unknown key. */
    execution_t e; memset(&e, 0, sizeof e);
    TEQ(json_parse_execution(
            "{\"id\":4,\"status\":\"pending\","
            "\"context_id\":1,"
            "\"skill_revision_id\":2,\"model_revision_id\":3,"
            "\"parent_execution_id\":0,\"created_at\":\"ts\"}", &e), 0);
    TEQ(e.id, 4);
    TSTREQ(e.status, "pending");
    TNULL(e.prompt);                          /* legacy column: not parsed */
    TEQ(e.context_id, 1);
    TEQ(e.skill_revision_id, 2);
    TEQ(e.model_revision_id, 3);
    TEQ(e.parent_execution_id, 0);
    TSTREQ(e.created_at, "ts");
    fexec(&e);

    /* log: {execution_id*, level*, event*, message, metadata}
     * (+ id, created_at) */
    execution_log_t l; memset(&l, 0, sizeof l);
    TEQ(json_parse_execution_log(
            "{\"id\":1,\"execution_id\":4,\"level\":\"info\",\"event\":\"ev\","
            "\"message\":\"msg\",\"metadata\":\"md\",\"created_at\":\"ts\"}",
            &l), 0);
    TEQ(l.id, 1);
    TEQ(l.execution_id, 4);
    TSTREQ(l.level, "info");
    TSTREQ(l.event, "ev");
    TSTREQ(l.message, "msg");
    TSTREQ(l.metadata, "md");
    TSTREQ(l.created_at, "ts");
    felog(&l);

    /* model_folder: {name*, parent_id} (+ id, timestamps) */
    model_folder_t mf; memset(&mf, 0, sizeof mf);
    TEQ(json_parse_model_folder(
            "{\"id\":2,\"name\":\"mf\",\"parent_id\":1,"
            "\"created_at\":\"a\",\"updated_at\":\"b\",\"deleted_at\":null}",
            &mf), 0);
    TEQ(mf.id, 2);
    TSTREQ(mf.name, "mf");
    TEQ(mf.parent_id, 1);
    TSTREQ(mf.created_at, "a");
    TSTREQ(mf.updated_at, "b");
    TNULL(mf.deleted_at);                    /* NULL if live */
    fmf(&mf);

    /* skill_folder: {name*, parent_id} (+ id, timestamps) */
    skill_folder_t sf; memset(&sf, 0, sizeof sf);
    TEQ(json_parse_skill_folder(
            "{\"id\":5,\"name\":\"sf\",\"parent_id\":0,"
            "\"created_at\":\"a\",\"updated_at\":\"b\","
            "\"deleted_at\":\"2024-02-02T00:00:00Z\"}", &sf), 0);
    TEQ(sf.id, 5);
    TSTREQ(sf.name, "sf");
    TEQ(sf.parent_id, 0);                    /* 0 = root */
    TSTREQ(sf.created_at, "a");
    TSTREQ(sf.updated_at, "b");
    TSTREQ(sf.deleted_at, "2024-02-02T00:00:00Z");
    fsf(&sf);

    /* skill: {name*, prompt_template*, folder_id, description,
     * output_schema} (+ id, timestamps) */
    skill_t s; memset(&s, 0, sizeof s);
    TEQ(json_parse_skill(
            "{\"id\":9,\"folder_id\":0,\"name\":\"sk\",\"description\":\"d\","
            "\"prompt_template\":\"do X\",\"output_schema\":\"{\\\"type\\\":"
            "\\\"string\\\"}\",\"created_at\":\"a\",\"updated_at\":\"b\","
            "\"deleted_at\":\"c\"}", &s), 0);
    TEQ(s.id, 9);
    TEQ(s.folder_id, 0);                     /* 0 = root */
    TSTREQ(s.name, "sk");
    TSTREQ(s.description, "d");
    TSTREQ(s.prompt_template, "do X");
    TSTREQ(s.output_schema, "{\"type\":\"string\"}");
    TSTREQ(s.created_at, "a");
    TSTREQ(s.updated_at, "b");
    TSTREQ(s.deleted_at, "c");
    fskill(&s);
}

/* ── Tier 6: error-path hygiene — struct fully zeroed on -1 ───────── */

/* The tables put string fields after id fields in some entities
 * (execution: status before context_id; folders: name before
 * parent_id; log: level/event/message/metadata before execution_id),
 * so a late id failure forces the walker to free already-copied
 * strings.  After -1 every string field must be NULL and every int
 * field 0. */

static void tier6_error_hygiene(void)
{
    model_t m; memset(&m, 0, sizeof m);
    TEQ(json_parse_model(
            "{\"folder_id\":-1,\"name\":\"a\",\"description\":\"b\","
            "\"backend\":\"c\",\"base_url\":\"d\","
            "\"model_identifier\":\"e\",\"configuration\":\"f\"}", &m), -1);
    TEQ(m.folder_id, 0);
    TNULL(m.name); TNULL(m.description); TNULL(m.backend);
    TNULL(m.base_url); TNULL(m.model_identifier); TNULL(m.configuration);

    context_t c; memset(&c, 0, sizeof c);
    TEQ(json_parse_context(
            "{\"id\":-1,\"type\":\"t\",\"content\":\"c\",\"hash\":\"h\","
            "\"metadata\":\"m\",\"created_at\":\"ts\"}", &c), -1);
    TEQ(c.id, 0);
    TNULL(c.type); TNULL(c.content); TNULL(c.content_hash);
    TNULL(c.metadata); TNULL(c.created_at);

    execution_t e; memset(&e, 0, sizeof e);
    TEQ(json_parse_execution(
            "{\"id\":4,\"status\":\"s\",\"context_id\":-5}",
            &e), -1);
    TNULL(e.status); TNULL(e.prompt); TNULL(e.created_at);
    TEQ(e.id, 0); TEQ(e.context_id, 0);
    TEQ(e.skill_revision_id, 0); TEQ(e.model_revision_id, 0);
    TEQ(e.parent_execution_id, 0);

    execution_log_t l; memset(&l, 0, sizeof l);
    TEQ(json_parse_execution_log(
            "{\"id\":1,\"level\":\"info\",\"event\":\"e\",\"message\":\"m\","
            "\"metadata\":\"md\",\"created_at\":\"ts\",\"execution_id\":-9}",
            &l), -1);
    TNULL(l.level); TNULL(l.event); TNULL(l.message);
    TNULL(l.metadata); TNULL(l.created_at);
    TEQ(l.id, 0); TEQ(l.execution_id, 0);

    model_folder_t mf; memset(&mf, 0, sizeof mf);
    TEQ(json_parse_model_folder(
            "{\"id\":1,\"name\":\"n\",\"parent_id\":-1}", &mf), -1);
    TNULL(mf.name); TNULL(mf.created_at); TNULL(mf.updated_at);
    TNULL(mf.deleted_at);
    TEQ(mf.id, 0); TEQ(mf.parent_id, 0);

    skill_folder_t sf; memset(&sf, 0, sizeof sf);
    TEQ(json_parse_skill_folder(
            "{\"id\":1,\"name\":\"n\",\"parent_id\":-1}", &sf), -1);
    TNULL(sf.name); TNULL(sf.created_at); TNULL(sf.updated_at);
    TNULL(sf.deleted_at);
    TEQ(sf.id, 0); TEQ(sf.parent_id, 0);

    skill_t s; memset(&s, 0, sizeof s);
    TEQ(json_parse_skill(
            "{\"id\":1,\"folder_id\":-1,\"name\":\"n\",\"description\":\"d\","
            "\"prompt_template\":\"p\",\"output_schema\":\"o\"}", &s), -1);
    TNULL(s.name); TNULL(s.description); TNULL(s.prompt_template);
    TNULL(s.output_schema); TNULL(s.created_at); TNULL(s.updated_at);
    TNULL(s.deleted_at);
    TEQ(s.id, 0); TEQ(s.folder_id, 0);
}

/* ── Tier 7: edge cases ───────────────────────────────────────────── */

static void tier7_edges(void)
{
    model_t m;

    /* empty string is a valid field ("" ≠ NULL) */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":\"\"}", &m), 0);
    TNTNULL(m.name);
    TEQ(strlen(m.name), (size_t)0);
    fmodel(&m);

    /* escapes: quote, backslash, newline, tab */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":\"a\\\"b\\\\c\\nd\\te\"}", &m), 0);
    TSTREQ(m.name, "a\"b\\c\nd\te");
    fmodel(&m);

    /* unicode escape \u00e9 → UTF-8 "é" */
    memset(&m, 0, sizeof m);
    TEQ(json_parse_model("{\"name\":\"\\u00e9\"}", &m), 0);
    TSTREQ(m.name, "\xc3\xa9");
    fmodel(&m);

    /* long string (10,000 chars): full copy, exact length */
    {
        size_t n = 10000;
        char *buf  = (char *)malloc(n + 40);
        char *json = (char *)malloc(n + 40);
        if (!buf || !json) { free(buf); free(json); T(0); return; }
        memset(buf, 'a', n);
        buf[n] = '\0';
        snprintf(json, n + 40, "{\"name\":\"%s\"}", buf);
        memset(&m, 0, sizeof m);
        TEQ(json_parse_model(json, &m), 0);
        TEQ(m.name ? strlen(m.name) : 0, n);
        if (m.name)
            T(memcmp(m.name, buf, n) == 0);
        fmodel(&m);
        free(buf);
        free(json);
    }

    /* nesting limit: > CJSON_NESTING_LIMIT (1000) brackets reject */
    {
        size_t n = 1500;
        char *deep = (char *)malloc(2 * n + 2);
        if (!deep) { T(0); return; }
        for (size_t i = 0; i < n; i++) deep[i] = '[';
        for (size_t i = n; i < 2 * n; i++) deep[i] = ']';
        deep[2 * n] = '\0';
        TEQ(json_validate(deep), -1);
        memset(&m, 0, sizeof m);
        TEQ(json_parse_model(deep, &m), -1);
        free(deep);
    }

    /* validate accepts anything well-formed; parsers require an
     * object root — the documented asymmetry */
    TEQ(json_validate("[]"), 0);
    {
        model_t m2; memset(&m2, 0, sizeof m2);
        TEQ(json_parse_model("[]", &m2), -1);
    }
}

int main(void)
{
    tier1_validate();
    tier2_root_rejection();
    tier3_single_fields();
    tier4_int_contract();
    tier5_full_payloads();
    tier6_error_hygiene();
    tier7_edges();

    if (g_fail == 0) {
        printf("PASS: all json tests passed (%d assertions)\n", g_total);
        return 0;
    }
    printf("FAIL: %d of %d assertion(s) failed\n", g_fail, g_total);
    return 1;
}
