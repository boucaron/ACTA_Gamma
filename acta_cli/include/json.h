#ifndef ACTA_JSON_H
#define ACTA_JSON_H

/*
 * JSON layer, implemented on top of vendored cJSON.
 * Parse (blob → entity struct) only; CLI output is hand-rolled in the
 * command handlers (no serialize API).
 * Entity structs come from the acta_db per-entity headers (<acta_db.h>);
 * the JSON layer never includes the dispatch header.
 */

/* ---- input: JSON blob → struct ---- */
/*
 * Returns 0 on success, -1 on failure:
 *   - malformed JSON (cJSON error offset is VLOG'd)
 *   - root is not an object
 *   - an id field is negative, a non-integer, or outside int range
 *   - OOM copying a string field
 * On -1 the pointed-to struct is left zeroed or fully freed (no partial
 * state); the caller does not need to free anything.
 * Populates the pointed-to struct (caller allocates).
 */
int json_parse_model(const char *blob, void *out);
int json_parse_skill(const char *blob, void *out);
int json_parse_context(const char *blob, void *out);
int json_parse_execution(const char *blob, void *out);
int json_parse_execution_log(const char *blob, void *out);
int json_parse_model_folder(const char *blob, void *out);
int json_parse_skill_folder(const char *blob, void *out);

/* ---- raw pass-through (for db exec results etc.) ---- */
/*
 * Validate that `blob` is well-formed JSON. Returns 0 or -1.
 */
int json_validate(const char *blob);

#endif /* ACTA_JSON_H */
