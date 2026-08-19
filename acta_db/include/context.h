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

int       acta_db_context_create(db_t *db, const context_t *c, int *out_id);
context_t *acta_db_context_get(db_t *db, int id);
context_t *acta_db_context_list_by_hash(db_t *db, const char *hash, int *out_count);
void      acta_db_context_free(context_t *c);
void      acta_db_context_list_free(context_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_CONTEXT_H */
