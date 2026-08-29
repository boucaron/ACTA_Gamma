#include "skill_test_helpers.h"

#define REF_DB  "acta_test_ref.db"

static int do_update(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill("update", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* run `skill get <id>` and return the captured stdout */
static const char *do_get(stest_ctx_t *ctx, const char *id)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, id, &g);
    stest_capture_begin(ctx);
    (void)cmd_skill("get", a, &g, ctx->db);
    stest_capture_end(ctx);
    targs_free(a, &g);
    return stest_stdout(ctx);
}

static void test_update_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);  /* "tata" at root */
    targs_flag(a, "name", "tata_renamed", &g);
    targs_flag(a, "prompt_template", "prompt that works", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":2");
    targs_free(a, &g);
}

static void test_update_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "5", &g);  /* "summarize2" at root */
    targs_flag(a, "name", "summarize2_v2", &g);
    targs_flag(a, "prompt_template", "New: {{input}}", &g);
    targs_flag(a, "description", "updated desc", &g);
    targs_flag(a, "folder_id", "1", &g);
    targs_flag(a, "output_schema", "{\"type\":\"object\"}", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_update_prompt_only(stest_ctx_t *ctx)
{
    /* Partial update: only prompt_template, name comes from the
     * live row (no "missing required field" error anymore). */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "prompt_template", "prompt only", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_update_name_preserves_description_and_schema(stest_ctx_t *ctx)
{
    /* P1 regression: a partial update must not wipe fields the
     * caller did not pass. */
    int id = stest_seed_skill(ctx, 0, "p1_base", "orig prompt",
                              "orig desc", "{\"type\":\"string\"}");
    TEST(ctx, id > 0);

    char idstr[16];
    snprintf(idstr, sizeof idstr, "%d", id);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, idstr, &g);
    targs_flag(a, "name", "p1_renamed", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);

    const char *out = do_get(ctx, idstr);
    TEST_CONTAINS(ctx, out, "\"name\":\"p1_renamed\"");
    TEST_CONTAINS(ctx, out, "\"description\":\"orig desc\"");
    TEST_CONTAINS(ctx, out, "\"prompt_template\":\"orig prompt\"");
    TEST_CONTAINS(ctx, out, "\"output_schema\":\"{");
}

static void test_update_name_preserves_folder(stest_ctx_t *ctx)
{
    /* P1 regression: omitting --folder_id must NOT move the skill
     * to the root folder. */
    int fid = stest_seed_folder(ctx, "p1_folder", 0);
    TEST(ctx, fid > 0);
    int id = stest_seed_skill(ctx, fid, "p1_fchild", "orig prompt",
                              "orig desc", NULL);
    TEST(ctx, id > 0);

    char idstr[16], fidstr[16], needle[32];
    snprintf(idstr, sizeof idstr, "%d", id);
    snprintf(fidstr, sizeof fidstr, "%d", fid);

    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, idstr, &g);
    targs_flag(a, "name", "p1_frenamed", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);

    const char *out = do_get(ctx, idstr);
    snprintf(needle, sizeof needle, "\"folder_id\":%s", fidstr);
    TEST_CONTAINS(ctx, out, needle);
}

static void test_update_no_fields_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    /* no fields at all */

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_update_empty_name_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "name", "", &g);
    targs_flag(a, "prompt_template", "prompt that works", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_update_nonexistent_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "99999", &g);
    targs_flag(a, "name", "ghost", &g);
    targs_flag(a, "prompt_template", "boo", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_NOT_FOUND);  /* fetch of the live row fails first */
    targs_free(a, &g);
}

static void test_update_invalid_id(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "0", &g);
    targs_flag(a, "name", "x", &g);
    targs_flag(a, "prompt_template", "y", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_update_missing_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "no id", &g);
    targs_flag(a, "prompt_template", "x", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_update_creates_revision(stest_ctx_t *ctx)
{
    /* After update, a new revision should exist in skill_revisions.
     * Verify via a subsequent get + check that updated_at changed,
     * or by counting revisions if the API exposes it.
     * For now: just verify update succeeds and the skill is still gettable.
     */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "3", &g);  /* "tata" in folder 1 */
    targs_flag(a, "name", "tata_v2", &g);
    targs_flag(a, "prompt_template", "updated prompt", &g);

    int rc = do_update(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);

    /* verify via get */
    global_opts_t gg = gopts_default();
    cmd_args_t *ga = targs_new();
    targs_pos(ga, "3", &gg);
    (void)cmd_skill("get", ga, &gg, ctx->db);
    targs_free(ga, &gg);
    targs_free(a, &g);
}

static void test_update_negative_folder_rejected(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);
    targs_flag(a, "name", "tata", &g);
    targs_flag(a, "prompt_template", "prompt that works", &g);
    targs_flag(a, "folder_id", "-1", &g);

    int rc = do_update(ctx, a, g);
    /* folder_id -1 → EXIT_INVALID (negatives rejected, not clamped to root) */
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_test_update(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_update_name(&ctx);
    test_update_all_fields(&ctx);
    test_update_prompt_only(&ctx);
    test_update_name_preserves_description_and_schema(&ctx);
    test_update_name_preserves_folder(&ctx);
    test_update_no_fields_rejected(&ctx);
    test_update_empty_name_rejected(&ctx);
    test_update_nonexistent_id(&ctx);
    test_update_invalid_id(&ctx);
    test_update_missing_positional(&ctx);
    test_update_creates_revision(&ctx);
    test_update_negative_folder_rejected(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
