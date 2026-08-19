#ifndef ACTA_DB_EXECUTION_H
#define ACTA_DB_EXECUTION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int     id;
    int     context_id;
    int     skill_revision_id;
    int     model_revision_id;
    char   *prompt;
    char   *raw_response;
    char   *result;
    char   *status;         /* "pending" | "running" | "completed" | "failed" */
    char   *error;
    char   *created_at;
    char   *started_at;
    char   *completed_at;
    int     parent_execution_id;  /* 0 = root */
} execution_t;

int         acta_db_execution_create(db_t *db, const execution_t *e, int *out_id);
execution_t *acta_db_execution_get(db_t *db, int id);
int         acta_db_execution_start(db_t *db, int id);
int         acta_db_execution_complete(db_t *db, int id, const char *result);
int         acta_db_execution_fail(db_t *db, int id, const char *error);
int         acta_db_execution_set_raw_response(db_t *db, int id, const char *raw);
execution_t *acta_db_execution_list_by_status(db_t *db, const char *status, int *out_count);
execution_t *acta_db_execution_list_children(db_t *db, int parent_id, int *out_count);
void        acta_db_execution_free(execution_t *e);
void        acta_db_execution_list_free(execution_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_EXECUTION_H */
