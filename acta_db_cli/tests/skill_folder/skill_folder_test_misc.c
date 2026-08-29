#include "../skill/skill_test_helpers.h"
#include <unistd.h>
#include <fcntl.h>

#define REF_DB  "acta_test_ref.db"

/* ── tests ────────────────────────────────────────────────────────── */

static void test_help(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("help", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Usage:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "create");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "rename");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "move");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "delete");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "restore");
    targs_free(a, &g);
}

static void test_unknown_action(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("frobnicate", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* run an action with stderr captured into buf (suggestions go to stderr) */
static int run_action_capturing_stderr(stest_ctx_t *ctx, const char *action,
                                       cmd_args_t *args, global_opts_t gopts,
                                       char *buf, size_t cap)
{
    fflush(NULL);

#ifdef _WIN32
    int saved_err = _dup(STDERR_FILENO);
    int p[2];
    if (saved_err < 0 || _pipe(p, 65536, O_BINARY) != 0) {
        if (saved_err >= 0) _close(saved_err);
        return -1;
    }
    _dup2(p[1], STDERR_FILENO);
    _close(p[1]);
#else
    int saved_err = dup(STDERR_FILENO);
    int p[2];
    if (saved_err < 0 || pipe(p) != 0) {
        if (saved_err >= 0) close(saved_err);
        return -1;
    }
    dup2(p[1], STDERR_FILENO);
    close(p[1]);
#endif


    int rc = cmd_skill_folder(action, args, &gopts, ctx->db);

    fflush(stderr);
    dup2(saved_err, STDERR_FILENO);
    close(saved_err);

    size_t n = 0;
    for (;;) {
        if (n >= cap - 1) break;
        ssize_t r = read(p[0], buf + n, cap - 1 - n);
        if (r <= 0) break;
        n += (size_t)r;
    }
    close(p[0]);
    buf[n] = '\0';
    return rc;
}

static void test_unknown_action_suggests_closest(stest_ctx_t *ctx)
{
    /* "creat" should suggest "create" (on stderr) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    char errbuf[4096];

    int rc = run_action_capturing_stderr(ctx, "creat", a, g,
                                         errbuf, sizeof errbuf);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    TEST_CONTAINS(ctx, errbuf, "Did you mean 'create'?");
    targs_free(a, &g);
}

static void test_get_fields_filter(stest_ctx_t *ctx)
{
    /* --fields name → only name in JSON */
    global_opts_t g = gopts_default();
    g.fields = (char *)"name";
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("get", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\"name\"");
    /* should NOT contain "created_at" */
    TEST(ctx, !strstr(out, "created_at"));
    targs_free(a, &g);
}

static void test_get_no_nulls(stest_ctx_t *ctx)
{
    /* --no_nulls → omit null fields (e.g. updated_at, deleted_at on fresh row) */
    global_opts_t g = gopts_default();
    g.no_nulls = 1;
    cmd_args_t *a = targs_new();
    targs_pos(a, "1", &g);

    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("get", a, &g, ctx->db);
    stest_capture_end(ctx);

    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    /* updated_at and deleted_at are NULL for folder 1 → should be omitted */
    TEST(ctx, !strstr(out, "updated_at"));
    TEST(ctx, !strstr(out, "deleted_at"));
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_misc(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_help(&ctx);
    test_unknown_action(&ctx);
    test_unknown_action_suggests_closest(&ctx);
    test_get_fields_filter(&ctx);
    test_get_no_nulls(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
