#ifndef ACTA_DB_MODEL_FOLDER_H
#define ACTA_DB_MODEL_FOLDER_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- model_folder_t ---------- */

typedef struct {
    int     id;
    char   *name;
    int     parent_id;      /* 0 = root */
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;     /* NULL if live */
} model_folder_t;

int          acta_db_model_folder_create(db_t *db, const char *name, int parent_id, int *out_id);
model_folder_t *acta_db_model_folder_get(db_t *db, int id, int *err);
int          acta_db_model_folder_rename(db_t *db, int id, const char *new_name);
int          acta_db_model_folder_soft_delete(db_t *db, int id);
model_folder_t **acta_db_model_folder_list_children(db_t *db,
                                                    int parent_id,
                                                    int offset, int limit,
                                                    int *out_count, int *err);
model_folder_t **acta_db_model_folder_list_all(db_t *db,
                                               int offset, int limit,
                                               int *out_count, int *err);
void          acta_db_model_folder_free(model_folder_t *f);
void          acta_db_model_folder_list_free(model_folder_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_FOLDER_H */
