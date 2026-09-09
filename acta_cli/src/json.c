/*
 * JSON layer — parse (blob → entity struct) only.
 *
 * Source of truth: docs/cli_spec.md.  Each parser below is a table of
 * (JSON key, kind, struct offset) rows whose keys are exactly the
 * wire keys the spec's input column documents for that entity
 * ("flags or JSON <same keys>"):
 *
 *   context        {type, content, hash, metadata}         + id, created_at
 *   model          {name, backend, model_identifier,
 *                    folder_id, description, base_url,
 *                    configuration}
 *   model_folder   {name, parent_id}                       + id, timestamps
 *   skill          {name, prompt_template, folder_id,
 *                    description, output_schema}          + id, timestamps
 *   skill_folder   {name, parent_id}                       + id, timestamps
 *   exec           {context_id, skill_revision_id,
 *                    model_revision_id, prompt,
 *                    parent_execution_id}                 + id, status, created_at
 *   log            {execution_id, level, event, message,
 *                    metadata}                            + id, created_at
 *
 * Field semantics (shared by all entities):
 *   JF_STR  — string field.  Absent / JSON null / wrong type → NULL,
 *             never an error (callers re-validate required fields).
 *   JF_ID   — non-negative int.  0 is a valid sentinel (root folder /
 *             no parent / omitted).  Absent → 0.  Negative, fractional,
 *             or out-of-int-range → parse failure.  Wrong type → absent
 *             (0), like a missing key.
 *
 * On failure the struct is left fully zeroed (partial string copies are
 * freed), so the caller never has to free anything (json.h contract).
 */

#include "json.h"
#include <acta_db.h>   /* entity structs only (model_t, skill_t, ...) —
                          the JSON layer never includes the dispatch header */

#include "cli.h"        /* VLOG */

#include <cjson/cJSON.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ── field descriptors ────────────────────────────────────────────── */

typedef enum { JF_STR, JF_ID } jf_kind_t;

typedef struct {
    const char *key;   /* wire key, verbatim from cli_spec.md */
    jf_kind_t   kind;
    size_t      off;   /* offsetof() in the entity struct */
} jfield_t;

/* ── helpers ───────────────────────────────────────────────────────── */

/* Copy a NUL-terminated string.  Returns 0 with *out set to the
 * malloc'd copy, or -1 on OOM (*out left NULL). */
static int str_dup(const char *s, char **out)
{
    *out = NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return -1;
    memcpy(p, s, n);
    *out = p;
    return 0;
}

/* Extract an optional string field from a cJSON object.
 * Returns 0 with *out set to a malloc'd copy (NULL when the JSON value
 * is null or the key is absent / not a string), -1 on OOM.
 * Status codes keep "absent" (not an error) distinguishable from OOM. */
static int jget_str(const cJSON *obj, const char *key, char **out)
{
    *out = NULL;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return 0;
    return str_dup(item->valuestring, out);
}

/* Extract an integer field from a cJSON object.
 * Returns 0 with *out set when the value is an exact integer in
 * [INT_MIN, INT_MAX]; 1 if the key is absent or not a number
 * (*out = 0); -1 if the value is a number but not an exact integer
 * (e.g. 3.7) or outside int range.  No silent truncation
 * (3.7 → 3) or wraparound past INT_MAX. */
static int jget_int(const cJSON *obj, const char *key, int *out)
{
    *out = 0;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return 1;
    double d = item->valuedouble;
    if (d < (double)INT_MIN || d > (double)INT_MAX) return -1;
    if (d != (double)(int)d) return -1;
    *out = (int)d;
    return 0;
}

/* Extract a non-negative id field.  0 is a valid sentinel (root
 * folder / no parent / omitted).  Negative values are user errors and
 * are reported, not silently clamped to 0.  Status codes as in
 * jget_int; absent or wrong type → 0 with *out = 0. */
static int jget_id(const cJSON *obj, const char *key, int *out)
{
    int st = jget_int(obj, key, out);
    if (st < 0 || *out < 0) return -1;
    return 0;
}

/* Parse a JSON blob and require an object root.
 * Uses cJSON_ParseWithOpts with require_null_terminated so that
 * trailing garbage ("{} x") is rejected — a well-formed JSON
 * document is exactly one value.  Trailing whitespace (e.g. a file
 * ending in a newline) is still accepted.
 * On failure the cJSON error offset is VLOG'd (a bare -1 would be
 * opaque).  Caller frees *root on success. */
static int parse_root(const char *blob, cJSON **root)
{
    *root = cJSON_ParseWithOpts(blob, NULL, 1);
    if (!*root) {
        const char *err = cJSON_GetErrorPtr();
        VLOG(1, "JSON parse failed at offset %zu: %s",
             err ? strlen(err) : 0, err ? err : "(unknown)");
        return -1;
    }
    if (!cJSON_IsObject(*root)) {
        VLOG(1, "JSON root is not an object");
        cJSON_Delete(*root);
        return -1;
    }
    return 0;
}

/* ── generic walker (tables above are the spec) ────────────────────── */

/*
 * Zero the struct, walk the field table, populate from `blob`.
 * First failing field aborts; every string field copied so far is
 * freed so the struct is left fully zeroed (json.h contract: caller
 * frees nothing on -1).
 */
static int jwalk(const char *blob, void *dst, size_t size,
                  const jfield_t *fields, size_t n)
{
    if (!blob || !dst) return -1;

    char *base = (char *)dst;
    memset(dst, 0, size);

    cJSON *root;
    if (parse_root(blob, &root) != 0) return -1;

    int ok = 1;
    for (size_t i = 0; i < n && ok; i++) {
        if (fields[i].kind == JF_STR) {
            if (jget_str(root, fields[i].key,
                         (char **)(base + fields[i].off)) < 0)
                ok = 0;
        } else {
            if (jget_id(root, fields[i].key,
                        (int *)(base + fields[i].off)) != 0)
                ok = 0;
        }
        if (!ok)
            VLOG(1, "JSON field '%s' rejected", fields[i].key);
    }

    if (!ok) {
        for (size_t i = 0; i < n; i++)
            if (fields[i].kind == JF_STR)
                free(*(char **)(base + fields[i].off));
        memset(dst, 0, size);   /* json.h: left fully zeroed on -1 */
    }

    cJSON_Delete(root);
    return ok ? 0 : -1;
}

/* ── per-entity field tables (keys verbatim from cli_spec.md) ─────── */

static const jfield_t model_fields[] = {
    { "folder_id",        JF_ID,  offsetof(model_t, folder_id) },
    { "name",             JF_STR, offsetof(model_t, name) },
    { "description",      JF_STR, offsetof(model_t, description) },
    { "backend",          JF_STR, offsetof(model_t, backend) },
    { "base_url",         JF_STR, offsetof(model_t, base_url) },
    { "model_identifier", JF_STR, offsetof(model_t, model_identifier) },
    { "configuration",    JF_STR, offsetof(model_t, configuration) },
};

static const jfield_t context_fields[] = {
    { "id",         JF_ID,  offsetof(context_t, id) },
    { "type",       JF_STR, offsetof(context_t, type) },
    { "content",    JF_STR, offsetof(context_t, content) },
    { "hash",       JF_STR, offsetof(context_t, content_hash) },
    { "metadata",   JF_STR, offsetof(context_t, metadata) },
    { "created_at", JF_STR, offsetof(context_t, created_at) },
};

static const jfield_t execution_fields[] = {
    { "id",                  JF_ID,  offsetof(execution_t, id) },
    { "status",              JF_STR, offsetof(execution_t, status) },
    { "prompt",              JF_STR, offsetof(execution_t, prompt) },
    { "context_id",          JF_ID,  offsetof(execution_t, context_id) },
    { "skill_revision_id",   JF_ID,  offsetof(execution_t, skill_revision_id) },
    { "model_revision_id",   JF_ID,  offsetof(execution_t, model_revision_id) },
    { "parent_execution_id", JF_ID,  offsetof(execution_t, parent_execution_id) },
    { "created_at",          JF_STR, offsetof(execution_t, created_at) },
};

static const jfield_t execution_log_fields[] = {
    { "id",           JF_ID,  offsetof(execution_log_t, id) },
    { "execution_id", JF_ID,  offsetof(execution_log_t, execution_id) },
    { "level",        JF_STR, offsetof(execution_log_t, level) },
    { "event",        JF_STR, offsetof(execution_log_t, event) },
    { "message",      JF_STR, offsetof(execution_log_t, message) },
    { "metadata",     JF_STR, offsetof(execution_log_t, metadata) },
    { "created_at",   JF_STR, offsetof(execution_log_t, created_at) },
};

static const jfield_t model_folder_fields[] = {
    { "id",         JF_ID,  offsetof(model_folder_t, id) },
    { "name",       JF_STR, offsetof(model_folder_t, name) },
    { "parent_id",  JF_ID,  offsetof(model_folder_t, parent_id) },
    { "created_at", JF_STR, offsetof(model_folder_t, created_at) },
    { "updated_at", JF_STR, offsetof(model_folder_t, updated_at) },
    { "deleted_at", JF_STR, offsetof(model_folder_t, deleted_at) },
};

static const jfield_t skill_folder_fields[] = {
    { "id",         JF_ID,  offsetof(skill_folder_t, id) },
    { "name",       JF_STR, offsetof(skill_folder_t, name) },
    { "parent_id",  JF_ID,  offsetof(skill_folder_t, parent_id) },
    { "created_at", JF_STR, offsetof(skill_folder_t, created_at) },
    { "updated_at", JF_STR, offsetof(skill_folder_t, updated_at) },
    { "deleted_at", JF_STR, offsetof(skill_folder_t, deleted_at) },
};

static const jfield_t skill_fields[] = {
    { "id",             JF_ID,  offsetof(skill_t, id) },
    { "folder_id",      JF_ID,  offsetof(skill_t, folder_id) },
    { "name",           JF_STR, offsetof(skill_t, name) },
    { "description",    JF_STR, offsetof(skill_t, description) },
    { "prompt_template",JF_STR, offsetof(skill_t, prompt_template) },
    { "output_schema",  JF_STR, offsetof(skill_t, output_schema) },
    { "created_at",     JF_STR, offsetof(skill_t, created_at) },
    { "updated_at",     JF_STR, offsetof(skill_t, updated_at) },
    { "deleted_at",     JF_STR, offsetof(skill_t, deleted_at) },
};

/* ── public parsers ────────────────────────────────────────────────── */

int json_parse_model(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(model_t),
                  model_fields,
                  sizeof model_fields / sizeof model_fields[0]);
}

int json_parse_context(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(context_t),
                  context_fields,
                  sizeof context_fields / sizeof context_fields[0]);
}

int json_parse_execution(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(execution_t),
                  execution_fields,
                  sizeof execution_fields / sizeof execution_fields[0]);
}

int json_parse_execution_log(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(execution_log_t),
                  execution_log_fields,
                  sizeof execution_log_fields / sizeof execution_log_fields[0]);
}

int json_parse_model_folder(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(model_folder_t),
                  model_folder_fields,
                  sizeof model_folder_fields / sizeof model_folder_fields[0]);
}

int json_parse_skill_folder(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(skill_folder_t),
                  skill_folder_fields,
                  sizeof skill_folder_fields / sizeof skill_folder_fields[0]);
}

int json_parse_skill(const char *blob, void *out)
{
    return jwalk(blob, out, sizeof(skill_t),
                  skill_fields,
                  sizeof skill_fields / sizeof skill_fields[0]);
}

/* ── raw pass-through ─────────────────────────────────────────────── */

int json_validate(const char *blob)
{
    if (!blob) return -1;
    /* require_null_terminated: reject trailing garbage ("{} x");
     * well-formed JSON is exactly one value. */
    cJSON *root = cJSON_ParseWithOpts(blob, NULL, 1);
    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        VLOG(1, "JSON parse failed at offset %zu: %s",
             err ? strlen(err) : 0, err ? err : "(unknown)");
        return -1;
    }
    cJSON_Delete(root);
    return 0;
}
