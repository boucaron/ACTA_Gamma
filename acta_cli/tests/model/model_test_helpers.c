#include "model_test_helpers.h"

#include "internal.h"
#include <sqlite3.h>

/* ── seed helpers ─────────────────────────────────────────────────── */

int mtest_seed_folder(stest_ctx_t *ctx, const char *name, int parent_id)
{
    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO model_folders(name, parent_id, created_at)"
             " VALUES('%s', %d, datetime('now'))",
             name, parent_id);

    char *errmsg = NULL;
    if (sqlite3_exec(ctx->db->handle, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        free(errmsg);
        return -1;
    }
    return (int)sqlite3_last_insert_rowid(ctx->db->handle);
}

int mtest_seed_model(stest_ctx_t *ctx,
                     int folder_id, const char *name,
                     const char *backend, const char *model_identifier,
                     const char *description, const char *base_url,
                     const char *configuration)
{
    /* Build each column fragment separately so NULLs are handled cleanly. */
    char folder[16];
    if (folder_id < 0)
        snprintf(folder, sizeof(folder), "NULL");
    else
        snprintf(folder, sizeof(folder), "%d", folder_id);

    char desc_col[512] = "NULL";
    if (description) snprintf(desc_col, sizeof(desc_col), "'%s'", description);

    char base_col[512] = "NULL";
    if (base_url) snprintf(base_col, sizeof(base_col), "'%s'", base_url);

    char conf_col[512] = "NULL";
    if (configuration) snprintf(conf_col, sizeof(conf_col), "'%s'", configuration);

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "INSERT INTO models(folder_id, name, description, backend,"
             " base_url, model_identifier, configuration, created_at)"
             " VALUES(%s, '%s', %s, '%s', %s, '%s', %s, datetime('now'))",
             folder, name, desc_col, backend, base_col, model_identifier, conf_col);

    char *errmsg = NULL;
    if (sqlite3_exec(ctx->db->handle, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        free(errmsg);
        return -1;
    }
    return (int)sqlite3_last_insert_rowid(ctx->db->handle);
}
