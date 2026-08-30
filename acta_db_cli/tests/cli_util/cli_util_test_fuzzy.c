/* cli_util_test_fuzzy.c — unit tests for the cli_util.h fuzzy-matching
 * primitives (edit_distance / closest_name / closest_action).
 *
 * closest_name is the flat name-list matcher shared by:
 *   - closest_action()  → every unknown_action() call site (10 entities)
 *   - entity_not_found() in commands.c (unknown-entity suggestion, 73591d4)
 *
 * The acceptance threshold is pinned per the documented contract in
 * cli_util.h: per target name, ≤ 3 chars accept 1 edit only, longer
 * names accept up to 2; an exact match (d=0) never "suggests" itself.
 */
#include "test_helpers.h"

#define REF_DB "acta_test_ref.db"

/* Entity name pool, mirroring entity_table[] in commands.c. */
static const char *const ENTITIES[] = {
    "db", "context", "model", "model_folder", "model_revision",
    "skill", "skill_folder", "skill_revision", "exec", "log"
};
#define N_ENTITIES (sizeof(ENTITIES) / sizeof(ENTITIES[0]))

/* ── edit_distance ────────────────────────────────────────────────── */

static void test_edit_distance_basics(stest_ctx_t *ctx)
{
    TEST_EQ(ctx, edit_distance("model", "model"), 0);
    TEST_EQ(ctx, edit_distance("model", "modul"), 1);   /* substitution (e→u) */
    TEST_EQ(ctx, edit_distance("model", "modelx"), 1);  /* insertion  */
    TEST_EQ(ctx, edit_distance("model", "mdel"), 1);    /* deletion   */
    TEST_EQ(ctx, edit_distance("model", "modle"), 2);   /* transposition = 2 (Levenshtein, not Damerau) */
    TEST_EQ(ctx, edit_distance("model", "mold"), 3);    /* delete 'e' + d/l transposition */
    TEST_EQ(ctx, edit_distance("", "abc"), 3);

    /* > 60 chars bails out at 61 without touching the 64x64 DP buffer. */
    char longstr[64];
    memset(longstr, 'a', 61);
    longstr[61] = '\0';
    TEST_EQ(ctx, edit_distance(longstr, "abc"), 61);
}

/* ── closest_name: degenerate inputs ──────────────────────────────── */

static void test_closest_name_inputs(stest_ctx_t *ctx)
{
    TEST_NULL(ctx, closest_name(NULL, ENTITIES, N_ENTITIES));
    TEST_NULL(ctx, closest_name("", ENTITIES, N_ENTITIES));
    TEST_NULL(ctx, closest_name("modle", NULL, 0));
    TEST_NULL(ctx, closest_name(NULL, NULL, 0));
}

/* Exact match is not a typo: d=0 never fires (`d > 0` in the matcher). */
static void test_closest_name_exact_match_is_null(stest_ctx_t *ctx)
{
    TEST_NULL(ctx, closest_name("model", ENTITIES, N_ENTITIES));
    TEST_NULL(ctx, closest_name("db", ENTITIES, N_ENTITIES));
}

/* ── closest_name: thresholds ─────────────────────────────────────── */

static void test_closest_name_long_targets(stest_ctx_t *ctx)
{
    /* 1 edit */
    TEST_STREQ(ctx, closest_name("model_", ENTITIES, N_ENTITIES), "model");
    TEST_STREQ(ctx, closest_name("model_foler", ENTITIES, N_ENTITIES),
               "model_folder");
    /* 2 edits — still accepted for long targets */
    TEST_STREQ(ctx, closest_name("modle", ENTITIES, N_ENTITIES), "model");  /* transposition */
    TEST_STREQ(ctx, closest_name("mdl", ENTITIES, N_ENTITIES), "model");
    /* 3+ edits — never */
    TEST_NULL(ctx, closest_name("xyz", ENTITIES, N_ENTITIES));
    TEST_NULL(ctx, closest_name("mxyz", ENTITIES, N_ENTITIES));
}

static void test_closest_name_short_targets(stest_ctx_t *ctx)
{
    /* "db" is ≤ 3 chars: only 1 edit is meaningful */
    TEST_STREQ(ctx, closest_name("dbx", ENTITIES, N_ENTITIES), "db");
    TEST_STREQ(ctx, closest_name("dbs", ENTITIES, N_ENTITIES), "db");
    /* 2 edits off a short target: no false positive */
    TEST_NULL(ctx, closest_name("dxy", ENTITIES, N_ENTITIES));
}

static void test_closest_name_nearest_wins(stest_ctx_t *ctx)
{
    /* Two candidates within threshold → smaller distance wins
     * (d("skl","skil")==1 beats d("skl","skill")==2). */
    const char *names[] = { "skill", "skil", "zzz" };
    TEST_STREQ(ctx, closest_name("skl", names, 3), "skil");

    /* Tie (equal distance) → first in list (strict `<` keeps the first). */
    const char *tie[] = { "model", "modle" };
    TEST_STREQ(ctx, closest_name("modl", tie, 2), "model");
}

/* ── closest_action: wrapper over action_def_t ────────────────────── */

static void test_closest_action(stest_ctx_t *ctx)
{
    action_def_t actions[] = {
        { "create", "create a row" },
        { "get",    "fetch a row"  },
        { "list",   "list rows"    },
    };
    const size_t n = sizeof(actions) / sizeof(actions[0]);

    TEST_STREQ(ctx, closest_action("cretae", actions, n), "create");
    /* short target ("get"): 1 edit fires, 2 does not */
    TEST_STREQ(ctx, closest_action("gets", actions, n), "get");
    TEST_NULL(ctx, closest_action("gz", actions, n));
    TEST_NULL(ctx, closest_action("frobnicate", actions, n));
    TEST_NULL(ctx, closest_action("cretae", actions, 0));
    TEST_NULL(ctx, closest_action("", actions, n));
    TEST_NULL(ctx, closest_action(NULL, actions, n));
}

/* ── runner ───────────────────────────────────────────────────────── */

int run_cli_util_test_fuzzy(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    test_edit_distance_basics(&ctx);
    test_closest_name_inputs(&ctx);
    test_closest_name_exact_match_is_null(&ctx);
    test_closest_name_long_targets(&ctx);
    test_closest_name_short_targets(&ctx);
    test_closest_name_nearest_wins(&ctx);
    test_closest_action(&ctx);

    int f = ctx.failures;
    stest_teardown(&ctx);
    return f;
}
