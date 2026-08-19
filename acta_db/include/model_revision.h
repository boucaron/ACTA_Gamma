#ifndef ACTA_DB_MODEL_REVISION_H
#define ACTA_DB_MODEL_REVISION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int     id;
    int     model_id;
    int     revision;
    int     folder_id;      /* 0 = root */
    char   *name;
    char   *description;
    char   *backend;
    char   *base_url;
    char   *model_identifier;
    char   *configuration;
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;     /* NULL if live */
} model_revision_t;

model_revision_t *acta_db_model_revision_get(db_t *db, int id);
model_revision_t *acta_db_model_revision_get_by_model_and_rev(db_t *db, int model_id, int revision);
model_revision_t *acta_db_model_revision_list_by_model(db_t *db, int model_id, int *out_count);
void              acta_db_model_revision_free(model_revision_t *r);
void              acta_db_model_revision_list_free(model_revision_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_REVISION_H */
