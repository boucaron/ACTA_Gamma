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
 *
 * Returns a heap-allocated context_t (free with acta_db_context_free),
 * or NULL when the row is not found.
 *
 * If err is non-NULL it is set to:
 *   ACTA_DB_OK         – row found, or not found (both are "success")
 *   ACTA_DB_ERR_SQL    – real database failure
 *   ACTA_DB_ERR_ALLOC  – memory allocation failure
 *   ACTA_DB_ERR_INVALID– db or id is invalid
 *
 * The err parameter may be NULL (caller ignores the code). */
context_t *acta_db_context_get(db_t *db, int id, int *err);

/* Return all contexts ordered by id.
 *
 * Returns a heap-allocated array of context_t pointers (free with
 * acta_db_context_list_free), or NULL on real failure.
 *
 * If out_count is non-NULL it receives the number of items.
 * If err is non-NULL it is set to ACTA_DB_OK on success (including
 * an empty result set) or a negative ACTA_DB_ERR_* code on failure.
 * Either out_count or err (or both) may be NULL. */
context_t **acta_db_context_list_all(db_t *db, int *out_count, int *err);

/* Return all contexts matching the given type, ordered by id.
 *
 * If type is NULL the function fails with ACTA_DB_ERR_INVALID.
 * Same return / err / out_count contract as acta_db_context_list_all. */
context_t **acta_db_context_list_by_type(db_t *db,
                                          const char *type,
                                          int *out_count,
                                          int *err);

/* Return all contexts matching the given content hash, ordered by id.
 *
 * If hash is NULL the function fails with ACTA_DB_ERR_INVALID.
 * Same return / err / out_count contract as acta_db_context_list_all. */
context_t **acta_db_context_list_by_hash(db_t *db,
                                          const char *hash,
                                          int *out_count,
                                          int *err);

/* Free a single context_t (and its string fields). Safe with NULL. */
void      acta_db_context_free(context_t *c);

/* Free an array of context_t pointers returned by a list_* function.
 * Frees each element and the array itself. Safe with NULL. */
void      acta_db_context_list_free(context_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_CONTEXT_H */
