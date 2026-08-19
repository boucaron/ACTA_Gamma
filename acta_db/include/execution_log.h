#ifndef ACTA_DB_EXECUTION_LOG_H
#define ACTA_DB_EXECUTION_LOG_H

#include "db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int     id;
    int     execution_id;
    char   *level;         /* "debug" | "info" | "warn" | "error" */
    char   *event;
    char   *message;
    char   *metadata;
    char   *created_at;
} execution_log_t;

int             acta_db_execution_log_create(db_t *db, const execution_log_t *log, int *out_id);
execution_log_t *acta_db_execution_log_list_by_execution(db_t *db, int execution_id, int *out_count);
void            acta_db_execution_log_free(execution_log_t *log);
void            acta_db_execution_log_list_free(execution_log_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACTA_DB_EXECUTION_LOG_H */
