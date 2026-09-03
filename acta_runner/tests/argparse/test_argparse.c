/*
 * test_argparse.c — pass-1/pass-2 argument parsing tests (R2).
 *
 * Pins the two-pass design of src/argparse.c:
 *
 *   Pass 1 (parse_globals): extracts --db, --version, --help/-h,
 *           --verbose/-v (0-3, stackable); everything else is
 *           compacted into g->argv with g->argv[0] = the action.
 *
 *   Pass 2 (cmd_args_*): per-action iteration. Known action flags:
 *           --pending (bool), --max <n>, --timeout <n>,
 *           --api_key <key>; value flags eat the next token unless it
 *           is inline (--name=value) or itself a flag; boolean flags
 *           never eat the next token; cmd_args_validate rejects
 *           unknown long options.
 *
 * No DB, no HTTP — plain asserts on the parser state.
 * Run from acta_runner/: `make test` (this suite runs first).
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "argparse.h"
#include "runner_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (cond)
        printf("  PASS %s\n", what);
    else {
        printf("  FAIL %s\n", what);
        failures++;
    }
}

/*
 * parse_globals over a literal argv. Does NOT free g->argv: the
 * caller must run its checks FIRST, then free(g.argv) (NULL-safe: it
 * is NULL on every failure path). Freeing before the checks would be
 * use-after-free (check()'s printf can reuse the freed block).
 */
static int p1(int argc, char **argv, global_opts_t *g)
{
    return parse_globals(argc, argv, g);
}

static cmd_args_t p2(char **argv, int argc)
{
    cmd_args_t it;
    cmd_args_init(&it, argc, argv);
    return it;
}

int main(void)
{
    /* ── pass 1: parse_globals ─────────────────────────────────── */
    printf("== pass 1 (parse_globals)\n");

    {
        char *av[] = { "prog", "run" };
        global_opts_t g;
        int rc = p1(2, av, &g);
        check(rc == EXIT_OK, "action only -> EXIT_OK");
        check(g.argc == 1 && g.argv && strcmp(g.argv[0], "run") == 0,
              "argv[0] = action");
        check(g.db == NULL, "db unset");
        check(g.verbose == 0 && !g.show_version && !g.show_help,
              "flags unset");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--db", "x", "run", "42" };
        global_opts_t g;
        int rc = p1(5, av, &g);
        check(rc == EXIT_OK, "--db <path> -> EXIT_OK");
        check(g.db && strcmp(g.db, "x") == 0, "--db <path> value");
        check(g.argc == 2 && strcmp(g.argv[0], "run") == 0 &&
              strcmp(g.argv[1], "42") == 0,
              "remainder compacted to [run 42]");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--db=x", "run" };
        global_opts_t g;
        int rc = p1(3, av, &g);
        check(rc == EXIT_OK, "--db=<path> -> EXIT_OK");
        check(g.db && strcmp(g.db, "x") == 0, "--db=<path> inline value");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--db" };
        global_opts_t g;
        int rc = p1(2, av, &g);
        check(rc == EXIT_CLI, "--db with missing value -> EXIT_CLI");
        check(g.argv == NULL, "nothing leaked on failure");
    }

    {
        char *av[] = { "prog", "--db", "x" };
        global_opts_t g;
        int rc = p1(3, av, &g);
        check(rc == EXIT_CLI, "globals only (no action) -> EXIT_CLI");
    }

    {
        char *av[] = { "prog" };
        global_opts_t g;
        int rc = p1(1, av, &g);
        check(rc == EXIT_CLI, "no arguments -> EXIT_CLI");
    }

    {
        char *av[] = { "prog", "-v", "-v", "run" };
        global_opts_t g;
        int rc = p1(4, av, &g);
        check(rc == EXIT_OK && g.verbose == 2, "-v -v -> verbose 2");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "-v", "-v", "-v", "-v", "run" };
        global_opts_t g;
        int rc = p1(6, av, &g);
        check(rc == EXIT_OK && g.verbose == 3,
              "-v x4 clamps at 3");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--verbose=2", "run" };
        global_opts_t g;
        int rc = p1(3, av, &g);
        check(rc == EXIT_OK && g.verbose == 2, "--verbose=2 -> 2");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--verbose=7", "run" };
        global_opts_t g;
        int rc = p1(3, av, &g);
        check(rc == EXIT_OK && g.verbose == 3,
              "--verbose=7 clamps to 3");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--verbose=abc", "run" };
        global_opts_t g;
        int rc = p1(3, av, &g);
        check(rc == EXIT_INVALID, "--verbose=abc -> EXIT_INVALID");
    }

    {
        char *av[] = { "prog", "--version" };
        global_opts_t g;
        int rc = p1(2, av, &g);
        /* The implementation returns 0 with show_version set (the
         * header's old "returns EXIT_CLI" wording is stale); this
         * pins the actual contract. */
        check(rc == EXIT_OK, "--version -> EXIT_OK");
        check(g.show_version == 1, "--version flag set");
        check(g.argc == 0 && g.argv == NULL, "no remainder after --version");
    }

    {
        char *av[] = { "prog", "-h" };
        global_opts_t g;
        int rc = p1(2, av, &g);
        check(rc == EXIT_OK && g.show_help == 1, "-h -> show_help");
    }

    {
        char *av[] = { "prog", "run", "--db", "x", "42", "--pending" };
        global_opts_t g;
        int rc = p1(6, av, &g);
        check(rc == EXIT_OK, "global mid-line -> EXIT_OK");
        check(g.db && strcmp(g.db, "x") == 0, "mid-line --db extracted");
        check(g.argc == 3 && strcmp(g.argv[0], "run") == 0 &&
              strcmp(g.argv[1], "42") == 0 &&
              strcmp(g.argv[2], "--pending") == 0,
              "remainder keeps action + action args in order");
        free(g.argv);
    }

    {
        char *av[] = { "prog", "--weird", "run" };
        global_opts_t g;
        int rc = p1(3, av, &g);
        check(rc == EXIT_OK, "unknown global passes through");
        check(g.argc == 2 && strcmp(g.argv[0], "--weird") == 0,
              "unknown token becomes the first remainder (the action)");
        free(g.argv);
    }

    /* ── pass 2: cmd_args_* ──────────────────────────────────────
     * The pass-2 view is the action's OWN args: main.c initialises it
     * with gopts.argv + 1 (argc - 1), so the action token ("run") is
     * NOT in these arrays. */
    printf("== pass 2 (cmd_args_*)\n");

    {
        char *av[] = { "42" };
        cmd_args_t it = p2(av, 1);
        const char *pos1 = cmd_args_next_positional(&it);
        const char *pos2 = cmd_args_next_positional(&it);
        check(pos1 && strcmp(pos1, "42") == 0, "next_positional -> 42");
        check(pos2 == NULL, "exhausted -> NULL");
        check(!cmd_args_has_flag(&it, "max") &&
              cmd_args_flag(&it, "max", 1) == NULL,
              "absent --max: has_flag 0, flag NULL");
    }

    {
        char *av[] = { "--pending" };
        cmd_args_t it = p2(av, 1);
        check(cmd_args_has_flag(&it, "pending"), "--pending present");
        check(cmd_args_next_positional(&it) == NULL,
              "boolean flag leaves no positional");
    }

    {
        char *av[] = { "--max", "3" };
        cmd_args_t it = p2(av, 2);
        const char *v = cmd_args_flag(&it, "max", 1);
        check(v && strcmp(v, "3") == 0, "--max <n> value");
        check(cmd_args_next_positional(&it) == NULL,
              "value token is not a positional");
    }

    {
        char *av[] = { "--max=3" };
        cmd_args_t it = p2(av, 1);
        const char *v = cmd_args_flag(&it, "max", 1);
        check(v && strcmp(v, "3") == 0, "--max=<n> inline value");
        check(cmd_args_next_positional(&it) == NULL,
              "inline flag leaves no positional");
    }

    {
        char *av[] = { "42", "--pending" };
        cmd_args_t it = p2(av, 2);
        const char *pos = cmd_args_next_positional(&it);
        check(pos && strcmp(pos, "42") == 0,
              "positional before trailing bool flag");
    }

    {
        char *av[] = { "--pending", "42" };
        cmd_args_t it = p2(av, 2);
        const char *pos = cmd_args_next_positional(&it);
        check(pos && strcmp(pos, "42") == 0,
              "bool flag does not eat the following positional");
    }

    {
        char *av[] = { "--timeout", "30", "--api_key", "k", "42" };
        cmd_args_t it = p2(av, 5);
        const char *t = cmd_args_flag(&it, "timeout", 1);
        const char *k = cmd_args_flag(&it, "api_key", 1);
        const char *pos = cmd_args_next_positional(&it);
        check(t && strcmp(t, "30") == 0, "--timeout value");
        check(k && strcmp(k, "k") == 0, "--api_key value");
        check(pos && strcmp(pos, "42") == 0,
              "positional after two value flags");
    }

    {
        char *av[] = { "--max", "--pending" };
        cmd_args_t it = p2(av, 2);
        check(cmd_args_next_positional(&it) == NULL,
              "value flag does not eat the following flag");
    }

    /* validate */
    {
        char *av[] = { "--max", "3", "--pending" };
        cmd_args_t it = p2(av, 3);
        check(cmd_args_validate(&it) == EXIT_OK,
              "known flags -> EXIT_OK");
    }

    {
        char *av[] = { "--bogus" };
        cmd_args_t it = p2(av, 1);
        check(cmd_args_validate(&it) == EXIT_INVALID,
              "unknown flag -> EXIT_INVALID");
    }

    {
        char *av[] = { "--bogus=x" };
        cmd_args_t it = p2(av, 1);
        check(cmd_args_validate(&it) == EXIT_INVALID,
              "unknown flag with inline value -> EXIT_INVALID");
    }

    {
        char *av[] = { "42" };
        cmd_args_t it = p2(av, 1);
        check(cmd_args_validate(&it) == EXIT_OK, "no flags -> EXIT_OK");
    }

    {
        char *av[] = { "--pending=x" };
        cmd_args_t it = p2(av, 1);
        check(cmd_args_validate(&it) == EXIT_OK,
              "known name with junk inline value still validates");
    }

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
