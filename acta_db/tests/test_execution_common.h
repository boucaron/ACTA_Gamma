#ifndef TEST_EXECUTION_COMMON_H
#define TEST_EXECUTION_COMMON_H

#include "db.h"
#include "execution.h"
#include "context.h"
#include "skill.h"
#include "skill_revision.h"
#include "model.h"
#include "model_revision.h"

#include <stdio.h>
#include <string.h>



/* Insert an execution row directly via SQL, temporarily disabling FK checks.
 * Useful for seeding rows without requiring valid FK targets.
 * Returns 0 on success, non-zero on failure. */
static int exec_insert_raw(db_t *db, int execution_id, int ctx_id,
                           int sr_id, int mr_id, int parent_id) {
    acta_db_exec(db, "PRAGMA foreign_keys=OFF;");
    char sql[256];
    if (parent_id > 0) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO executions (id, context_id, skill_revision_id, "
                 "model_revision_id, parent_execution_id) "
                 "VALUES (%d, %d, %d, %d, %d);",
                 execution_id, ctx_id, sr_id, mr_id, parent_id);
    } else {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO executions (id, context_id, skill_revision_id, "
                 "model_revision_id) "
                 "VALUES (%d, %d, %d, %d);",
                 execution_id, ctx_id, sr_id, mr_id);
    }
    int rc = acta_db_exec(db, sql);
    acta_db_exec(db, "PRAGMA foreign_keys=ON;");
    return rc;
}

/* Create a minimal valid context + skill_revision + model_revision.
 * Returns 0 on success, fills out_* with the IDs needed for execution_create. */
static int exec_setup(db_t *db, int *out_ctx, int *out_sr, int *out_mr) {
    /* --- context --- */
    context_t ctx = {0};
    ctx.type         = (char *)"test";
    ctx.content      = (char *)"hello";
    ctx.content_hash = (char *)"deadbeef";
    int ctx_id = 0;
    if (acta_db_context_create(db, &ctx, &ctx_id) != ACTA_DB_OK) return -1;

    /* --- skill --- */
    skill_t sk = {0};
    sk.name            = (char *)"ExecSkill";
    sk.description     = (char *)"desc";
    sk.prompt_template = (char *)"You are helpful.";
    sk.output_schema   = (char *)"json";
    int skill_id = 0;
    if (acta_db_skill_create(db, &sk, &skill_id) != ACTA_DB_OK) return -1;
    int err = 0;
    skill_revision_t *srev = acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    if (!srev) return -1;
    int sr_id = srev->id;
    acta_db_skill_revision_free(srev);

    /* --- model --- */
    model_t m = {0};
    m.name             = (char *)"ExecModel";
    m.backend          = (char *)"openai";
    m.model_identifier = (char *)"gpt-4";
    int model_id = 0;
    if (acta_db_model_create(db, &m, &model_id) != ACTA_DB_OK) return -1;
    model_revision_t *mrev = acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
    if (!mrev) return -1;
    int mr_id = mrev->id;
    acta_db_model_revision_free(mrev);

    *out_ctx = ctx_id;
    *out_sr  = sr_id;
    *out_mr  = mr_id;
    return ACTA_DB_OK;
}

/* Convenience: create an execution with default fields. Returns row id or -1. */
static int exec_create(db_t *db, int ctx_id, int sr_id, int mr_id,
                       const char *prompt, int parent_id) {
    execution_t e;
    memset(&e, 0, sizeof(e));
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.prompt              = (char *)prompt;
    e.status              = ACTA_EXEC_STATUS_PENDING;
    e.parent_execution_id = parent_id;

    int out_id = 0;
    if (acta_db_execution_create(db, &e, &out_id) != ACTA_DB_OK) return -1;
    return out_id;
}



/* ---- sub-runners ---- */
void run_execution_create_tests(void);
void run_execution_lifecycle_tests(void);
void run_execution_list_tests(void);

#endif
