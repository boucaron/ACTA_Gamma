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

/*
 * Test helpers for execution tests.
 *
 * Conventions:
 *   – Setup helpers return 0 on success, -1 on failure.
 *   – ID-returning helpers return the row id (> 0) on success, -1 on failure.
 *   – All helpers assume an in-memory or per-test DB; they do not clean up
 *     on partial failure.  (If context_create succeeds but skill_create
 *     fails, the context row is left behind.  With a fresh :memory: DB
 *     per test this is invisible.)
 *   – String-literal-to-char* casts are intentional: the C API uses
 *     `char *` fields, the API only reads and copies them.
 */

/* Suppress -Wunused-function for TUs that include this header but do
 * not call exec_insert_raw. */
#if defined(__GNUC__) || defined(__clang__)
#define TEST_EXEC_UNUSED __attribute__((unused))
#else
#define TEST_EXEC_UNUSED
#endif

/* ------------------------------------------------------------------ */
/*  Raw INSERT for seeding rows without valid FK targets.             */
/*                                                                    */
/*  ⚠  PRAGMA foreign_keys is a no-op inside a transaction.          */
/*     If the test harness wraps setup in acta_db_transaction,        */
/*     this function will fail on the FK constraint.                 */
/*     Use only outside a transaction, or create real FK targets      */
/*     via exec_setup instead.                                       */
/*                                                                    */
/*  Returns ACTA_DB_OK (0) on success, ACTA_DB_ERR_SQL on failure.   */
/* ------------------------------------------------------------------ */
static TEST_EXEC_UNUSED int exec_insert_raw(db_t *db, int execution_id, int ctx_id,
                           int sr_id, int mr_id, int parent_id) {
    acta_db_exec(db, "PRAGMA foreign_keys=OFF;");

    char sql[256];
    if (parent_id > 0) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO executions "
                 "  (id, context_id, skill_revision_id, model_revision_id, "
                 "   parent_execution_id, status) "
                 "VALUES (%d, %d, %d, %d, %d, 'pending');",
                 execution_id, ctx_id, sr_id, mr_id, parent_id);
    } else {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO executions "
                 "  (id, context_id, skill_revision_id, model_revision_id, "
                 "   status) "
                 "VALUES (%d, %d, %d, %d, 'pending');",
                 execution_id, ctx_id, sr_id, mr_id);
    }
    int rc = acta_db_exec(db, sql);
    acta_db_exec(db, "PRAGMA foreign_keys=ON;");
    return rc;
}

/* ------------------------------------------------------------------ */
/*  Create a minimal valid context + skill_revision + model_revision. */
/*  Returns 0 on success, -1 on failure.                             */
/*  Fills out_ctx / out_sr / out_mr with the IDs needed by           */
/*  acta_db_execution_create.                                         */
/* ------------------------------------------------------------------ */
static int exec_setup(db_t *db, int *out_ctx, int *out_sr, int *out_mr) {
    int err;

    /* context */
    context_t ctx = {0};
    ctx.type         = (char *)"test";
    ctx.content      = (char *)"hello";
    ctx.content_hash = (char *)"deadbeef";
    int ctx_id = 0;
    if (acta_db_context_create(db, &ctx, &ctx_id) != ACTA_DB_OK) return -1;

    /* skill → revision 1 */
    skill_t sk = {0};
    sk.name            = (char *)"ExecSkill";
    sk.description     = (char *)"desc";
    sk.prompt_template = (char *)"You are helpful.";
    sk.output_schema   = (char *)"json";
    int skill_id = 0;
    if (acta_db_skill_create(db, &sk, &skill_id) != ACTA_DB_OK) return -1;
    skill_revision_t *srev =
        acta_db_skill_revision_get_by_skill_and_rev(db, skill_id, 1, &err);
    if (!srev) return -1;
    int sr_id = srev->id;
    acta_db_skill_revision_free(srev);

    /* model → revision 1 */
    model_t m = {0};
    m.name             = (char *)"ExecModel";
    m.backend          = (char *)"openai";
    m.model_identifier = (char *)"gpt-4";
    int model_id = 0;
    if (acta_db_model_create(db, &m, &model_id) != ACTA_DB_OK) return -1;
    model_revision_t *mrev =
        acta_db_model_revision_get_by_model_and_rev(db, model_id, 1, &err);
    if (!mrev) return -1;
    int mr_id = mrev->id;
    acta_db_model_revision_free(mrev);

    *out_ctx = ctx_id;
    *out_sr  = sr_id;
    *out_mr  = mr_id;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Create an execution with default fields.                         */
/*  (executions.prompt is a legacy column: never written by the      */
/*   create path, so it is always SQL NULL in rows created here.)    */
/*  Returns the new row id (> 0) on success, -1 on failure.          */
/* ------------------------------------------------------------------ */
static int exec_create(db_t *db, int ctx_id, int sr_id, int mr_id,
                       int parent_id) {
    execution_t e = {0};
    e.context_id          = ctx_id;
    e.skill_revision_id   = sr_id;
    e.model_revision_id   = mr_id;
    e.status              = ACTA_EXEC_STATUS_PENDING;
    e.parent_execution_id = parent_id;

    int out_id = 0;
    if (acta_db_execution_create(db, &e, &out_id) != ACTA_DB_OK) return -1;
    return out_id;
}

/* ------------------------------------------------------------------ */
/*  Sub-runners                                                       */
/* ------------------------------------------------------------------ */
int run_execution_create_tests(void);
int run_execution_lifecycle_tests(void);
int run_execution_list_tests(void);

#endif
