#ifndef ACTA_DB_CONTEXT_H
#define ACTA_DB_CONTEXT_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Row ──────────────────────────────────────────────────────────── */

typedef struct {
    int     id;
    char   *type;
    char   *content;
    char   *content_hash;
    char   *metadata;
    char   *created_at;
} context_t;

/* ── Query / filter ───────────────────────────────────────────────── */

/* All fields are optional (NULL = "match all" for that dimension).
 *
 *   type  – exact match on the type column.
 *   hash  – exact match on the content_hash column.
 *
 * At least one should typically be set to avoid a full table scan,
 * but the API does not enforce it. */
typedef struct {
    const char *type;   /* NULL = any type */
    const char *hash;   /* NULL = any hash */
} context_query_t;

/* ── Single-row operations ────────────────────────────────────────── */

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
 *   ACTA_DB_OK          – row found, or not found (both are "success")
 *   ACTA_DB_ERR_SQL    – real database failure
 *   ACTA_DB_ERR_ALLOC  – memory allocation failure
 *   ACTA_DB_ERR_INVALID – db or id is invalid
 *
 * The err parameter may be NULL (caller ignores the code). */
context_t *acta_db_context_get(db_t *db, int id, int *err);

/* ── Query (paginated) ────────────────────────────────────────────── */

/* Return a page of contexts matching `q`, ordered by id ASC.
 *
 * q      – filter criteria; NULL treats both fields as "match all".
 * offset – skip this many rows before starting (>= 0).
 * limit  – maximum number of rows to return (<= 0 = no cap).
 *
 * Returns a heap-allocated array of context_t pointers (free with
 * acta_db_context_list_free), or NULL on real failure.
 *
 * If out_count is non-NULL it receives the number of items actually
 * returned (may be less than limit if the result set is exhausted).
 * If err is non-NULL it is set to ACTA_DB_OK on success (including an
 * empty result set) or a negative ACTA_DB_ERR_* code on failure.
 * Either out_count or err (or both) may be NULL. */
context_t **acta_db_context_query(db_t *db,
                                  const context_query_t *q,
                                  int offset,
                                  int limit,
                                  int *out_count,
                                  int *err);

/* Return the total number of context rows matching `q`.
 *
 * q – filter criteria; NULL treats both fields as "match all".
 *
 * Returns the row count (>= 0) on success, or -1 on error.
 * If err is non-NULL it is set to ACTA_DB_OK or a negative
 * ACTA_DB_ERR_* code. The err parameter may be NULL. */
int acta_db_context_count(db_t *db,
                          const context_query_t *q,
                          int *err);

/* ── Ownership / cleanup ──────────────────────────────────────────── */

/* Free a single context_t (and its string fields). Safe with NULL. */
void      acta_db_context_free(context_t *c);

/* Free an array of context_t pointers returned by a query/list function.
 * Frees each element and the array itself. Safe with NULL. */
void      acta_db_context_list_free(context_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_CONTEXT_H */
