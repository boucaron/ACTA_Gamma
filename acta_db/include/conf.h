#ifndef ACTA_CONF_H
#define ACTA_CONF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * acta_conf_t — per-machine settings parsed from the config file.
 *
 * Source of truth: docs/plans/acta-config-file.md (work item 1).  This is
 * the "shared helper" read by all three binaries; acta_cli and acta_runner
 * call this C parser directly, and the GUI uses an equivalent Qt
 * (QJsonDocument) reader (work item 6) rather than this code.
 *
 * The file holds AT MOST these four operator settings.  Backend URL, model
 * id, and configuration stay in the DB model record — this struct holds
 * per-machine operator settings, not entity data.
 *
 * Ownership:
 *   success (return 0)  -> caller owns every non-NULL string field; call
 *                          acta_conf_free() when done.
 *   failure (return -1) -> the struct is left fully zeroed; free nothing.
 *
 * "0" in max_chars / timeout means "absent from the file"; the caller
 * applies the built-in default (see the plan) in that case.
 */
typedef struct {
    char *api_key;   /* heap copy of "api_key", or NULL when absent */
    char *db;        /* heap copy of "db", or NULL when absent */
    long  max_chars; /* "max_chars" positive integer, or 0 when absent */
    long  timeout;   /* "timeout" positive integer (seconds), or 0 when absent */
} acta_conf_t;

/*
 * Parse a config-file JSON blob into `out`.
 *
 * `blob` must be exactly one well-formed, NUL-terminated JSON document whose
 * root is an object containing AT MOST these keys:
 *     "api_key"   string
 *     "db"        string
 *     "max_chars" positive integer
 *     "timeout"   positive integer
 *
 * Fail-closed, mirroring the model `configuration` blob check in
 * acta_runner/src/run.c: any of the following returns -1 and leaves `out`
 * fully zeroed (the caller frees nothing):
 *     - `blob` is NULL or not well-formed JSON (trailing garbage rejected);
 *     - the root is not an object;
 *     - any top-level key is not one of the four known keys (unknown/typo'd);
 *     - "api_key" / "db" present but not a string;
 *     - "max_chars" / "timeout" present but not a positive integer
 *       (wrong type, fractional, zero, negative, or out of range).
 *
 * Returns 0 on success, -1 on failure.  `err_msg`, if non-NULL, receives a
 * malloc'd one-line diagnostic the caller must free (NULL on success).
 */
int acta_conf_parse(const char *blob, acta_conf_t *out, char **err_msg);

/*
 * Free the string fields owned by a parsed acta_conf_t and zero the numeric
 * fields.  NULL-safe.  The struct itself is a plain POD embedded in the
 * caller; this frees only its heap string fields.
 */
void acta_conf_free(acta_conf_t *conf);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_CONF_H */
