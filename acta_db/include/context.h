#ifndef ACTA_DB_CONTEXT_H
#define ACTA_DB_CONTEXT_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int     id;
    char   *type;
    char   *content;
    char   *content_hash;
    char   *metadata;
    char   *created_at;
} context_t;

/* Insert a context row.
 * Returns ACTA_DB_OK on success; ACTA_DB_ERR_INVALID if required fields
 * are missing; ACTA_DB_ERR_SQL if the statement fails.
 * On success *out_id receives the new row's id. */
int       acta_db_context_create(db_t *db, const context_t *c, int *out_id);

/* Fetch a single context by id.
 * Returns a heap-allocated context_t (caller must free with
 * acta_db_context_free) or NULL if not found or on error. */
context_t *acta_db_context_get(db_t *db, int id);

/* Return all contexts ordered by id.
 * Returns a heap-allocated array of context_t (free with
 * acta_db_context_list_free) or NULL on error.
 * *out_count receives the number of items. */
context_t *acta_db_context_list_all(db_t *db, int *out_count);

/* Return all contexts matching the given type, ordered by id.
 * Returns a heap-allocated array of context_t or NULL on error /
 * if type is NULL. *out_count receives the number of items. */
context_t *acta_db_context_list_by_type(db_t *db, const char *type, int *out_count);

/* Return all contexts matching the given content hash, ordered by id.
 * Returns a heap-allocated array of context_t or NULL on error /
 * if hash is NULL. *out_count receives the number of items. */
context_t *acta_db_context_list_by_hash(db_t *db, const char *hash, int *out_count);

/* Free a single context_t (and its string fields). */
void      acta_db_context_free(context_t *c);

/* Free an array of contexts returned by a list_* function. */
void      acta_db_context_list_free(context_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_CONTEXT_H */
