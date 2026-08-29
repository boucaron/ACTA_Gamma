#include "../skill/skill_test_helpers.h"
#include <unistd.h>
#include <fcntl.h> 

#define REF_DB  "acta_test_ref.db"

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_skill_folder("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    /* create a root-level folder with just a name */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "BrandNewFolder", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_with_parent(stest_ctx_t *ctx)
{
    /* create a child under folder 1 (authSkill) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "ChildOfAuth", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "IdOnlySF", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* output should be a bare integer + newline, no braces */
    const char *out = stest_stdout(ctx);
    TEST_CONTAINS(ctx, out, "\n");
    TEST(ctx, !strstr(out, "{"));
    targs_free(a, &g);
}

static void test_create_missing_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --name */
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_empty_name(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_root_unique_violation(stest_ctx_t *ctx)
{
    /* "authSkill" already exists at root (id=1 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "authSkill", &g);

    int rc = do_create(ctx, a, g);
    /* should fail: uq_skill_folders_root on (name) WHERE parent_id IS NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_child_unique_violation(stest_ctx_t *ctx)
{
    /* "oauthFlow" already exists in parent 1 (id=2 in ref DB) */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "oauthFlow", &g);
    targs_flag(a, "parent_id", "1", &g);

    int rc = do_create(ctx, a, g);
    /* should fail: uq_skill_folders_child on (parent_id, name) WHERE parent_id IS NOT NULL */
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

static void test_create_same_name_different_parent_ok(stest_ctx_t *ctx)
{
    /* "oauthFlow" exists under parent 1 (id=2).
     * Create "oauthFlow" under parent 0 (root) → should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "oauthFlow", &g);
    /* no parent_id → defaults to 0 (root) */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_negative_parent_rejected(stest_ctx_t *ctx)
{
    /* parse_folder_id rejects negatives (typo guard) → EXIT_INVALID;
     * they are NOT clamped to root. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "NegParent", &g);
    targs_flag(a, "parent_id", "-3", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_non_numeric_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "BadParent", &g);
    targs_flag(a, "parent_id", "abc", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_nonexistent_parent(stest_ctx_t *ctx)
{
    /* parent_id=99999 does not exist → FK violation */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "name", "OrphanChild", &g);
    targs_flag(a, "parent_id", "99999", &g);

    int rc = do_create(ctx, a, g);
    TEST(ctx, rc != EXIT_OK);
    targs_free(a, &g);
}

/* ── --json input: feed stdin through a pipe (unit-testable) ─────── */

static char g_json_stdout[8192];

/* Run the create action with stdin fed from stdin_blob and stdout
 * captured into g_json_stdout. Returns the handler rc, or -1 if the
 * fd plumbing itself failed. */
static int run_create_json(stest_ctx_t *ctx, cmd_args_t *args,
                           global_opts_t gopts, const char *stdin_blob)
{
    fflush(NULL);

#ifdef _WIN32
    int saved_in  = _dup(STDIN_FILENO);
    int saved_out = _dup(STDOUT_FILENO);
    int in_p[2], out_p[2];
    if (saved_in < 0 || saved_out < 0 ||
        _pipe(in_p,  65536, O_BINARY) != 0 ||
        _pipe(out_p, 65536, O_BINARY) != 0) {
        if (saved_in  >= 0) _close(saved_in);
        if (saved_out >= 0) _close(saved_out);
        return -1;
    }
#else
    int saved_in  = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int in_p[2], out_p[2];
    if (saved_in < 0 || saved_out < 0 ||
        pipe(in_p) != 0 || pipe(out_p) != 0) {
        if (saved_in  >= 0) close(saved_in);
        if (saved_out >= 0) close(saved_out);
        return -1;
    }
#endif

    const char *data = stdin_blob ? stdin_blob : "";
    size_t blen = strlen(data);
    size_t off = 0;
    while (off < blen) {
        ssize_t w = write(in_p[1], data + off, blen - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    close(in_p[1]);

    dup2(in_p[0], STDIN_FILENO);
    dup2(out_p[1], STDOUT_FILENO);
    close(in_p[0]);
    close(out_p[1]);

    /* ── debug: verify what the pipe actually holds ── */
    /* {
        char dbg[256] = {0};
        ssize_t dr = read(STDIN_FILENO, dbg, sizeof(dbg) - 1);
        fprintf(stderr, "DEBUG run_create_json: read %zd bytes: '%s'\n", dr, dbg);
    } */
    /* ── end debug ── */

    int rc = cmd_skill_folder("create", args, &gopts, ctx->db);

    fflush(stdout);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_in, STDIN_FILENO);
    close(saved_in);
    close(saved_out);

    size_t n = 0;
    for (;;) {
        if (n >= sizeof g_json_stdout - 1) break;
        ssize_t r = read(out_p[0], g_json_stdout + n,
                         sizeof g_json_stdout - 1 - n);
        if (r <= 0) break;
        n += (size_t)r;
    }
    close(out_p[0]);
    g_json_stdout[n] = '\0';
    return rc;
}


static void test_create_json_invalid(stest_ctx_t *ctx)
{
    /* --json with garbage stdin → EXIT_INVALID */
    global_opts_t g = gopts_json();
    cmd_args_t *a = targs_new();

    int rc = run_create_json(ctx, a, g, "this is not json");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_json_missing_name(stest_ctx_t *ctx)
{
    /* valid JSON object but no 'name' → EXIT_INVALID */
    global_opts_t g = gopts_json();
    cmd_args_t *a = targs_new();

    int rc = run_create_json(ctx, a, g, "{}");
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_json_valid(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.json_input = (char *)"{\"name\":\"JsonFolder\"}";
    cmd_args_t *a = targs_new();

    int rc = run_create_json(ctx, a, g, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, g_json_stdout, "\"id\":");
    targs_free(a, &g);
}

static void test_create_json_with_parent(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    g.json_input = (char *)"{\"name\":\"JsonChild\",\"parent_id\":1}";
    cmd_args_t *a = targs_new();

    int rc = run_create_json(ctx, a, g, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, g_json_stdout, "\"id\":");
    targs_free(a, &g);
}



/* ── runner ───────────────────────────────────────────────────────── */

int run_skill_folder_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_with_parent(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_name(&ctx);
    test_create_empty_name(&ctx);
    test_create_root_unique_violation(&ctx);
    test_create_child_unique_violation(&ctx);
    test_create_same_name_different_parent_ok(&ctx);
    test_create_negative_parent_rejected(&ctx);
    test_create_non_numeric_parent(&ctx);
    test_create_nonexistent_parent(&ctx);
    test_create_json_invalid(&ctx);
    test_create_json_missing_name(&ctx);
    test_create_json_valid(&ctx);
    test_create_json_with_parent(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
