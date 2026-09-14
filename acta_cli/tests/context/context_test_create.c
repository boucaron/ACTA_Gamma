#include "test_helpers.h"
#include "sha256.h"

#include <string.h>

#define REF_DB "acta_test_ref.db"

/* SHA-256 of data → 64 lowercase hex chars + NUL in out (>= 65 bytes). */
static char *sha256_hex(const void *data, size_t len, char out[65])
{
    static const char hexd[] = "0123456789abcdef";
    SHA256_CTX ctx;
    BYTE digest[SHA256_BLOCK_SIZE];
    size_t i;

    sha256_init(&ctx);
    sha256_update(&ctx, (const BYTE *)data, len);
    sha256_final(&ctx, digest);
    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        out[i * 2]     = hexd[digest[i] >> 4];
        out[i * 2 + 1] = hexd[digest[i] & 0x0f];
    }
    out[64] = '\0';
    return out;
}

/* ── helpers local to this file ──────────────────────────────────── */

static int do_create(stest_ctx_t *ctx, cmd_args_t *args, global_opts_t gopts)
{
    stest_capture_begin(ctx);
    int rc = cmd_context("create", args, &gopts, ctx->db);
    stest_capture_end(ctx);
    return rc;
}

/* ── tests ────────────────────────────────────────────────────────── */

static void test_create_basic(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "hello", &g);
    targs_flag(a, "hash", "testhash1", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\"");
    targs_free(a, &g);
}

static void test_create_all_fields(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "session", &g);
    targs_flag(a, "content", "full payload", &g);
    targs_flag(a, "hash", "fullhash", &g);
    targs_flag(a, "metadata", "{\"k\":\"v\"}", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

static void test_create_id_only(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_id_only();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "idonly", &g);
    targs_flag(a, "hash", "idonlyhash", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    const char *out = stest_stdout(ctx);
    TEST(ctx, !strstr(out, "{"));
    TEST_CONTAINS(ctx, out, "\n");
    targs_free(a, &g);
}

static void test_create_missing_type(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no --type */
    targs_flag(a, "content", "orphan", &g);
    targs_flag(a, "hash", "orphanhash", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

static void test_create_missing_content(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    /* no --content */
    targs_flag(a, "hash", "nocontenthash", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* Known SHA-256 vectors: FIPS 180-2 ("", "abc") plus block-boundary
 * lengths (55/56 = single-block padding edges, 64/119 = multi-block,
 * 1000 = many blocks), checked against Python's hashlib. */
static void test_sha256_vectors(stest_ctx_t *ctx)
{
    char hex[65];

    TEST_EQ(ctx, strcmp(sha256_hex("", 0, hex),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), 0);
    TEST_EQ(ctx, strcmp(sha256_hex("abc", 3, hex),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), 0);

    char buf[1024];
    size_t n;

    for (n = 0; n < 1000; n++)
        buf[n] = 'a';
    TEST_EQ(ctx, strcmp(sha256_hex(buf, 55, hex),
        "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"), 0);
    TEST_EQ(ctx, strcmp(sha256_hex(buf, 56, hex),
        "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"), 0);
    TEST_EQ(ctx, strcmp(sha256_hex(buf, 64, hex),
        "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"), 0);
    TEST_EQ(ctx, strcmp(sha256_hex(buf, 119, hex),
        "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"), 0);
    TEST_EQ(ctx, strcmp(sha256_hex(buf, 1000, hex),
        "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"), 0);
}

/* Omitted --hash must default to SHA-256(content), lowercase hex —
 * the same rule the GUI applies. */
static void test_create_missing_hash_defaults_to_sha256(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "nohash", &g);
    /* no --hash → derived */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);

    /* create prints only {"id":N}; verify the stored hash via get */
    const char *out = stest_stdout(ctx);
    const char *p = strstr(out, "\"id\":");
    TEST_NOT_NULL(ctx, p);
    int id = (int)atoi(p + 5);

    cmd_args_t *ag = targs_new();
    char idstr[16];
    snprintf(idstr, sizeof idstr, "%d", id);
    targs_pos(ag, idstr, &g);
    stest_capture_begin(ctx);
    rc = cmd_context("get", ag, &g, ctx->db);
    stest_capture_end(ctx);
    TEST_EQ(ctx, rc, EXIT_OK);
    /* SHA-256("nohash") */
    TEST_CONTAINS(ctx, stest_stdout(ctx),
        "dd3ada42190c728ed157609e9b768eed295bc839ae3bb13db4f33d5850cd40a8");
    targs_free(ag, &g);
    targs_free(a, &g);
}

static void test_create_all_missing(stest_ctx_t *ctx)
{
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    /* no flags at all → "misspelled flag" warning path */

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_INVALID);
    targs_free(a, &g);
}

/* ── input-source regression (P2) ───────────────────────────────────
 *  --json <blob> (space + '=' form), --from_file (present + missing),
 *  --stdin, and conflicting sources — run through parse_globals +
 *  handler exactly like main.c does (stest_run_argv). */

static void test_create_json_invalid(stest_ctx_t *ctx)
{
    /* --json <garbage> → EXIT_INVALID (the blob is used verbatim;
     * stdin is not read). */
    char *argv0[] = { "acta_cli", "context", "create",
                      "--json", "this is not json" };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_json_space(stest_ctx_t *ctx)
{
    /* --json <blob> (space form): the flag value must be honoured
     * (P2: it used to be ignored and stdin read instead). */
    char *argv0[] = { "acta_cli", "context", "create",
                      "--json",
                      "{\"type\":\"text\",\"content\":\"src json space\","
                      "\"hash\":\"srchash_space\"}" };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_json_equals(stest_ctx_t *ctx)
{
    /* --json=<blob> (equals form) */
    char *argv0[] = { "acta_cli", "context", "create",
                      "--json={\"type\":\"text\",\"content\":\"src json "
                      "equals\",\"hash\":\"srchash_equals\"}" };
    int rc = stest_run_argv(ctx, cmd_context, 4, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_present(stest_ctx_t *ctx)
{
    const char *path = stest_write_input(ctx,
        "{\"type\":\"text\",\"content\":\"src from_file\","
        "\"hash\":\"srchash_file\"}");
    TEST_NOT_NULL(ctx, path);
    char *argv0[] = { "acta_cli", "context", "create",
                      "--from_file", (char *)path };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_from_file_missing(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "create",
                      "--from_file", "./tmp/acta_no_such_input.json" };
    int rc = stest_run_argv(ctx, cmd_context, 5, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "create", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_context, 4, argv0,
        "{\"type\":\"text\",\"content\":\"src stdin\","
        "\"hash\":\"srchash_stdin\"}");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "\"id\":");
}

static void test_create_src_conflict_json_stdin(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "create",
                      "--json", "{}", "--stdin" };
    int rc = stest_run_argv(ctx, cmd_context, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_json_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "create",
                      "--json", "{}", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_context, 7, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_src_conflict_stdin_file(stest_ctx_t *ctx)
{
    char *argv0[] = { "acta_cli", "context", "create",
                      "--stdin", "--from_file",
                      "./tmp/acta_conflict_input.json" };
    int rc = stest_run_argv(ctx, cmd_context, 6, argv0, "");
    TEST_EQ(ctx, rc, EXIT_INVALID);
}

static void test_create_duplicate_content_ok(stest_ctx_t *ctx)
{
    /* contexts has no unique index on (type, content, hash),
     * so inserting a duplicate should succeed. */
    global_opts_t g = gopts_default();
    cmd_args_t *a = targs_new();
    targs_flag(a, "type", "text", &g);
    targs_flag(a, "content", "hello world", &g);
    targs_flag(a, "hash", "abc123", &g);

    int rc = do_create(ctx, a, g);
    TEST_EQ(ctx, rc, EXIT_OK);
    targs_free(a, &g);
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_context_test_create(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_create_basic(&ctx);
    test_create_all_fields(&ctx);
    test_create_id_only(&ctx);
    test_create_missing_type(&ctx);
    test_create_missing_content(&ctx);
    test_sha256_vectors(&ctx);
    test_create_missing_hash_defaults_to_sha256(&ctx);
    test_create_all_missing(&ctx);
    test_create_json_invalid(&ctx);
    test_create_src_json_space(&ctx);
    test_create_src_json_equals(&ctx);
    test_create_src_from_file_present(&ctx);
    test_create_src_from_file_missing(&ctx);
    test_create_src_stdin(&ctx);
    test_create_src_conflict_json_stdin(&ctx);
    test_create_src_conflict_json_file(&ctx);
    test_create_src_conflict_stdin_file(&ctx);
    test_create_duplicate_content_ok(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
