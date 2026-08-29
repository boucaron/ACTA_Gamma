#ifndef ACTA_JSON_H
#define ACTA_JSON_H

#include <stdio.h>

/*
 * JSON serialize / parse helpers (cJSON-backed).
 * See json.c for the implementation. The serialize_* functions are
 * declared per entity; not all are implemented yet (see json.c).

/* ---- output: struct → compact/pretty one-line JSON ---- */
/*
 * Returns malloc'd string; caller frees.
 * NULL on error.
 *
 * `fields`  — comma-separated whitelist, or NULL for all.
 * `no_nulls`— omit null-valued fields.
 * `pretty`  — 2-space indent.
 */
char *json_serialize_model(const void *model,
                           const char *fields,
                           int no_nulls,
                           int pretty);
/* ... one per entity ... */
char *json_serialize_skill(const void *skill,
                           const char *fields, int no_nulls, int pretty);
char *json_serialize_context(const void *ctx,
                             const char *fields, int no_nulls, int pretty);
char *json_serialize_execution(const void *exec,
                               const char *fields, int no_nulls, int pretty);

/* ---- array (list) ---- */
char *json_serialize_model_array(const void **items, int n,
                                 const char *fields,
                                 int no_nulls, int pretty);

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

/* ---- table output (human-readable columns) ---- */
/*
 * Print rows as aligned columns to `out`.
 * Truncates strings > 40 chars. NULL → "-".
 */
void json_print_table(FILE *out,
                      const char *const *headers, int ncols,
                      const char *const *row, int nrows);

#endif /* ACTA_JSON_H */
