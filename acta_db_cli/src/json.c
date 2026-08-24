#include "json.h"
#include <stdio.h>
#include <string.h>

/*
 * STUBS — real implementation will call into cJSON.
 * Signatures match json.h; bodies are no-ops / "not implemented".
 */

/* ---- serialize ---- */
char *json_serialize_model(const void *model, const char *fields,
                           int no_nulls, int pretty) {
    (void)model; (void)fields; (void)no_nulls; (void)pretty;
    fprintf(stderr, "[json] serialize_model: stub\n");
    return NULL;
}

char *json_serialize_skill(const void *skill, const char *fields,
                           int no_nulls, int pretty) {
    (void)skill; (void)fields; (void)no_nulls; (void)pretty;
    fprintf(stderr, "[json] serialize_skill: stub\n");
    return NULL;
}

char *json_serialize_context(const void *ctx, const char *fields,
                             int no_nulls, int pretty) {
    (void)ctx; (void)fields; (void)no_nulls; (void)pretty;
    fprintf(stderr, "[json] serialize_context: stub\n");
    return NULL;
}

char *json_serialize_execution(const void *exec, const char *fields,
                               int no_nulls, int pretty) {
    (void)exec; (void)fields; (void)no_nulls; (void)pretty;
    fprintf(stderr, "[json] serialize_execution: stub\n");
    return NULL;
}

char *json_serialize_model_array(const void **items, int n,
                                 const char *fields,
                                 int no_nulls, int pretty) {
    (void)items; (void)n; (void)fields; (void)no_nulls; (void)pretty;
    fprintf(stderr, "[json] serialize_model_array: stub\n");
    return NULL;
}

/* ---- parse ---- */
int json_parse_model(const char *blob, void *out) {
    (void)blob; (void)out;
    fprintf(stderr, "[json] parse_model: stub\n");
    return -1;
}

int json_parse_skill(const char *blob, void *out) {
    (void)blob; (void)out;
    fprintf(stderr, "[json] parse_skill: stub\n");
    return -1;
}

int json_parse_context(const char *blob, void *out) {
    (void)blob; (void)out;
    fprintf(stderr, "[json] parse_context: stub\n");
    return -1;
}

int json_parse_execution(const char *blob, void *out) {
    (void)blob; (void)out;
    fprintf(stderr, "[json] parse_execution: stub\n");
    return -1;
}

int json_validate(const char *blob) {
    (void)blob;
    fprintf(stderr, "[json] validate: stub\n");
    return 0;
}

/* ---- table ---- */
void json_print_table(FILE *out,
                      const char *const *headers, int ncols,
                      const char *const *row, int nrows) {
    (void)out; (void)headers; (void)ncols; (void)row; (void)nrows;
    fprintf(stderr, "[json] print_table: stub\n");
}
