#include "internal.h"
#include "execution.h"

#include <stdio.h>

static execution_t *row_to_execution(sqlite3_stmt *stmt) {
    execution_t *e = calloc(1, sizeof(execution_t));
    if (!e) return NULL;
    e->id                  = db_col_int(stmt, 0);
    e->context_id          = db_col_int(stmt, 1);
    e->skill_revision_id   = db_col_int(stmt, 2);
    e->model_revision_id   = db_col_int(stmt, 3);
    e->prompt              = db_col_text(stmt, 4);
    e->raw_response        = db_col_text(stmt, 5);
    e->result              = db_col_text(stmt, 6);
    e->status              = db_col_text(stmt, 7);
    e->error               = db_col_text(stmt, 8);
    e->created_at          = db_col_text(stmt, 9);
    e->started_at          = db_col_text(stmt, 10);
    e->completed_at        = db_col_text(stmt, 11);
    e->parent_execution_id = db_col_int_or_zero(stmt, 12);
    return e;
}

#define EXEC_SELECT "SELECT id, context_id, skill_revision_id, model_revision_id, prompt, raw_response, result, status, error, created_at, started_at, completed_at, parent_execution_id FROM executions"

int acta_db_execution_create(db_t *db, const execution_t *e, int *out_id) {
    if (!db || !e || !e->status || !out_id) return -1;
    const char *sql =
        "INSERT INTO executions (context_id, skill_revision_id, model_revision_id, prompt, status, parent_execution_id) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, e->context_id);
    sqlite3_bind_int(stmt, 2, e->skill_revision_id);
    sqlite3_bind_int(stmt, 3, e->model_revision_id);
    if (e->prompt) sqlite3_bind_text(stmt, 4, e->prompt, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 4);
    sqlite3_bind_text(stmt, 5, e->status, -1, SQLITE_TRANSIENT);
    if (e->parent_execution_id == 0) sqlite3_bind_null(stmt, 6);
    else sqlite3_bind_int(stmt, 6, e->parent_execution_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    *out_id = (int)sqlite3_last_insert_rowid(db->handle);
    return 0;
}

execution_t *acta_db_execution_get(db_t *db, int id) {
    if (!db) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", EXEC_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, id);
    execution_t *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_execution(stmt);
    sqlite3_finalize(stmt);
    return result;
}

int acta_db_execution_start(db_t *db, int id) {
    if (!db) return -1;
    const char *sql = "UPDATE executions SET status = 'running', started_at = datetime('now') WHERE id = ? AND status = 'pending';";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? 0 : -1;
}


int acta_db_execution_complete(db_t *db, int id, const char *result) {
    if (!db) return -1;
    const char *sql = "UPDATE executions SET status = 'completed', result = ?, completed_at = datetime('now') WHERE id = ? AND status = 'running';";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    if (result) sqlite3_bind_text(stmt, 1, result, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? 0 : -1;
}

int acta_db_execution_fail(db_t *db, int id, const char *error) {
    if (!db) return -1;
    const char *sql = "UPDATE executions SET status = 'failed', error = ?, completed_at = datetime('now') WHERE id = ? AND status = 'running';";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    if (error) sqlite3_bind_text(stmt, 1, error, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? 0 : -1;
}


int acta_db_execution_set_raw_response(db_t *db, int id, const char *raw) {
    if (!db) return -1;
    const char *sql = "UPDATE executions SET raw_response = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    if (raw) sqlite3_bind_text(stmt, 1, raw, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    int changes = sqlite3_changes(db->handle);
    sqlite3_finalize(stmt);
    return changes > 0 ? 0 : -1;
}


execution_t *acta_db_execution_list_by_status(db_t *db, const char *status, int *out_count) {
    if (!db || !status || !out_count) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE status = ? ORDER BY created_at DESC;", EXEC_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);

    int count = 0;
    execution_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        execution_t *item = row_to_execution(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        execution_t *tmp = realloc(items, sizeof(execution_t) * (count + 1));
        if (!tmp) { acta_db_execution_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

execution_t *acta_db_execution_list_children(db_t *db, int parent_id, int *out_count) {
    if (!db || !out_count) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "%s WHERE parent_execution_id = ? ORDER BY created_at;", EXEC_SELECT);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, parent_id);

    int count = 0;
    execution_t *items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        execution_t *item = row_to_execution(stmt);
        if (!item) { sqlite3_finalize(stmt); return NULL; }
        execution_t *tmp = realloc(items, sizeof(execution_t) * (count + 1));
        if (!tmp) { acta_db_execution_free(item); sqlite3_finalize(stmt); return NULL; }
        items = tmp;
        items[count++] = *item;
        free(item);
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return items;
}

void acta_db_execution_free(execution_t *e) {
    if (!e) return;
    free(e->prompt); free(e->raw_response); free(e->result);
    free(e->status); free(e->error); free(e->created_at);
    free(e->started_at); free(e->completed_at); free(e);
}

void acta_db_execution_list_free(execution_t *items, int count) {
    if (!items) return;
    for (int i = 0; i < count; i++) {
        free(items[i].prompt);
        free(items[i].raw_response);
        free(items[i].result);
        free(items[i].status);
        free(items[i].error);
        free(items[i].created_at);
        free(items[i].started_at);
        free(items[i].completed_at);
    }
    free(items);
}

