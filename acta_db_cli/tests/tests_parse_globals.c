/* tests_parse_globals.c
 *
 * Build:  cc -o test_parse tests_parse_globals.c argparse.c cli.c
 * Run:    ./test_parse
 *
 * Each case builds a fake argv[], calls parse_globals,
 * and asserts on the resulting global_opts_t fields.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cli.h"
#include "argparse.h"

/* ------------------------------------------------------------------ */
/*  tiny test harness                                                  */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

#define CHECK(cond)                                                    \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #cond);                       \
            g_fail++;                                                 \
            goto next;                                                \
        }                                                             \
    } while (0)

#define CHECK_STR(field, expected)                                    \
    do {                                                              \
        if (expected ? !field || strcmp(field, expected) : field) {   \
            fprintf(stderr, "FAIL %s:%d  %s = '%s' (want '%s')\n",  \
                    __FILE__, __LINE__, #field,                      \
                    (char *)(field ?: "(null)"),                     \
                    (char *)(expected ?: "(null)"));                 \
            g_fail++;                                                 \
            goto next;                                                \
        }                                                             \
    } while (0)

/* ------------------------------------------------------------------ */
/*  helpers                                                           */
/* ------------------------------------------------------------------ */

/* Build a minimal argv array.  caller provides tokens, we prepend
 * the program name so that parse_globals sees argc >= 1. */
static int mk_argv(char ***out_argv, int *out_argc, ...)
{
    /* count */
    va_list ap;
    va_start(ap);
    int n = 0;
    while (va_arg(ap, const char *)) n++;
    va_end(ap);

    /* allocate: progname + n tokens + sentinel */
    char **argv = malloc((n + 2) * sizeof(char *));
    assert(argv);
    argv[0] = "actagamma_db";

    va_start(ap);
    for (int i = 0; i < n; i++) argv[i + 1] = va_arg(ap, const char *);
    va_end(ap);
    argv[n + 1] = NULL;

    *out_argv = argv;
    *out_argc = n + 1;
    return n + 1;
}

/* ------------------------------------------------------------------ */
/*  tests                                                             */
/* ------------------------------------------------------------------ */

static void test_basic_passthrough(void)
{
    /* actagamma_db model list */
    char **argv; int argc;
    mk_argv(&argv, &argc, "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.argc == 2);
    CHECK_STR(g.argv[0], "model");
    CHECK_STR(g.argv[1], "list");
    CHECK(g.db == NULL);
    CHECK(g.verbose == 0);
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_db_space_form(void)
{
    /* actagamma_db --db /tmp/a.db model list */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--db", "/tmp/a.db", "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK_STR(g.db, "/tmp/a.db");
    CHECK(g.argc == 2);
    CHECK_STR(g.argv[0], "model");
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_db_equals_form(void)
{
    /* actagamma_db --db=/tmp/b.db model list */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--db=/tmp/b.db", "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK_STR(g.db, "/tmp/b.db");
    CHECK(g.argc == 2);
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_boolean_flags(void)
{
    /* actagamma_db --no-nulls --id-only --count --table --pretty model list */
    char **argv; int argc;
    mk_argv(&argv, &argc,
            "--no-nulls", "--id-only", "--count", "--table", "--pretty",
            "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.no_nulls == 1);
    CHECK(g.id_only == 1);
    CHECK(g.count == 1);
    CHECK(g.table == 1);
    CHECK(g.pretty == 1);
    CHECK(g.argc == 2);
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_verbose_stacking(void)
{
    /* actagamma_db -v -v -v model list  →  verbose == 3 */
    char **argv; int argc;
    mk_argv(&argv, &argc, "-v", "-v", "-v", "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.verbose == 3);
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_verbose_explicit(void)
{
    /* actagamma_db --verbose=2 model list */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--verbose=2", "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.verbose == 2);
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_json_equals(void)
{
    /* actagamma_db --json={"a":1} model create */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--json={\"a\":1}", "model", "create");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK_STR(g.json_input, "{\"a\":1}");
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_from_file_space(void)
{
    /* actagamma_db --from-file ./spec.json model create */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--from-file", "./spec.json", "model", "create");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK_STR(g.from_file, "./spec.json");
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_stdin_flag(void)
{
    /* actagamma_db --stdin model create */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--stdin", "model", "create");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.from_stdin == 1);
    free(g.argv);
next:
    (void)argv; (void)argc;
}

static void test_early_exit_version(void)
{
    /* actagamma_db --version */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--version");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.show_version == 1);
    CHECK(g.argc == 0);
    CHECK(g.argv == NULL);
next:
    (void)argv; (void)argc;
}

static void test_early_exit_help_short(void)
{
    /* actagamma_db -h */
    char **argv; int argc;
    mk_argv(&argv, &argc, "-h");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.show_help == 1);
    CHECK(g.argc == 0);
next:
    (void)argv; (void)argc;
}

static void test_early_exit_tools(void)
{
    /* actagamma_db --tools */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--tools");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK(g.show_tools == 1);
    CHECK(g.argc == 0);
next:
    (void)argv; (void)argc;
}

static void test_error_missing_value(void)
{
    /* actagamma_db --db model list   ← --db eats "model" as path,
     * leaving only "list" → argc < 2 → EXIT_CLI */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--db", "model", "list");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    /* "model" consumed as db value, "list" is sole positional → fail */
    CHECK(rc == EXIT_CLI);
next:
    (void)argv; (void)argc;
}

static void test_error_missing_value_at_end(void)
{
    /* actagamma_db model list --db   ← dangling --db with no value */
    char **argv; int argc;
    mk_argv(&argv, &argc, "model", "list", "--db");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == EXIT_CLI);
next:
    (void)argv; (void)argc;
}

static void test_error_no_entity(void)
{
    /* actagamma_db --no-nulls   ← only flags, no entity+action */
    char **argv; int argc;
    mk_argv(&argv, &argc, "--no-nulls");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == EXIT_CLI);
next:
    (void)argv; (void)argc;
}

static void test_mixed_interleaved(void)
{
    /* actagamma_db model --fields id,name --pretty --db=local list --no-nulls */
    /*
     * Globals consumed: --fields, --pretty, --db, --no-nulls
     * Positionals:     model, list
     */
    char **argv; int argc;
    mk_argv(&argv, &argc,
            "model", "--fields", "id,name", "--pretty",
            "--db=local", "list", "--no-nulls");

    global_opts_t g;
    int rc = parse_globals(argc, argv, &g);

    CHECK(rc == 0);
    CHECK_STR(g.db, "local");
    CHECK_STR(g.fields, "id,name");
    CHECK(g.pretty == 1);
    CHECK(g.no_nulls == 1);
    CHECK(g.argc == 2);
    CHECK_STR(g.argv[0], "model");
    CHECK_STR(g.argv[1], "list");
    free(g.argv);
next:
    (void)argv; (void)argc;
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        {"basic_passthrough",    test_basic_passthrough    },
        {"db_space",             test_db_space_form        },
        {"db_equals",            test_db_equals_form       },
        {"boolean_flags",        test_boolean_flags        },
        {"verbose_stacking",     test_verbose_stacking     },
        {"verbose_explicit",     test_verbose_explicit     },
        {"json_equals",          test_json_equals          },
        {"from_file_space",      test_from_file_space      },
        {"stdin_flag",           test_stdin_flag           },
        {"early_version",        test_early_exit_version   },
        {"early_help",           test_early_exit_help_short},
        {"early_tools",          test_early_exit_tools     },
        {"err_missing_val",      test_error_missing_value  },
        {"err_missing_val_end",  test_error_missing_value_at_end},
        {"err_no_entity",        test_error_no_entity      },
        {"mixed_interleaved",    test_mixed_interleaved    },
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        int before = g_pass + g_fail;
        tests[i].fn();
        if (g_pass + g_fail == before) {
            /* no CHECK fired → all passed */
            g_pass++;
            printf("  PASS  %s\n", tests[i].name);
        } else {
            printf("  FAIL  %s\n", tests[i].name);
        }
    }

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
