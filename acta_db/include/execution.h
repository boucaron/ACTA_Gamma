#ifndef ACTA_DB_EXECUTION_H
#define ACTA_DB_EXECUTION_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACTA_EXEC_STATUS_PENDING   "pending"
#define ACTA_EXEC_STATUS_RUNNING   "running"
#define ACTA_EXEC_STATUS_COMPLETED "completed"
#define ACTA_EXEC_STATUS_FAILED    "failed"

typedef struct {
    int     id;
    int     context_id;
    int     skill_revision_id;
    int     model_revision_id;
    char   *prompt;
    char   *raw_response;
    char   *result;
    char   *status;
    char   *error;
    char   *created_at;
    char   *started_at;
    char   *completed_at;
    int     parent_execution_id;
} execution_t;

/* --- action / mutation functions (return int status directly) --- */
int  acta_db_execution_create(db_t *db, const execution_t *e, int *out_id);
int  acta_db_execution_start(db_t *db, int id);
int  acta_db_execution_complete(db_t *db, int id, const char *result);
int  acta_db_execution_fail(db_t *db, int id, const char *error);
int  acta_db_execution_set_raw_response(db_t *db, int id, const char *raw);

/* --- getters / listers (standardised err pattern) --- */
execution_t  *acta_db_execution_get(db_t *db, int id, int *err);
execution_t **acta_db_execution_list_by_status(db_t *db, const char *status, int *err);
execution_t **acta_db_execution_list_children(db_t *db, int parent_id, int *err);
execution_t **acta_db_execution_list_by_context(db_t *db, int context_id, int *err);

/* --- free --- */
void acta_db_execution_free(execution_t *e);
void acta_db_execution_list_free(execution_t **items);

#ifdef __cplusplus
}
#endif

#endif
