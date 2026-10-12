#ifndef MODEL_TEST_HELPERS_H
#define MODEL_TEST_HELPERS_H

#include "test_helpers.h"

/* ── model-specific seed helpers ──────────────────────────────────── */

/* Insert a row into model_folders on the live test DB.
 * Returns the new folder id, or -1 on error. */
int mtest_seed_folder(stest_ctx_t *ctx, const char *name, int parent_id);

/* Insert a row into models (bypassing the trigger by using raw SQL
 * for the models row, then inserting the initial revision manually).
 * Returns the new model id, or -1 on error.
 *
 * `backend` is written straight into the column — the DB layer does not
 * validate it, so a bad value here is invisible until acta_runner reads
 * the row.  Keep it inside the closed vocabulary (llamacpp | openai,
 * docs/plans/backend_dialects.md §9 #1/#2) so a seed never re-introduces
 * a value the CLI itself would reject. */
int mtest_seed_model(stest_ctx_t *ctx,
                     int folder_id, const char *name,
                     const char *backend, const char *model_identifier,
                     const char *description, const char *base_url,
                     const char *configuration);

/* ── per-file runners ─────────────────────────────────────────────── */
int run_model_test_create(void);
int run_model_test_get(void);
int run_model_test_update(void);
int run_model_test_delete_restore(void);
int run_model_test_move(void);
int run_model_test_list_count(void);
int run_model_test_misc(void);

#endif /* MODEL_TEST_HELPERS_H */
