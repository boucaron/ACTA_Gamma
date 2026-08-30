/* ─────────────────────────────────────────────────────────────────────
 * execution_test_lifecycle.c
 * Unit tests for:  exec start / cancel / complete / fail / set-raw
 * ───────────────────────────────────────────────────────────────────── */
#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* ── local helper ──────────────────────────────────────────────────── */

static int do_exec(stest_ctx_t *ctx, const char *action,
                  cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_exec(action, args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ══════════════════════════════════════════════════════════════════ */
/*  start                                                            */
/* ══════════════════════════════════════════════════════════════════ */

static void test_start_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_exec(ctx, "start", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_start_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */

    int rc = do_exec(ctx, "start", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_start_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_exec(ctx, "start", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_start_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_exec(ctx, "start", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  cancel                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static void test_cancel_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_exec(ctx, "cancel", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_cancel_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "cancel", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_cancel_id_zero(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);

    int rc = do_exec(ctx, "cancel", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_cancel_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_exec(ctx, "cancel", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  complete                                                         */
/* ══════════════════════════════════════════════════════════════════ */

static void test_complete_with_result(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();

    /* exec 3 is 'pending' in ref DB — must transition to 'running' first */
    {
        cmd_args_t *sa = targs_new();
        targs_pos(sa, "3", &g);
        int rc = do_exec(ctx, "start", sa, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(sa, &g);
    }

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "3", &g);
        targs_flag(a, "result", "done successfully", &g);

        int rc = do_exec(ctx, "complete", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
}


static void test_complete_no_result(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();

    /* use id=5 (still 'pending' — untouched by earlier tests) */
    {
        cmd_args_t *sa = targs_new();
        targs_pos(sa, "5", &g);
        int rc = do_exec(ctx, "start", sa, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(sa, &g);
    }

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "5", &g);
        /* no --result (optional) */

        int rc = do_exec(ctx, "complete", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
}

static void test_complete_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "complete", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_complete_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_exec(ctx, "complete", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}


/* ══════════════════════════════════════════════════════════════════ */
/*  fail                                                             */
/* ══════════════════════════════════════════════════════════════════ */

static void test_fail_with_error(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();

    /* id=4 is 'pending' — must transition to 'running' first */
    {
        cmd_args_t *sa = targs_new();
        targs_pos(sa, "4", &g);
        int rc = do_exec(ctx, "start", sa, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(sa, &g);
    }

    {
        cmd_args_t *a = targs_new();
        targs_pos(a, "4", &g);
        targs_flag(a, "error", "timeout after 30s", &g);
        int rc = do_exec(ctx, "fail", a, g);
        TEST_EQ(ctx, rc, EXIT_OK);
        targs_free(a, &g);
    }
}


static void test_fail_no_error(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();

    /* id=1 is 'running' at this point (started by test_start_basic);
     * use it so we don't collide with test_fail_with_error on id=4 */
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    /* no --error (optional) */

    int rc = do_exec(ctx, "fail", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_fail_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_exec(ctx, "fail", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_fail_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);

    int rc = do_exec(ctx, "fail", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}


/* ══════════════════════════════════════════════════════════════════ */
/*  set-raw                                                          */
/* ══════════════════════════════════════════════════════════════════ */

static void test_set_raw_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    targs_flag(a, "raw", "raw model output text", &g);

    int rc = do_exec(ctx, "set-raw", a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_set_raw_missing_raw(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);
    /* no --raw (required) */

    int rc = do_exec(ctx, "set-raw", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_set_raw_missing_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no positional */
    targs_flag(a, "raw", "x", &g);

    int rc = do_exec(ctx, "set-raw", a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_set_raw_nonexistent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "9999", &g);
    targs_flag(a, "raw", "x", &g);

    int rc = do_exec(ctx, "set-raw", a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ────────────────────────────────────────────────────────── */

int run_execution_test_lifecycle(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    /* start */
    test_start_basic(&ctx);
    test_start_missing_id(&ctx);
    test_start_id_zero(&ctx);
    test_start_nonexistent(&ctx);

    /* cancel */
    test_cancel_basic(&ctx);
    test_cancel_missing_id(&ctx);
    test_cancel_id_zero(&ctx);
    test_cancel_nonexistent(&ctx);

    /* complete */
    test_complete_with_result(&ctx);
    test_complete_no_result(&ctx);
    test_complete_missing_id(&ctx);
    test_complete_nonexistent(&ctx);

    /* fail */
    test_fail_with_error(&ctx);
    test_fail_no_error(&ctx);
    test_fail_missing_id(&ctx);
    test_fail_nonexistent(&ctx);

    /* set-raw */
    test_set_raw_basic(&ctx);
    test_set_raw_missing_raw(&ctx);
    test_set_raw_missing_id(&ctx);
    test_set_raw_nonexistent(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
