#ifndef ACTA_JSON_H
#define ACTA_JSON_H

/*
 * JSON layer, implemented on top of vendored cJSON.
 * Parse (blob → entity struct) only; CLI output is hand-rolled in the
 * command handlers (no serialize API).
 * Entity structs are declared in commands.h.
 */

/* ---- input: JSON blob → struct ---- */
/*
 * Returns 0 on success, -1 on parse error.
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
