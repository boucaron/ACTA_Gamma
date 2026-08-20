#ifndef ACTA_DB_INTERNAL_H
#define ACTA_DB_INTERNAL_H

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

/* Unpack the opaque db_t */
struct db_t {
    sqlite3 *handle;
    char    *last_error;   /* NULL when no error */
    int       in_transaction;  /* 0 = idle, 1 = BEGIN issued, not yet closed */
};

/* strdup that returns NULL on allocation failure (we prefer NULL over crashing) */
static inline char *db_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (!copy) return NULL;
    memcpy(copy, s, len);
    return copy;
}

/* Read a text column. Returns NULL if the column is SQL NULL. */
static inline char *db_col_text(sqlite3_stmt *stmt, int col) {
    const unsigned char *val = sqlite3_column_text(stmt, col);
    return val ? db_strdup((const char *)val) : NULL;
}

/* Read an integer column. */
static inline int db_col_int(sqlite3_stmt *stmt, int col) {
    return (int)sqlite3_column_int64(stmt, col);
}

/* Read an integer column, mapping SQL NULL to 0 (for parent_id / folder_id). */
static inline int db_col_int_or_zero(sqlite3_stmt *stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return 0;
    return db_col_int(stmt, col);
}

/* Free a struct that has N string pointers at known offsets.
 * Used by all _free functions. The caller provides a helper macro. */
#define DB_FREE_STR(field) do { free(field); field = NULL; } while(0)

#endif /* ACTA_DB_INTERNAL_H */
