#ifndef ACTA_DB_MODEL_H
#define ACTA_DB_MODEL_H

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
model_folder_t *acta_db_model_folder_get(db_t *db, int id);
int          acta_db_model_folder_rename(db_t *db, int id, const char *new_name);
int          acta_db_model_folder_soft_delete(db_t *db, int id);
model_folder_t **acta_db_model_folder_list_children(db_t *db, int parent_id, int *out_count, int *err);
model_folder_t **acta_db_model_folder_list_all(db_t *db, int *out_count, int *err);
void          acta_db_model_folder_free(model_folder_t *f);
void          acta_db_model_folder_list_free(model_folder_t **items, int count);

int  acta_db_model_restore(db_t *db, int id);
int  acta_db_model_move_to_folder(db_t *db, int model_id, int folder_id);

/* ---------- model_t ---------- */

typedef struct {
    int     id;
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
} model_t;

int      acta_db_model_create(db_t *db, const model_t *m, int *out_id);
model_t *acta_db_model_get(db_t *db, int id);
model_t *acta_db_model_get_live(db_t *db, int id);   /* NULL if soft-deleted */
int      acta_db_model_update(db_t *db, const model_t *m);
int      acta_db_model_soft_delete(db_t *db, int id);
model_t **acta_db_model_list_in_folder(db_t *db, int folder_id, int *out_count, int *err);
model_t **acta_db_model_list_all(db_t *db, int *out_count, int *err);
void     acta_db_model_free(model_t *m);
void     acta_db_model_list_free(model_t **items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_MODEL_H */
