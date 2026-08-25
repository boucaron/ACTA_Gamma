#include "json.h"
#include "commands.h"   /* for model_t, skill_t, etc. */

#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── internal helpers ─────────────────────────────────────────────── */

/* Duplicate a C string.  NULL → NULL. */
static char *dup_or_null(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Extract an optional string field from a cJSON object.
 * Returns malloc'd string, or NULL if the key is absent / JSON null. */
static char *jget_str(const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return NULL;
    return dup_or_null(item->valuestring);
}

/* Extract an optional int field.
 * Returns `def` if the key is absent or not a number. */
static int jget_int(const cJSON *obj, const char *key, int def)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return (int)item->valuedouble;
}

/* ── json_parse_model ─────────────────────────────────────────────── */

int json_parse_model(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    model_t *m = (model_t *)out;
    memset(m, 0, sizeof(*m));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int fid = jget_int(root, "folder_id", 0);
    m->folder_id        = (fid > 0) ? fid : 0;

    m->name             = jget_str(root, "name");
    m->description      = jget_str(root, "description");
    m->backend          = jget_str(root, "backend");
    m->base_url         = jget_str(root, "base_url");
    m->model_identifier = jget_str(root, "model_identifier");
    m->configuration    = jget_str(root, "configuration");

    cJSON_Delete(root);
    return 0;
}


/* ── json_validate ────────────────────────────────────────────────── */

int json_validate(const char *blob)
{
    if (!blob) return -1;
    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;
    cJSON_Delete(root);
    return 0;
}

/* ── remaining stubs (to be filled in per entity) ─────────────────── */

char *json_serialize_model(const void *model,
                           const char *fields, int no_nulls, int pretty)
{
    (void)model; (void)fields; (void)no_nulls; (void)pretty;
    return NULL;  /* TODO */
}

char *json_serialize_skill(const void *skill,
                           const char *fields, int no_nulls, int pretty)
{
    (void)skill; (void)fields; (void)no_nulls; (void)pretty;
    return NULL;
}

char *json_serialize_context(const void *ctx,
                             const char *fields, int no_nulls, int pretty)
{
    (void)ctx; (void)fields; (void)no_nulls; (void)pretty;
    return NULL;
}

char *json_serialize_execution(const void *exec,
                               const char *fields, int no_nulls, int pretty)
{
    (void)exec; (void)fields; (void)no_nulls; (void)pretty;
    return NULL;
}

char *json_serialize_model_array(const void **items, int n,
                                 const char *fields,
                                 int no_nulls, int pretty)
{
    (void)items; (void)n; (void)fields; (void)no_nulls; (void)pretty;
    return NULL;
}

/* ── json_parse_context ───────────────────────────────────────────── */

int json_parse_context(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    context_t *c = (context_t *)out;
    memset(c, 0, sizeof(*c));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int id = jget_int(root, "id", 0);
    c->id           = (id > 0) ? id : 0;

    c->type         = jget_str(root, "type");
    c->content      = jget_str(root, "content");
    c->content_hash = jget_str(root, "content_hash");
    c->metadata     = jget_str(root, "metadata");
    c->created_at   = jget_str(root, "created_at");

    cJSON_Delete(root);
    return 0;
}


/* ── json_parse_execution ────────────────────────────────────────── */

int json_parse_execution(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    execution_t *e = (execution_t *)out;
    memset(e, 0, sizeof(*e));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int id = jget_int(root, "id", 0);
    e->id = (id > 0) ? id : 0;

    e->status              = jget_str(root, "status");
    e->prompt              = jget_str(root, "prompt");

    int ctx   = jget_int(root, "context_id", 0);
    e->context_id          = (ctx > 0) ? ctx : 0;

    int sr    = jget_int(root, "skill_revision_id", 0);
    e->skill_revision_id   = (sr > 0) ? sr : 0;

    int mr    = jget_int(root, "model_revision_id", 0);
    e->model_revision_id   = (mr > 0) ? mr : 0;

    int pid   = jget_int(root, "parent_execution_id", 0);
    e->parent_execution_id = (pid > 0) ? pid : 0;   /* 0 = no parent */

    e->created_at          = jget_str(root, "created_at");

    cJSON_Delete(root);
    return 0;
}

/* ── json_parse_execution_log ─────────────────────────────────────── */

int json_parse_execution_log(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    execution_log_t *log = (execution_log_t *)out;
    memset(log, 0, sizeof(*log));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int id = jget_int(root, "id", 0);
    log->id            = (id > 0) ? id : 0;

    int eid = jget_int(root, "execution_id", 0);
    log->execution_id  = (eid > 0) ? eid : 0;

    log->level         = jget_str(root, "level");
    log->event         = jget_str(root, "event");
    log->message       = jget_str(root, "message");
    log->metadata      = jget_str(root, "metadata");
    log->created_at    = jget_str(root, "created_at");

    cJSON_Delete(root);
    return 0;
}

/* ── json_parse_model_folder ──────────────────────────────────────── */

int json_parse_model_folder(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    model_folder_t *f = (model_folder_t *)out;
    memset(f, 0, sizeof(*f));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int id = jget_int(root, "id", 0);
    f->id            = (id > 0) ? id : 0;

    f->name          = jget_str(root, "name");

    int pid = jget_int(root, "parent_id", 0);
    f->parent_id     = (pid > 0) ? pid : 0;   /* 0 = root */

    f->created_at    = jget_str(root, "created_at");
    f->updated_at    = jget_str(root, "updated_at");
    f->deleted_at    = jget_str(root, "deleted_at"); /* NULL if live */

    cJSON_Delete(root);
    return 0;
}


/* ── json_parse_skill_folder ──────────────────────────────────────── */

int json_parse_skill_folder(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    skill_folder_t *f = (skill_folder_t *)out;
    memset(f, 0, sizeof(*f));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int id = jget_int(root, "id", 0);
    f->id            = (id > 0) ? id : 0;

    f->name          = jget_str(root, "name");

    int pid = jget_int(root, "parent_id", 0);
    f->parent_id     = (pid > 0) ? pid : 0;   /* 0 = root */

    f->created_at    = jget_str(root, "created_at");
    f->updated_at    = jget_str(root, "updated_at");
    f->deleted_at    = jget_str(root, "deleted_at"); /* NULL if live */

    cJSON_Delete(root);
    return 0;
}



/* ── json_parse_skill ─────────────────────────────────────────────── */

int json_parse_skill(const char *blob, void *out)
{
    if (!blob || !out) return -1;

    skill_t *s = (skill_t *)out;
    memset(s, 0, sizeof(*s));

    cJSON *root = cJSON_Parse(blob);
    if (!root) return -1;

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int id = jget_int(root, "id", 0);
    s->id            = (id > 0) ? id : 0;

    int fid = jget_int(root, "folder_id", 0);
    s->folder_id     = (fid > 0) ? fid : 0;   /* 0 = root */

    s->name               = jget_str(root, "name");
    s->description        = jget_str(root, "description");
    s->prompt_template    = jget_str(root, "prompt_template");
    s->output_schema      = jget_str(root, "output_schema");

    s->created_at         = jget_str(root, "created_at");
    s->updated_at         = jget_str(root, "updated_at");
    s->deleted_at         = jget_str(root, "deleted_at"); /* NULL if live */

    cJSON_Delete(root);
    return 0;
}


void json_print_table(FILE *out,
                      const char *const *headers, int ncols,
                      const char *const *row, int nrows)
{
    (void)out; (void)headers; (void)ncols; (void)row; (void)nrows;
    /* TODO */
}
