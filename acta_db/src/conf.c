/*
 * conf.c — per-machine config file parser.
 *
 * Source of truth: docs/plans/acta-config-file.md (work item 1).
 *
 * Parsed with the same fail-closed style as the model `configuration` blob
 * check in acta_runner/src/run.c: not-an-object, unknown key, wrong type, or
 * malformed JSON -> hard error.  One shared helper, read by all three
 * binaries (the GUI uses an equivalent Qt/QJsonDocument reader).
 *
 * The parser is deliberately logging-agnostic: it reports a one-line
 * diagnostic via the `err_msg` out-parameter (a malloc'd string the caller
 * frees) instead of VLOG'ing, so acta_cli and acta_runner can surface it in
 * their own error channels.
 */

#include "conf.h"

#include <cjson/cJSON.h>

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The exact wire keys of the config file. Any other top-level key is a
 * contract violation (mirror of the known[] check in run.c). */
static const char *known_keys[] = { "api_key", "db", "max_chars", "timeout" };
#define NKEYS (sizeof known_keys / sizeof known_keys[0])

static int key_known(const char *k)
{
    for (size_t i = 0; i < NKEYS; i++)
        if (k && strcmp(known_keys[i], k) == 0)
            return 1;
    return 0;
}

/* Heap-copy a NUL-terminated string. Returns NULL on OOM or NULL input. */
static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

/* Store a one-line diagnostic in *err_msg (malloc'd; the caller frees it).
 * vsnprintf always NUL-terminates (truncating if the message is too long),
 * so buf is always a valid C string. */
static void set_err(char **err_msg, const char *fmt, ...)
{
    if (!err_msg) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    char *p = dup_str(buf);
    if (p) *err_msg = p;
}

/* Parse exactly one NUL-terminated JSON document and require an object
 * root.  require_null_terminated rejects trailing garbage ("{} x") — a
 * well-formed JSON document is exactly one value; trailing whitespace (e.g.
 * a file ending in a newline) is still accepted.  Caller frees *root on
 * success.  Returns 0 ok, -1 fail (with *err set). */
static int parse_root(const char *blob, cJSON **root, char **err)
{
    *root = NULL;
    if (!blob) {
        set_err(err, "config: empty input");
        return -1;
    }
    *root = cJSON_ParseWithOpts(blob, NULL, 1);
    if (!*root) {
        const char *e = cJSON_GetErrorPtr();
        set_err(err, "config is not valid JSON%s%s",
                e ? ": " : "", e ? e : "");
        return -1;
    }
    if (!cJSON_IsObject(*root)) {
        set_err(err, "config root is not an object");
        cJSON_Delete(*root);
        *root = NULL;
        return -1;
    }
    return 0;
}

/* Read an optional string field.
 *   absent              -> *out = NULL, 0
 *   present + string    -> *out = dup, 0  (-1 on OOM)
 *   present + wrong type-> -1, *err set
 */
static int get_str(const cJSON *obj, const char *key, char **out, char **err)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) { *out = NULL; return 0; }              /* absent */
    if (!cJSON_IsString(item)) {
        set_err(err, "config key '%s' must be a string", key);
        return -1;
    }
    *out = dup_str(item->valuestring);
    if (!*out) {
        set_err(err, "out of memory copying config key '%s'", key);
        return -1;
    }
    return 0;
}

/* Read an optional positive-integer field.
 *   absent                -> *out = 0, 0
 *   present + positive int-> *out = value, 0
 *   present + wrong type / fractional / <= 0 / out of range -> -1, *err
 */
static int get_posint(const cJSON *obj, const char *key, long *out, char **err)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) { *out = 0; return 0; }                 /* absent */
    if (!cJSON_IsNumber(item)) {
        set_err(err, "config key '%s' must be a number", key);
        return -1;
    }
    double d = item->valuedouble;
    if (d != (double)(long)d) {
        set_err(err, "config key '%s' must be an integer", key);
        return -1;
    }
    if (d <= 0.0 || d > (double)LONG_MAX) {
        set_err(err, "config key '%s' must be a positive integer", key);
        return -1;
    }
    *out = (long)d;
    return 0;
}

int acta_conf_parse(const char *blob, acta_conf_t *out, char **err_msg)
{
    if (err_msg) *err_msg = NULL;
    if (!out) return -1;
    out->api_key = NULL;
    out->db = NULL;
    out->max_chars = 0;
    out->timeout = 0;

    cJSON *root;
    if (parse_root(blob, &root, err_msg) != 0)
        return -1;

    /* Unknown top-level key -> contract violation (mirror of run.c). */
    for (const cJSON *item = root->child; item; item = item->next) {
        if (!key_known(item->string)) {
            set_err(err_msg, "unknown config key: '%s'",
                    item->string ? item->string : "(null)");
            cJSON_Delete(root);
            return -1;
        }
    }

    int ok = 1;
    if (get_str(root, "api_key", &out->api_key, err_msg) != 0)
        ok = 0;
    if (ok && get_str(root, "db", &out->db, err_msg) != 0)
        ok = 0;
    if (ok && get_posint(root, "max_chars", &out->max_chars, err_msg) != 0)
        ok = 0;
    if (ok && get_posint(root, "timeout", &out->timeout, err_msg) != 0)
        ok = 0;

    if (!ok) {
        /* Failure: free any strings copied so far and zero the struct
         * (conf.h contract: the caller frees nothing on -1). */
        free(out->api_key);
        free(out->db);
        out->api_key = NULL;
        out->db = NULL;
        out->max_chars = 0;
        out->timeout = 0;
    }
    cJSON_Delete(root);
    return ok ? 0 : -1;
}

void acta_conf_free(acta_conf_t *conf)
{
    if (!conf) return;
    free(conf->api_key);
    free(conf->db);
    conf->api_key = NULL;
    conf->db = NULL;
    conf->max_chars = 0;
    conf->timeout = 0;
}

#define ACTA_CONFPATH_MAX 4096

/* Platform app-data base directory (no trailing slash), or NULL when it
 * cannot be resolved.  Same logic as appdata_base() in acta_dbpath.c
 * (both copies); keep in lockstep. */
static const char *conf_appdata_base(void)
{
#ifdef _WIN32
    const char *p = getenv("APPDATA");
    return (p && p[0]) ? p : NULL;
#else
    const char *p = getenv("XDG_DATA_HOME");
    if (p && p[0])
        return p;
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return NULL;
    static char fallback[ACTA_CONFPATH_MAX];
    size_t n = strlen(home);
    if (n + 1 + sizeof("/.local/share") - 1 + 1 > sizeof fallback)
        return NULL;
    snprintf(fallback, sizeof fallback, "%s/.local/share", home);
    return fallback;
#endif
}

const char *acta_conf_default_path(void)
{
    static char path[ACTA_CONFPATH_MAX];
    const char *base = conf_appdata_base();
    if (!base)
        return "./ACTA Gamma.conf";

    size_t n = strlen(base);
    if (n + 1 + sizeof("/ACTA Gamma/ACTA Gamma.conf") > sizeof path)
        return "./ACTA Gamma.conf";

#ifdef _WIN32
    snprintf(path, sizeof path, "%s\\ACTA Gamma\\ACTA Gamma.conf", base);
#else
    snprintf(path, sizeof path, "%s/ACTA Gamma/ACTA Gamma.conf", base);
#endif
    return path;
}

/* Read a whole file into a malloc'd NUL-terminated buffer.  Returns 0 on
 * success (the caller frees *out); -1 when the file cannot be opened or
 * read, or on allocation failure (leaves *out NULL). */
static int slurp_file(const char *path, char **out)
{
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    char *buf = NULL;
    size_t cap = 0, len = 0;
    for (;;) {
        if (len + 1 > cap) {
            size_t ncap = cap ? cap * 2 : 4096;
            char *nb = realloc(buf, ncap);
            if (!nb) {
                free(buf);
                fclose(f);
                return -1;
            }
            buf = nb;
            cap = ncap;
        }
        size_t got = fread(buf + len, 1, cap - len - 1, f);
        len += got;
        if (got == 0)
            break;
    }
    fclose(f);
    buf[len] = '\0';
    *out = buf;
    return 0;
}

int acta_conf_read(const char *path, acta_conf_t *conf, int *missing,
                   char **err_msg)
{
    if (err_msg)
        *err_msg = NULL;
    if (missing)
        *missing = 0;
    if (!path) {
        set_err(err_msg, "config: NULL path");
        return -1;
    }

    char *blob = NULL;
    if (slurp_file(path, &blob) != 0) {
        /* Missing or unreadable file: not an error, the file is simply
         * unavailable as a fallback. */
        if (missing)
            *missing = 1;
        return 0;
    }
    int rc = acta_conf_parse(blob, conf, err_msg);
    free(blob);
    return rc;
}

int acta_conf_api_key_status(const char *env_key, const char *file_key,
                             const char **msg)
{
    /* Env wins when set -- even when set to the empty string; the file
     * is a fallback, not a second channel. */
    const char *key = (env_key != NULL) ? env_key : file_key;

    if (key == NULL) {
        if (msg)
            *msg = "OPENAI_API_KEY is not set and the config file "
                   "provides no \"api_key\"; set the environment variable "
                   "or add \"api_key\" to the config file";
        return ACTA_KEY_UNSET_ERR;
    }
    if (key[0] == '\0') {
        if (msg)
            *msg = (env_key != NULL)
                ? "warning: OPENAI_API_KEY is empty - no Authorization "
                  "header will be sent (acceptable only for a keyless "
                  "localhost server; a bad idea in general)"
                : "warning: the config file \"api_key\" is empty - no "
                  "Authorization header will be sent (acceptable only "
                  "for a keyless localhost server; a bad idea in general)";
        return ACTA_KEY_EMPTY_WARN;
    }
    if (msg)
        *msg = NULL;
    return ACTA_KEY_OK;
}
