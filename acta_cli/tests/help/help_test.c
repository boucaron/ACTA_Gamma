/* help_test.c — P0: per-action help.
 *
 * Covers the handler-level entry point (`<entity> help <action>`)
 * for all ten entities:
 *   H1  `X help <action>` prints only that action's section (rc 0,
 *       section header present, other sections and the "Actions:"
 *       header absent)
 *   H2  `X help` (no sub-positional) still prints the full entity help
 *   H3  `X help help` also prints the full entity help
 *   H4  `X help <unknown>` → EXIT_CLI (canonical unknown-action path)
 *
 * The main-level entry point (`<entity> <action> --help`, unknown
 * entity/action with --help) is pinned in tests/gparse via
 * parse_globals + entity_help.
 */
#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* Run `fn("help", …)` with an optional sub-positional, capturing
 * stdout. Returns the handler rc. */
static int do_help(stest_ctx_t *ctx, stest_cmd_fn_t fn, const char *sub)
{
    global_opts_t g = gopts_default();
    cmd_args_t   *a = targs_new();
    if (sub)
        targs_pos(a, sub, &g);

    stest_capture_begin(ctx);
    int rc = fn("help", a, &g, ctx->db);
    stest_capture_end(ctx);

    targs_free(a, &g);
    return rc;
}

/* ── H1: one section per (entity, action) ────────────────────────── */

typedef struct {
    stest_cmd_fn_t fn;
    const char     *action;
    const char     *needle;      /* must be present */
    const char     *anti_needle; /* must NOT be present (another section) */
} help_case_t;

static const help_case_t cases[] = {
    { cmd_db,            "init",      "== init",       "== version"    },
    { cmd_context,       "create",    "== create",     "== list"       },
    { cmd_context,       "list",      "== list",       "== create"     },
    { cmd_model,         "create",    "== create",     "== move"       },
    { cmd_model,         "move",      "== move",       "== create"     },
    { cmd_model_folder,  "rename",    "== rename",     "== move"       },
    { cmd_model_revision,"get-latest","== get-latest", "== count"      },
    { cmd_skill,         "update",    "== update",     "== list"       },
    { cmd_skill_folder,  "move",      "== move",       "== rename"     },
    { cmd_skill_rev,     "count",     "== count",      "== list"       },
    { cmd_exec,          "set-raw",   "== set-raw",    "== start"      },
    { cmd_exec,          "cancel",    "== cancel",     "== set-raw"    },
    { cmd_execution_log, "create",    "== create",     "== list"       },
};

static void test_per_action(stest_ctx_t *ctx)
{
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const help_case_t *c = &cases[i];
        int rc = do_help(ctx, c->fn, c->action);
        TEST_EQ(ctx, rc, EXIT_OK);
        TEST_CONTAINS(ctx, stest_stdout(ctx), c->needle);
        TEST(ctx, strstr(stest_stdout(ctx), c->anti_needle) == NULL);
        /* a single section must not carry the full-help header */
        TEST(ctx, strstr(stest_stdout(ctx), "Actions:") == NULL);
    }
}

/* ── H2: bare `X help` → full entity help ─────────────────────────── */

static void test_full_help(stest_ctx_t *ctx)
{
    int rc = do_help(ctx, cmd_model, NULL);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Actions:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "== create");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "== move");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "help <action>");

    rc = do_help(ctx, cmd_db, NULL);
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Actions:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "== init");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "== version");
}

/* ── H3: `X help help` → full entity help ─────────────────────────── */

static void test_help_help(stest_ctx_t *ctx)
{
    int rc = do_help(ctx, cmd_context, "help");
    TEST_EQ(ctx, rc, EXIT_OK);
    TEST_CONTAINS(ctx, stest_stdout(ctx), "Actions:");
    TEST_CONTAINS(ctx, stest_stdout(ctx), "== create");
}

/* ── H4: unknown sub-action → EXIT_CLI ────────────────────────────── */

static void test_unknown_action(stest_ctx_t *ctx)
{
    TEST_EQ(ctx, do_help(ctx, cmd_model, "nosuch"), EXIT_CLI);
    TEST_EQ(ctx, do_help(ctx, cmd_exec,  "nosuch"), EXIT_CLI);
    TEST_EQ(ctx, do_help(ctx, cmd_db,    "nosuch"), EXIT_CLI);
}

/* ── suite entry point ───────────────────────────────────────────── */

int run_help_test_all(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_per_action(&ctx);
    test_full_help(&ctx);
    test_help_help(&ctx);
    test_unknown_action(&ctx);

    stest_teardown(&ctx);
    return ctx.failures;
}
