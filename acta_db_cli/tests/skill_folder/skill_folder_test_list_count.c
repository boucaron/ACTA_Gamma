#include "test_helpers.h"

#define REF_DB  "acta_test_ref.db"

/* Reference-DB state per runner (fresh copy each run):
 *
 *    id  name       parent
 *    --  --------   ------
 *     1  authSkill  NULL
 *     2  oauthFlow  1
 */

static int do_list(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("list", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

static int do_count(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("count", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* count non-overlapping occurrences of needle in hay */
static int count_occ(const char *hay, const char *needle)
{
    int n = 0;
    size_t len = strlen(needle);
    const char *p = hay;
    while ((p = strstr(p, needle)) != NULL) { n++; p += len; }
    return n;
}

/* ── list tests ───────────────────────────────────────────────────── */

static void test_list_all(stest_ctx_t *ctx)
{
    /* no positional → all folders (id 1, 2) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_CONTAINS(ctx, out, "\"id\":2");
    TEST_EQ(ctx, count_occ(out, "\"id\":"), 2);
    targs_free(a, &g);
}

static void test_list_all_keyword(stest_ctx_t *ctx)
{
    /* explicit "all" positional → same as no positional */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "all", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    TEST_EQ(ctx, count_occ(out, "\"id\":"), 2);
    targs_free(a, &g);
}

static void test_list_children_of_parent(stest_ctx_t *ctx)
{
    /* parent_id=1 → only id=2 (oauthFlow) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"id\":2");
    /* id=1 is the parent itself, must not appear as a child */
    TEST(ctx, !strstr(out, "\"id\":1"));
    TEST_EQ(ctx, count_occ(out, "\"id\":"), 1);
    targs_free(a, &g);
}

static void test_list_children_none(stest_ctx_t *ctx)
{
    /* parent_id=2 has no children → empty list */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "[]\n");
    targs_free(a, &g);
}

static void test_list_with_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* exactly 1 entry, and it is the first row (id 1) */
    const char *out = stest_stdout(ctx);
    TEST_EQ(ctx, count_occ(out, "\"id\":"), 1);
    TEST_CONTAINS(ctx, out, "\"id\":1");
    targs_free(a, &g);
}

static void test_list_with_offset(stest_ctx_t *ctx)
{
    /* offset=1 → skip first row, get second */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_EQ(ctx, count_occ(out, "\"id\":"), 1);
    TEST_CONTAINS(ctx, out, "\"id\":2");
    targs_free(a, &g);
}

static void test_list_count_flag(stest_ctx_t *ctx)
{
    /* --count (global option) on list → just a number.
     * In the real CLI parse_globals extracts --count into gopts.count,
     * so we exercise the same path here. */
    global_opts_t g = gopts_default();
    g.count = 1;
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");
    targs_free(a, &g);
}

static void test_list_count_flag_with_parent(stest_ctx_t *ctx)
{
    /* --count combined with a parent positional → children count */
    global_opts_t g = gopts_default();
    g.count = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "1\n");
    targs_free(a, &g);
}

static void test_list_fields_shaping(stest_ctx_t *ctx)
{
    /* --fields id,name → no other fields in the JSON */
    global_opts_t g = gopts_default();
    g.fields = (char *)"id,name";
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"name\"");
    TEST(ctx, !strstr(out, "created_at"));
    TEST(ctx, !strstr(out, "updated_at"));
    TEST(ctx, !strstr(out, "parent_id"));
    targs_free(a, &g);
}

static void test_list_no_nulls_shaping(stest_ctx_t *ctx)
{
    /* --no_nulls → omit null string fields (updated_at/deleted_at
     * are NULL for both seed folders) */
    global_opts_t g = gopts_default();
    g.no_nulls = 1;
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "updated_at"));
    TEST(ctx, !strstr(out, "deleted_at"));
    targs_free(a, &g);
}

static void test_list_table_output(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.table = 1;
    cmd_args_t *a = targs_new();

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "authSkill");
    targs_free(a, &g);
}

static void test_list_invalid_offset(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "offset", "abc", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_invalid_limit(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "limit", "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_non_numeric_positional(stest_ctx_t *ctx)
{
    /* strict parse: "abc" is rejected (no more atoi clamping) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_list_negative_positional(stest_ctx_t *ctx)
{
    /* negative parent id is a typo → rejected, not clamped */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_list(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── count tests ──────────────────────────────────────────────────── */

static void test_count_all(stest_ctx_t *ctx)
{
    /* 2 folders in ref DB */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");
    targs_free(a, &g);
}

static void test_count_children(stest_ctx_t *ctx)
{
    /* parent 1 has 1 child (oauthFlow) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "1\n");
    targs_free(a, &g);
}

static void test_count_children_none(stest_ctx_t *ctx)
{
    /* parent 2 has 0 children */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "2", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "0\n");
    targs_free(a, &g);
}

static void test_count_all_keyword(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "all", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_STREQ(ctx, stest_stdout(ctx), "2\n");
    targs_free(a, &g);
}

static void test_count_non_numeric_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "abc", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_count_negative_positional(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_pos(a, "-1", &g);

    int rc = do_count(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_list_count(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_list_all(&ctx);
    test_list_all_keyword(&ctx);
    test_list_children_of_parent(&ctx);
    test_list_children_none(&ctx);
    test_list_with_limit(&ctx);
    test_list_with_offset(&ctx);
    test_list_count_flag(&ctx);
    test_list_count_flag_with_parent(&ctx);
    test_list_fields_shaping(&ctx);
    test_list_no_nulls_shaping(&ctx);
    test_list_table_output(&ctx);
    test_list_invalid_offset(&ctx);
    test_list_invalid_limit(&ctx);
    test_list_non_numeric_positional(&ctx);
    test_list_negative_positional(&ctx);

    test_count_all(&ctx);
    test_count_children(&ctx);
    test_count_children_none(&ctx);
    test_count_all_keyword(&ctx);
    test_count_non_numeric_positional(&ctx);
    test_count_negative_positional(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
