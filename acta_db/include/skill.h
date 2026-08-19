#ifndef ACTA_DB_SKILL_H
#define ACTA_DB_SKILL_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- skill_folder_t ---------- */

typedef struct {
    int     id;
    char   *name;
    int     parent_id;      /* 0 = root */
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;
} skill_folder_t;

int           acta_db_skill_folder_create(db_t *db, const char *name, int parent_id, int *out_id);
skill_folder_t *acta_db_skill_folder_get(db_t *db, int id);
int           acta_db_skill_folder_rename(db_t *db, int id, const char *new_name);
int           acta_db_skill_folder_soft_delete(db_t *db, int id);
skill_folder_t *acta_db_skill_folder_list_children(db_t *db, int parent_id, int *out_count);
skill_folder_t *acta_db_skill_folder_list_all(db_t *db, int *out_count);
void          acta_db_skill_folder_free(skill_folder_t *f);
void          acta_db_skill_folder_list_free(skill_folder_t *items, int count);

/* ---------- skill_t ---------- */

typedef struct {
    int     id;
    int     folder_id;      /* 0 = root */
    char   *name;
    char   *description;
    char   *prompt_template;
    char   *output_schema;    
    char   *created_at;
    char   *updated_at;
    char   *deleted_at;
} skill_t;

int      acta_db_skill_create(db_t *db, const skill_t *s, int *out_id);
skill_t *acta_db_skill_get(db_t *db, int id);
skill_t *acta_db_skill_get_live(db_t *db, int id);
int      acta_db_skill_update(db_t *db, const skill_t *s);
int      acta_db_skill_soft_delete(db_t *db, int id);
skill_t *acta_db_skill_list_in_folder(db_t *db, int folder_id, int *out_count);
skill_t *acta_db_skill_list_all(db_t *db, int *out_count);
void     acta_db_skill_free(skill_t *s);
void     acta_db_skill_list_free(skill_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_SKILL_H */
